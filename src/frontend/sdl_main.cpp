        if(mainBoard || main8088Enabled) {
            if(config.dipSerialMode==MachineConfig::SerialMode::Sync &&
               config.serial0!="pty" && config.serial0!="none")
                throw std::invalid_argument(
                    "DIPSW.serial_mode=sync is unsupported on physical termios ports");
            serial=std::make_unique<SerialBackend>(
                config.serial0,config.dipSerialBaudSwitches,false,
                config.serialRtsCts,
                config.dipSerialCd==MachineConfig::SerialCd::CdAndDsr);
            if(serial->enabled())
                std::cout<<(serial->physical()?"Serial device: ":"Serial PTY: ")
                         <<serial->label()<<"\n"<<std::flush;
        }
        std::mutex cpuMutex;
        auto serviceSerial=[&]() {
            if(!serial)return;
            serial->synchronize(*machine.systemBoard().usartDevice());
            if(mainBoard)mainBoard->syncSerialIrq();
            serial->service(
                [&](unsigned char& ch) {
                    BYTE value=0;
                    if(!machine.systemBoard().takeSerialOutput(value))return false;
                    ch=value;return true;
                },
                [&](unsigned char ch){machine.systemBoard().queueSerialInput(ch);});
        };
        // Trace real shared-RAM bus transactions, not guessed values from
        // periodic snapshots. Limit completed requests to keep logs bounded.
        if(traceInkey && mainBoard) {
            auto active=std::make_shared<bool>(false);
            auto count=std::make_shared<unsigned>(0);
            machine.setDisplaySharedTraceObserver(
                [active,count](DisplaySubsystem::SharedTrace e) {
                    if(e.mainCpu && e.write && e.offset==2 && e.value==0x2d) {
                        if(*count>=64)return;
                        *active=true;
                        ++*count;
                        std::cerr<<"INKEY2 #"<<*count<<" request FC82=2D\n";
                    }
                    if(!*active)return;
                    if(e.offset==3 || e.offset==4) {
                        std::cerr<<"INKEY2 #"<<*count<<" "<<(e.mainCpu?"MAIN":"SUB")
                                 <<(e.write?" W":" R")<<" FC8"<<unsigned(e.offset)
                                 <<"=$"<<std::uppercase<<std::hex
                                 <<std::setw(2)<<std::setfill('0')<<unsigned(e.value)
                                 <<std::dec<<std::setfill(' ')<<"\n";
                        if(e.mainCpu && !e.write && e.offset==4)*active=false;
                    }
                });
        }
        // In the opt-in trap mode, use a separate host queue for INKEY 2.
        // The real sub-ROM keyboard path is left untouched otherwise.
        SDL_StartTextInput();
        bool running=true,paused=false;
        std::uint64_t executed=0; // owned by the CPU worker
        std::atomic<std::uint64_t> publishedExecuted{0}; // SDL diagnostics
        std::uint64_t lastReported=0;
        std::uint64_t ioWrites=0;
        if(diagnostics)
            machine.setDisplayIoWriteObserver([&](DisplaySubsystem::IoWriteTrace){++ioWrites;});
        auto lastLog=std::chrono::steady_clock::now();
        auto lastKanjiLog=std::chrono::steady_clock::now();
        // Match the original emulator's approximately 32 ms paint cadence.
        auto lastFrame=lastLog-std::chrono::milliseconds(32);
        CpuSpeedMeter cpuMeter;
        WorkerGroup cpuWorker;
        std::atomic<unsigned> cpuResumeGeneration{0};
        auto lastSpeedReport=std::chrono::steady_clock::now();
        bool wasPaused=false;
        CpuBoard* lastCpuOwner=machine.activeBoard();
        // ROM milestones from the original FM-11 8088 boot ROM.
        // Emit only the first visit to each stage, plus bounded early
        // instructions and occasional progress checkpoints.
        constexpr std::array<unsigned,10> romBootOffsets{
            0x00c5,0x011b,0x0123,0x014e,0x01be,
            0x01f1,0x0291,0x0385,0x042f,0x045b
        };
        std::array<bool,romBootOffsets.size()> romBootSeen{};
        bool diskIplSeen=false, cbiosSeen=false;
        auto traceCpmSnapshot=[&](const char* event) {
            if(!traceCpm80 || !mainBoard)return;
            const auto* owner=machine.activeBoard();
            // No CPU-visible I/O reads: several FDxx reads acknowledge IRQs.
            std::cerr<<"CPM80 "<<event
                     <<" step="<<executed
                     <<" owner="<<(owner && owner->cpu().type()==CpuType::Z80?"Z80":"6809")
                     <<" pc=$"<<std::hex<<(owner?owner->cpu().pc():0)
                     <<" pc6809=$"<<mainBoard->cpu().pc()
                     <<" FD05write=$"<<unsigned(machine.mainSubCpuControl())
                     <<" FDCtrk=$"<<unsigned(mainBoard->fdcTrack())
                     <<" FDCsec=$"<<unsigned(mainBoard->fdcSector())
                     <<" FDCstatus=$"<<unsigned(mainBoard->fdcStatus())
                     <<" phys10000=$"<<unsigned(mainBoard->readPhysical(0x10000))
                     <<" ccpbase=$"<<unsigned(mainBoard->readPhysical(0xdc00))
                     <<" biosbase=$"<<unsigned(mainBoard->readPhysical(0xf200))
                     <<std::dec
                     <<" fdcReads="<<mainBoard->fdcReadCommands()
                     <<" fdcOK="<<mainBoard->fdcSuccessfulReads()
                     <<" fdcFailed="<<mainBoard->fdcFailedReads()
                     <<" hdReads="<<mainBoard->hdReadAttempts()
                     <<" hdFailed="<<mainBoard->hdReadFailures()
                     <<"\n";
        };
        if(traceCpm80)traceCpmSnapshot("reset");

        // Independent hardware clocks: the PTM and sub-CPU workers are
        // unrelated to SDL rendering and the main CPU instruction pacing.
        std::atomic<DebugMonitor*> monitorProbe{nullptr};
        if(monitorEnabled)machine.setDisplayBreakProbe([&](std::uint32_t pc) {
            auto* mon=monitorProbe.load(std::memory_order_acquire);
            return !mon || !mon->needsProbe() ||
                   !mon->probe(DebugMonitor::Target::Sub,pc);
        });
        if(machine.boardCount())machine.startRealtimePtm();
        machine.startDisplayCpu();

        if(machine.boardCount())cpuWorker.start(
            [&,pacer=SdlCpuPacer(speed),
               lastTick=std::chrono::steady_clock::now(),
               generation=0u]() mutable {
                const auto now=std::chrono::steady_clock::now();
                const auto requested=cpuResumeGeneration.load(std::memory_order_acquire);
                if(generation!=requested) {
                    generation=requested;
                    lastTick=now;
                    pacer.reset();
                }
                const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(
                    now-lastTick);
                lastTick=now;
                pacer.addElapsed(elapsed);
                // Drain PTY input before executing the next CPU batch.
                // Running serial I/O here avoids SDL waiting to acquire
                // cpuMutex while the CPU continually executes instructions.
                serviceSerial();
                unsigned ran=0;
                {
                    // One bounded batch is the main CPU's ownership boundary.
                    // No SDL renderer ever executes guest instructions.
                    std::unique_lock<std::mutex> cpuLock(cpuMutex,std::defer_lock);
                    // SDL only reads CPU state in explicit diagnostic modes.
                    if(diagnostics || cpuSpeed)cpuLock.lock();
                    for(;ran<std::min<std::uint64_t>(instructions,512) && pacer.ready();++ran) {
                        const auto* active=machine.activeBoard();
                        if(!active)break;
                        if(auto* mon=monitorProbe.load(std::memory_order_acquire)) {
                            if(mon->needsProbe() &&
                               mon->probe(DebugMonitor::cpuTarget(active->cpu().type()),
                                          active->cpu().pc()))
                                break;
                        }
                        const bool ptmIrqSyncedBefore=
                            mainBoard && active==static_cast<CpuBoard*>(mainBoard);
                        if(ptmIrqSyncedBefore) {
                            mainBoard->syncPtmIrq();
                            const auto wait=mainBoard->cpuWaitState();
                            if(wait!=CSim09::WaitState::Running) {
                                const auto& regs=dynamic_cast<CSim09&>(mainBoard->cpu()).regs;
                                const bool canWake=mainBoard->irqPending() &&
                                    (wait==CSim09::WaitState::Sync ||
                                     (regs.CC&0x10)==0);
                                if(!canWake) {
                                    pacer.reset();
                                    break;
                                }
                            }
                        }
                        const auto activeType=machine.activeCpuType();
                        const auto hz=activeType==CpuType::Z80
                            ?machine.clockZ80Hz()
                            :(activeType==CpuType::I8088 || activeType==CpuType::NECV20)
                                ?machine.clock8088Hz()
                                :mainBoard?mainBoard->cpuClockHz():config.main6809ClockHz;
                        if(traceBoot &&
                           (activeType==CpuType::I8088 || activeType==CpuType::NECV20)) {
                            auto* board8088=dynamic_cast<Main8088Board*>(machine.activeBoard());
                            if(board8088) {
                                const auto& cpu=static_cast<const Main8088Board::Core&>(board8088->cpu());
                                const auto cs=cpu.cs(), ip=cpu.ip();
                                bool milestone=false;
                                if(cs==0xff00) {
                                    for(std::size_t n=0;n<romBootOffsets.size();++n) {
                                        if(ip==romBootOffsets[n] && !romBootSeen[n]) {
                                            romBootSeen[n]=true;
                                            milestone=true;
                                        }
                                    }
                                }
                                const bool diskIpl=(cs==0x07c0 && ip==0);
                                const bool cbios=(cs==0x0400 && ip==0x2500);
                                const bool handoff=(diskIpl && !diskIplSeen) ||
                                                   (cbios && !cbiosSeen);
                                if(diskIpl)diskIplSeen=true;
                                if(cbios)cbiosSeen=true;
                                if(executed<96 || milestone || handoff ||
                                   (executed && executed%200000==0)) {
                                    std::cerr<<"8088 boot step="<<executed
                                             <<" CS:IP="<<std::hex<<std::setw(4)
                                             <<std::setfill('0')<<cs<<":"
                                             <<std::setw(4)<<ip
                                             <<" AX="<<std::setw(4)<<cpu.reg(0)
                                             <<" BX="<<std::setw(4)<<cpu.reg(3)
                                             <<" SP="<<std::setw(4)<<cpu.reg(4)
                                             <<" DS="<<std::setw(4)<<cpu.segment(3)
                                             <<" SS="<<std::setw(4)<<cpu.segment(2)
                                             <<" FLAGS="<<std::setw(4)<<cpu.flags()
                                             <<std::dec<<std::setfill(' ')<<"\n";
                                }
                            }
                        }
                        if(traceBoot && executed<64 && mainBoard) {
                            std::cerr<<"main step="<<executed
                                     <<" PC=$"<<std::hex
                                     <<std::setw(4)<<std::setfill('0')
                                     <<mainBoard->cpu().pc()
                                     <<" subCtrl=$"<<std::setw(2)
                                     <<unsigned(machine.mainSubCpuControl())
                                     <<std::dec<<std::setfill(' ')<<"\n";
                        }
                        machine.executeOne(ptmIrqSyncedBefore);
                        const auto guestCycles=machine.lastExecutionCycles();
                        pacer.consume(guestCycles,hz);
                        if(cpuSpeed) {
                            const auto card=activeType==CpuType::Z80
                                ?CpuSpeedMeter::Card::Z80
                                :(activeType==CpuType::I8088 || activeType==CpuType::NECV20)
                                    ?CpuSpeedMeter::Card::Main8088
                                    :CpuSpeedMeter::Card::Main6809;
                            cpuMeter.record(card,guestCycles);
                        }
                        const auto step=++executed;
                        if(traceCpm80) {
                            CpuBoard* owner=machine.activeBoard();
                            if(owner!=lastCpuOwner) {
                                traceCpmSnapshot("handoff");
                                lastCpuOwner=owner;
                            } else if(step%200000==0)
                                traceCpmSnapshot("progress");
                        }
                    }
                    publishedExecuted.store(executed,std::memory_order_release);
                }
                // Flush guest output before yielding.
                serviceSerial();
                // PTM and display CPU continue on their independent workers.
                if(ran==0 || !pacer.ready())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });

        // Command execution is serialized on the SDL/UI thread.
        auto applyPauseState=[&]() {
            if(paused==wasPaused)return;
            if(paused) {
                cpuWorker.pause();
                machine.pauseRealtimePtm();
                machine.pauseDisplayCpu();
            } else {
                machine.resumeDisplayCpu();
                machine.resumeRealtimePtm();
                cpuResumeGeneration.fetch_add(1,std::memory_order_release);
                cpuWorker.resume();
            }
            if(cpuSpeed) {
                {std::lock_guard<std::mutex> cpuLock(cpuMutex);cpuMeter.reset();}
                lastSpeedReport=std::chrono::steady_clock::now();
            }
            wasPaused=paused;
        };
        std::unique_ptr<DebugMonitor> monitor;
        std::unique_ptr<MonitorInput> monitorInput;
        if(monitorEnabled) {
            // Install memory access hooks only while every guest worker is
            // paused; std::function replacement is not thread-safe.
            const bool startupPaused=paused;
            paused=true;
            applyPauseState();
            monitor=std::make_unique<DebugMonitor>(machine,[&](bool stop) {
                paused=stop;applyPauseState();
            });
            monitorProbe.store(monitor.get(),std::memory_order_release);
            monitorInput=std::make_unique<MonitorInput>();
            monitor->emit("FM-11 Debug Monitor (Stage 3-C). Type help.\n"+monitor->prompt());
            paused=startupPaused;
            applyPauseState();
        }
        while(running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
#ifdef FM11_SETTINGS_GUI
                if(filePickerGui && filePickerGui->processEvent(event))continue;
                if(settingsGui && settingsGui->processEvent(event))continue;
                if(mainMenuGui && mainMenuGui->processEvent(event))continue;
                if(event.type==SDL_KEYDOWN && !event.key.repeat &&
                   event.key.keysym.sym==SDLK_F10 &&
                   (event.key.keysym.mod&KMOD_CTRL)) {
                    settingsGui->toggle();
                    continue;
                }
#endif
                if(event.type==SDL_QUIT)running=false;
#ifdef FM11_SETTINGS_GUI
                // With two SDL windows, closing the CRT does not necessarily
                // generate SDL_QUIT. Closing Settings only hides that window.
                if(event.type==SDL_WINDOWEVENT &&
                   event.window.event==SDL_WINDOWEVENT_CLOSE &&
                   event.window.windowID==SDL_GetWindowID(ui.window))running=false;
#endif
                if(event.type==SDL_TEXTINPUT && !paused && !testPattern && !testKanji) {
                    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(event.text.text);
                        *p;++p) {
                        // The original keyboard queue carries 8-bit codes;
                        // UTF-8 multibyte input needs a separate keymap.
                        if(*p>=0x20 && *p<0x7f) {
                            machine.enqueueDisplayKeyCode(*p);
                        }
                    }
                }
                if(event.type==SDL_KEYDOWN && !event.key.repeat) {
                    const auto sym=event.key.keysym.sym;
                    const auto mod=event.key.keysym.mod;
                    if(testKanji && sym==SDLK_F3) {
                        if(machine.displaySubsystem().kanji2Installed()) {
                            kanjiLevel=3-kanjiLevel;
                            kanjiPage=0;
                            KanjiTestPattern::paint(
                                machine.systemBoard().displaySubsystem(),kanjiPage,kanjiLevel);
                            const std::string title="FM-11 Kanji level "+
                                std::to_string(kanjiLevel)+" page 1/"+
                                std::to_string(KanjiTestPattern::pages);
                            SDL_SetWindowTitle(ui.window,title.c_str());
                        }
                    } else if(testKanji && (sym==SDLK_PAGEDOWN || sym==SDLK_PAGEUP)) {
                        const unsigned next=sym==SDLK_PAGEDOWN
                            ?(kanjiPage+1)%KanjiTestPattern::pages
                            :(kanjiPage+KanjiTestPattern::pages-1)%KanjiTestPattern::pages;
                        if(next!=kanjiPage) {
                            kanjiPage=next;
                            KanjiTestPattern::paint(
                                machine.systemBoard().displaySubsystem(),kanjiPage,kanjiLevel);
                            const unsigned first=kanjiPage*KanjiTestPattern::glyphsPerPage;
                            const unsigned last=std::min(
                                first+KanjiTestPattern::glyphsPerPage,
                                KanjiTestPattern::glyphCount)-1;
                            std::ostringstream title;
                            title<<"FM-11 Kanji level "<<kanjiLevel
                                 <<" page "<<(kanjiPage+1)
                                 <<"/"<<KanjiTestPattern::pages
                                 <<" (glyph "<<std::hex<<std::uppercase
                                 <<first<<"-"<<last<<")";
                            SDL_SetWindowTitle(ui.window,title.str().c_str());
                        }
                    } else if(sym==SDLK_F2 && !testKanji)paused=!paused;
                    else if(sym==SDLK_F1 && !testKanji) {
                        high=!high;
                        ui.setMode(high);
                    } else if(sym==SDLK_F12 && (mod&KMOD_CTRL)) {
                        running=false;
                    } else if(!paused && !testPattern && !testKanji) {
                        // Physical/special keys come from SDL_KEYDOWN; printable
                        // characters (including Space) come from SDL_TEXTINPUT.
                        // Non-ASCII FM-11 codes match the original KeyCode.h.
                        std::uint16_t code=0;
                        if(sym==SDLK_RETURN || sym==SDLK_KP_ENTER)code=0x000d;
                        else if(sym==SDLK_BACKSPACE)code=0x0008;
                        else if(sym==SDLK_TAB)code=0x0009;
                        else if(sym==SDLK_ESCAPE)code=0x001b;
                        else if(sym==SDLK_INSERT)code=0x0110;
                        else if(sym==SDLK_DELETE)code=0x0114;
                        else if(sym==SDLK_HOME)code=0x0115;
                        else if(sym==SDLK_UP)code=0x0116;
                        else if(sym==SDLK_DOWN)code=0x0117;
                        else if(sym==SDLK_LEFT)code=0x0118;
                        else if(sym==SDLK_RIGHT)code=0x0119;
                        else if(sym==SDLK_PAGEUP)code=0x0136;
                        else if(sym==SDLK_PAGEDOWN)code=0x0137;
                        else if(sym>=SDLK_F3 && sym<=SDLK_F10)
                            code=static_cast<std::uint16_t>(0x0100+sym-SDLK_F1);
                        else if(sym==SDLK_F11)code=0x0121;
                        else if(sym==SDLK_F12)code=0x0122;
                        else if((mod&KMOD_CTRL) && sym>=SDLK_a && sym<=SDLK_z)
                            code=static_cast<std::uint16_t>(sym-SDLK_a+1);
                        else if((mod&KMOD_CTRL) && sym==SDLK_LEFTBRACKET)code=0x001b;
                        else if((mod&KMOD_CTRL) && sym==SDLK_BACKSLASH)code=0x001c;
                        else if((mod&KMOD_CTRL) && sym==SDLK_RIGHTBRACKET)code=0x001d;
                        if(code) {
                            machine.enqueueDisplayKeyCode(code);
                        }
                    }
                }
            }
            if(monitor && monitor->breakPending()) {
                paused=true;
                applyPauseState();
                monitor->emit(monitor->takeBreak());
                monitor->emit(monitor->prompt());
            }
            applyPauseState();
#ifdef FM11_SETTINGS_GUI
            if(const auto request=settingsGui->takeFddRequest()) {
                const bool restorePaused=paused;
                paused=true;
                applyPauseState(); // CPU, display CPU and PTM are quiescent.
                std::string error;
                try {
                    if(!liveFloppies)
                        throw std::runtime_error("no FDD controller has been initialized");
                    liveFloppies->exchange(request->channel,request->drive,request->path);
                    std::cerr<<"Stage 4-B "
                             <<(request->path.empty()?"Ejected ":"Mounted ")
                             <<(request->channel?"2HD":"2D")<<" FDD"
                             <<request->drive<<": "<<request->path<<"\n";
                } catch(const std::exception& e) {
                    error=e.what();
                }
                settingsGui->finishFddRequest(*request,error);
                paused=restorePaused;
                applyPauseState();
            }
#endif
            if(monitorInput) {
                std::string line;
                while(monitorInput->pop(line)) {
                    monitor->recordInput(line);
                    monitor->emit(monitor->execute(line,paused));
                    monitor->emit(monitor->prompt());
                }
            }
            // CPU, PTM and display execution are independently scheduled.
            // The SDL thread only services host events and paints frames.
            if(!paused && !testPattern && !testKanji && !machine.boardCount())
                SDL_Delay(1);
            cpuWorker.rethrowWorkerFailure();
            if(cpuSpeed && !paused && machine.boardCount()) {
                const auto reportNow=std::chrono::steady_clock::now();
                const auto reportElapsed=reportNow-lastSpeedReport;
                if(reportElapsed>=std::chrono::seconds(1)) {
                    CpuSpeedMeter::Sample sample;
                    {std::lock_guard<std::mutex> cpuLock(cpuMutex);
                     sample=cpuMeter.take(reportElapsed);}
                    const auto hz6809=mainBoard?mainBoard->cpuClockHz():0;
                    const auto hzZ80=z80Enabled?machine.clockZ80Hz():0;
                    const auto hz8088=main8088Enabled?machine.clock8088Hz():0;
                    std::cerr<<std::fixed<<std::setprecision(3)
                             <<"CPU speed wall="<<sample.wallSeconds<<"s"
                             <<" 6809="<<sample.main6809.cycles<<" T"
                             <<" ("<<sample.mhz(CpuSpeedMeter::Card::Main6809)
                             <<" MHz, "<<sample.main6809.instructions<<" ins)"
                             ;
                    if(z80Enabled)
                        std::cerr<<" Z80="<<sample.z80.cycles<<" T"
                                 <<" ("<<sample.mhz(CpuSpeedMeter::Card::Z80)
                                 <<" MHz, "<<sample.z80.instructions<<" ins)";
                    if(main8088Enabled)
                        std::cerr<<" 8088/V20="<<sample.main8088.cycles<<" T"
                                 <<" ("<<sample.mhz(CpuSpeedMeter::Card::Main8088)
                                 <<" MHz, "<<sample.main8088.instructions<<" ins)";
                    std::cerr<<" target=";
                    if(speed==0)std::cerr<<"unlimited";
                    else std::cerr<<speed<<"x";
                    std::cerr<<" guest/wall="
                             <<sample.progressRatio(hz6809,hzZ80,hz8088)<<"x"
                             <<" [main CPU only; SYNC/CWAI lowers ratio]\n";
                    lastSpeedReport=reportNow;
                }
            }
            const auto frameTime=std::chrono::steady_clock::now();
            if(frameTime-lastFrame>=std::chrono::milliseconds(32)) {
                lastFrame=frameTime;
                const unsigned cursorTick=static_cast<unsigned>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        frameTime.time_since_epoch()).count()/32);
            // Sample main state before taking the display lock. The
            // CPU worker uses CPU -> display lock order for mailbox access.
            int sampledMainPc=-1;
            if(diagnostics) {
                std::lock_guard<std::mutex> cpuLock(cpuMutex);
                if(mainBoard)sampledMainPc=static_cast<int>(mainBoard->cpu().pc());
            }
            if(ui.glRenderer) {
                // Only the snapshot copy is serialized with the sub-CPU.
                // All OpenGL uploads, uniforms, drawing and swap occur after
                // releasing the display-bus lock.
#ifdef FM11_SETTINGS_GUI
                // The settings SDL_Renderer may switch the current OpenGL
                // context. Restore the guest renderer's context explicitly.
                if(SDL_GL_MakeCurrent(ui.window,ui.glContext)!=0)
                    throw std::runtime_error(SDL_GL_GetCurrentContext()?
                        "cannot restore guest OpenGL context":SDL_GetError());
#endif
                const auto snapshot=OpenGlRenderer::capture(machine,!ankPath.empty() && !testKanji);
                int outputW=0,outputH=0;
                SDL_GL_GetDrawableSize(ui.window,&outputW,&outputH);
#ifdef FM11_SETTINGS_GUI
                int menuPixels=MainMenuGui::menuHeight;
                int windowW=0,windowH=0;
                SDL_GetWindowSize(ui.window,&windowW,&windowH);
                menuPixels=windowH>0?MainMenuGui::menuHeight*outputH/windowH:
                    MainMenuGui::menuHeight;
#endif
                ui.glRenderer->render(snapshot,high,testPattern,!ankPath.empty() && !testKanji,
                                       outputW,outputH,cursorTick,scanlines,
#ifdef FM11_SETTINGS_GUI
                                       menuPixels
#else
                                       0
#endif
                                       );
#ifdef FM11_SETTINGS_GUI
                mainMenuGui->render(*settingsGui,paused,running);
#endif
                SDL_GL_SwapWindow(ui.window);
            } else {
                DisplayBitmap::Frame frame{};
                {
                auto displayGuard=machine.lockDisplay();
                frame=machine.captureDisplayFrame(high);
                // Original SubCpu::OnDraw derives both layers from FD A0.
                // In particular, loading a font does NOT enable text by itself.
                if(machine.boardCount()) {
                    const BYTE control=machine.mainDisplayControl();
                    if((control&0x0c)==0) {
                        std::fill(frame.argb.begin(),frame.argb.end(),0xff000000u);
                    } else {
                        // Original PalTab is a lookup of eight four-bit indices.
                        for(auto& pixel:frame.argb) {
                            unsigned index=0;
                            if(pixel&0x00ff0000u)index|=1;
                            if(pixel&0x0000ff00u)index|=2;
                            if(pixel&0x000000ffu)index|=4;
                            pixel=DisplayBitmap::referenceColor(
                                machine.mainDisplayPalette(index));
                        }
                    }
                    if((control&0x03)!=0 && !ankPath.empty())
                        DisplayBitmap::compositeText(frame,machine.displaySubsystem());
                } else if(!ankPath.empty() && !testKanji) {
                    DisplayBitmap::compositeText(frame,machine.displaySubsystem());
                }
                if(!testPattern && !testKanji && !ankPath.empty() &&
                   (!machine.boardCount() || (machine.mainDisplayControl()&0x03)!=0))
                    DisplayBitmap::compositeCursor(frame,machine.displaySubsystem(),cursorTick);
                if(testPattern) {
                    for(std::size_t y=0;y<frame.height;++y)
                        for(std::size_t x=0;x<frame.width;++x)
                            frame.argb[y*frame.width+x]=DisplayBitmap::colorArgb(
                                static_cast<unsigned>((x/80+y/50)%8));
                }
                const auto now=std::chrono::steady_clock::now();
                if(diagnostics && now-lastLog>=std::chrono::seconds(2)) {
                    std::cerr<<"display PC="<<std::hex<<machine.displayCpuPc()
                             <<std::dec<<" steps="<<publishedExecuted.load()
                             <<" delta="<<(publishedExecuted.load()-lastReported)
                             <<" ioWrites="<<ioWrites
                             <<" VRAM_nonzero="<<machine.displayNonzeroVramBytes()
                             <<" page="<<machine.displayPage()
                             <<" revision="<<machine.displayRevision()
                             <<" mainPC="<<sampledMainPc
                             <<" FDA0="<<unsigned(machine.mainDisplayControl())
                             <<" FD05="<<unsigned(machine.mainSubCpuControl())
                             <<" FD04_latch="<<unsigned(machine.mainEventStatus())
                             <<" BUSY="<<unsigned(machine.displayBusyStatus())
                             <<" sharedWrites="<<machine.displaySharedWriteRevision()
                             <<" shared0="<<unsigned(machine.displaySharedByte(0))
                             <<" halted="<<machine.displayHalted()
                             <<(machine.boardCount()?" main6809":" display-only")
                             <<(testPattern?" test-pattern":"")<<"\n";
                    lastReported=publishedExecuted.load();
                    lastLog=now;
                }
                } // release display bus before SDL texture upload/draw
                if(SDL_UpdateTexture(ui.texture,nullptr,frame.argb.data(),
                                     640*static_cast<int>(sizeof(std::uint32_t)))!=0)
                    throw std::runtime_error(SDL_GetError());
                // Integer-only pixel scaling: avoid uneven 1/2-pixel stripes
                // when a 640-wide frame is stretched to a 960-wide window.
                // For windows smaller than the native image, use a centered
                // clipped 1:1 image instead of fractional downscaling.
                int outputW=0,outputH=0;
                if(SDL_GetRendererOutputSize(ui.renderer,&outputW,&outputH)!=0)
                    throw std::runtime_error(SDL_GetError());
                const int nativeW=static_cast<int>(frame.width);
#ifdef FM11_SETTINGS_GUI
                const int menuPixels=MainMenuGui::menuHeight;
#else
                const int menuPixels=0;
#endif
                const int availableHeight=std::max(0,outputH-menuPixels);
                const int scale=std::max(1,std::min(outputW/nativeW,availableHeight/400));
                const SDL_Rect destination{
                    (outputW-nativeW*scale)/2,
                    menuPixels+(availableHeight-400*scale)/2,
                    nativeW*scale,400*scale
                };
                // The emulator image stays untouched. Paint the letterbox
                // margins dark gray, and draw a one-pixel border OUTSIDE
                // the screen rectangle whenever those pixels are visible.
                if(SDL_SetRenderDrawColor(ui.renderer,48,52,56,255)!=0)
                    throw std::runtime_error(SDL_GetError());
                if(SDL_RenderClear(ui.renderer)!=0)
                    throw std::runtime_error(SDL_GetError());
                if(SDL_RenderCopy(ui.renderer,ui.texture,nullptr,&destination)!=0)
                    throw std::runtime_error(SDL_GetError());
                // Optional CRT scanline mask on alternate doubled rows.
                if(!high && scanlines) {
                    SDL_SetRenderDrawBlendMode(ui.renderer,SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ui.renderer,0,0,0,128);
                    for(int line=0;line<200;++line) {
                        const SDL_Rect stripe{destination.x,
                            destination.y+(line*2+1)*scale,
                            destination.w,scale};
                        SDL_RenderFillRect(ui.renderer,&stripe);
                    }
                    SDL_SetRenderDrawBlendMode(ui.renderer,SDL_BLENDMODE_NONE);
                }
                const SDL_Rect border{
                    destination.x-1,destination.y-1,
                    destination.w+2,destination.h+2
                };
                if(SDL_SetRenderDrawColor(ui.renderer,144,148,152,255)!=0)
                    throw std::runtime_error(SDL_GetError());
                if(SDL_RenderDrawRect(ui.renderer,&border)!=0)
                    throw std::runtime_error(SDL_GetError());
#ifdef FM11_SETTINGS_GUI
                mainMenuGui->render(*settingsGui,paused,running);
#endif
                SDL_RenderPresent(ui.renderer);
                }
            }
            // The two renderers consume the same sub-CPU-written graphics
            // VRAM; report guest Kanji reads regardless of renderer mode.
            if(traceKanji) {
                const auto current=std::chrono::steady_clock::now();
                if(current-lastKanjiLog>=std::chrono::seconds(2)) {
                    const auto state=machine.displaySubsystem().kanjiStatistics();
                    std::cerr<<"KANJI selections="<<state.selections
                             <<" left="<<state.leftReads
                             <<" right="<<state.rightReads
                             <<" videoRevision="<<machine.displayRevision()
                             <<"\n";
                    lastKanjiLog=current;
                }
            }
            // The main CPU no longer depends on SDL paint cadence. Yield
            // host time to the three emulation workers between UI polls.
#ifdef FM11_SETTINGS_GUI
            if(settingsGui)settingsGui->render();
            if(filePickerGui)filePickerGui->render();
#endif
            SDL_Delay(1);
        }
        // The worker probes retain a non-owning monitor pointer. Stop
        // every worker before destroying the monitor to avoid use-after-free.
        cpuWorker.stop();
        machine.stopDisplayCpu();
        machine.stopRealtimePtm();
        monitorProbe.store(nullptr,std::memory_order_release);
        monitorInput.reset();
        monitor.reset();
        // No whole-image rewrite on exit. Ensure all immediately pwrite'd
        // disk updates are fdatasync'd after emulated writers have stopped.
        if(!imageSync.shutdown()) {
            std::cerr<<"fm11-sdl: final disk image sync failed\\n";
            return 1;
        }
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"fm11-sdl: "<<e.what()<<"\n";
        return 1;
    }
}
        auto configureSerial=[&]() {
            serial.reset();
        if(mainBoard || main8088Enabled) {
            if(config.dipSerialMode==MachineConfig::SerialMode::Sync &&
               config.serial0!="pty" && config.serial0!="none")
                throw std::invalid_argument(
                    "DIPSW.serial_mode=sync is unsupported on physical termios ports");
            serial=std::make_unique<SerialBackend>(
                config.serial0,config.dipSerialBaudSwitches,false,
                config.serialRtsCts,
                config.dipSerialCd==MachineConfig::SerialCd::CdAndDsr);
            if(serial->enabled())
                std::cout<<(serial->physical()?"Serial device: ":"Serial PTY: ")
                         <<serial->label()<<"\n"<<std::flush;
        }
        };
        configureSerial();
        std::mutex cpuMutex;
        auto serviceSerial=[&]() {
            if(!serial)return;
            serial->synchronize(*machine.systemBoard().usartDevice());
            if(mainBoard)mainBoard->syncSerialIrq();
            serial->service(
                [&](unsigned char& ch) {
                    BYTE value=0;
                    if(!machine.systemBoard().takeSerialOutput(value))return false;
                    ch=value;return true;
                },
                [&](unsigned char ch){machine.systemBoard().queueSerialInput(ch);});
        };
        // Trace real shared-RAM bus transactions, not guessed values from
        // periodic snapshots. Limit completed requests to keep logs bounded.
        if(traceInkey && mainBoard) {
            auto active=std::make_shared<bool>(false);
            auto count=std::make_shared<unsigned>(0);
            machine.setDisplaySharedTraceObserver(
                [active,count](DisplaySubsystem::SharedTrace e) {
                    if(e.mainCpu && e.write && e.offset==2 && e.value==0x2d) {
                        if(*count>=64)return;
                        *active=true;
                        ++*count;
                        std::cerr<<"INKEY2 #"<<*count<<" request FC82=2D\n";
                    }
                    if(!*active)return;
                    if(e.offset==3 || e.offset==4) {
                        std::cerr<<"INKEY2 #"<<*count<<" "<<(e.mainCpu?"MAIN":"SUB")
                                 <<(e.write?" W":" R")<<" FC8"<<unsigned(e.offset)
                                 <<"=$"<<std::uppercase<<std::hex
                                 <<std::setw(2)<<std::setfill('0')<<unsigned(e.value)
                                 <<std::dec<<std::setfill(' ')<<"\n";
                        if(e.mainCpu && !e.write && e.offset==4)*active=false;
                    }
                });
        }
        // In the opt-in trap mode, use a separate host queue for INKEY 2.
        // The real sub-ROM keyboard path is left untouched otherwise.
        SDL_StartTextInput();
        bool running=true,paused=false;
        std::uint64_t executed=0; // owned by the CPU worker
        std::atomic<std::uint64_t> publishedExecuted{0}; // SDL diagnostics
        std::uint64_t lastReported=0;
        std::uint64_t ioWrites=0;
        if(diagnostics)
            machine.setDisplayIoWriteObserver([&](DisplaySubsystem::IoWriteTrace){++ioWrites;});
        auto lastLog=std::chrono::steady_clock::now();
        auto lastKanjiLog=std::chrono::steady_clock::now();
        // Match the original emulator's approximately 32 ms paint cadence.
        auto lastFrame=lastLog-std::chrono::milliseconds(32);
        CpuSpeedMeter cpuMeter;
        WorkerGroup cpuWorker;
        std::atomic<unsigned> cpuResumeGeneration{0};
        auto lastSpeedReport=std::chrono::steady_clock::now();
        bool wasPaused=false;
        CpuBoard* lastCpuOwner=machine.activeBoard();
        // ROM milestones from the original FM-11 8088 boot ROM.
        // Emit only the first visit to each stage, plus bounded early
        // instructions and occasional progress checkpoints.
        constexpr std::array<unsigned,10> romBootOffsets{
            0x00c5,0x011b,0x0123,0x014e,0x01be,
            0x01f1,0x0291,0x0385,0x042f,0x045b
        };
        std::array<bool,romBootOffsets.size()> romBootSeen{};
        bool diskIplSeen=false, cbiosSeen=false;
        auto traceCpmSnapshot=[&](const char* event) {
            if(!traceCpm80 || !mainBoard)return;
            const auto* owner=machine.activeBoard();
            // No CPU-visible I/O reads: several FDxx reads acknowledge IRQs.
            std::cerr<<"CPM80 "<<event
                     <<" step="<<executed
                     <<" owner="<<(owner && owner->cpu().type()==CpuType::Z80?"Z80":"6809")
                     <<" pc=$"<<std::hex<<(owner?owner->cpu().pc():0)
                     <<" pc6809=$"<<mainBoard->cpu().pc()
                     <<" FD05write=$"<<unsigned(machine.mainSubCpuControl())
                     <<" FDCtrk=$"<<unsigned(mainBoard->fdcTrack())
                     <<" FDCsec=$"<<unsigned(mainBoard->fdcSector())
                     <<" FDCstatus=$"<<unsigned(mainBoard->fdcStatus())
                     <<" phys10000=$"<<unsigned(mainBoard->readPhysical(0x10000))
                     <<" ccpbase=$"<<unsigned(mainBoard->readPhysical(0xdc00))
                     <<" biosbase=$"<<unsigned(mainBoard->readPhysical(0xf200))
                     <<std::dec
                     <<" fdcReads="<<mainBoard->fdcReadCommands()
                     <<" fdcOK="<<mainBoard->fdcSuccessfulReads()
                     <<" fdcFailed="<<mainBoard->fdcFailedReads()
                     <<" hdReads="<<mainBoard->hdReadAttempts()
                     <<" hdFailed="<<mainBoard->hdReadFailures()
                     <<"\n";
        };
        if(traceCpm80)traceCpmSnapshot("reset");

        // Independent hardware clocks: the PTM and sub-CPU workers are
        // unrelated to SDL rendering and the main CPU instruction pacing.
        std::atomic<DebugMonitor*> monitorProbe{nullptr};
        if(monitorEnabled)machine.setDisplayBreakProbe([&](std::uint32_t pc) {
            auto* mon=monitorProbe.load(std::memory_order_acquire);
            return !mon || !mon->needsProbe() ||
                   !mon->probe(DebugMonitor::Target::Sub,pc);
        });
        if(machine.boardCount())machine.startRealtimePtm();
        machine.startDisplayCpu();

        if(machine.boardCount())cpuWorker.start(
            [&,pacer=SdlCpuPacer(speed),
               lastTick=std::chrono::steady_clock::now(),
               generation=0u]() mutable {
                const auto now=std::chrono::steady_clock::now();
                const auto requested=cpuResumeGeneration.load(std::memory_order_acquire);
                if(generation!=requested) {
                    generation=requested;
                    lastTick=now;
                    pacer.reset();
                }
                const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(
                    now-lastTick);
                lastTick=now;
                pacer.addElapsed(elapsed);
                // Drain PTY input before executing the next CPU batch.
                // Running serial I/O here avoids SDL waiting to acquire
                // cpuMutex while the CPU continually executes instructions.
                serviceSerial();
                unsigned ran=0;
                {
                    // One bounded batch is the main CPU's ownership boundary.
                    // No SDL renderer ever executes guest instructions.
                    std::unique_lock<std::mutex> cpuLock(cpuMutex,std::defer_lock);
                    // SDL only reads CPU state in explicit diagnostic modes.
                    if(diagnostics || cpuSpeed)cpuLock.lock();
                    for(;ran<std::min<std::uint64_t>(instructions,512) && pacer.ready();++ran) {
                        const auto* active=machine.activeBoard();
                        if(!active)break;
                        if(auto* mon=monitorProbe.load(std::memory_order_acquire)) {
                            if(mon->needsProbe() &&
                               mon->probe(DebugMonitor::cpuTarget(active->cpu().type()),
                                          active->cpu().pc()))
                                break;
                        }
                        const bool ptmIrqSyncedBefore=
                            mainBoard && active==static_cast<CpuBoard*>(mainBoard);
                        if(ptmIrqSyncedBefore) {
                            mainBoard->syncPtmIrq();
                            const auto wait=mainBoard->cpuWaitState();
                            if(wait!=CSim09::WaitState::Running) {
                                const auto& regs=dynamic_cast<CSim09&>(mainBoard->cpu()).regs;
                                const bool canWake=mainBoard->irqPending() &&
                                    (wait==CSim09::WaitState::Sync ||
                                     (regs.CC&0x10)==0);
                                if(!canWake) {
                                    pacer.reset();
                                    break;
                                }
                            }
                        }
                        const auto activeType=machine.activeCpuType();
                        const auto hz=activeType==CpuType::Z80
                            ?machine.clockZ80Hz()
                            :(activeType==CpuType::I8088 || activeType==CpuType::NECV20)
                                ?machine.clock8088Hz()
                                :mainBoard?mainBoard->cpuClockHz():config.main6809ClockHz;
                        if(traceBoot &&
                           (activeType==CpuType::I8088 || activeType==CpuType::NECV20)) {
                            auto* board8088=dynamic_cast<Main8088Board*>(machine.activeBoard());
                            if(board8088) {
                                const auto& cpu=static_cast<const Main8088Board::Core&>(board8088->cpu());
                                const auto cs=cpu.cs(), ip=cpu.ip();
                                bool milestone=false;
                                if(cs==0xff00) {
                                    for(std::size_t n=0;n<romBootOffsets.size();++n) {
                                        if(ip==romBootOffsets[n] && !romBootSeen[n]) {
                                            romBootSeen[n]=true;
                                            milestone=true;
                                        }
                                    }
                                }
                                const bool diskIpl=(cs==0x07c0 && ip==0);
                                const bool cbios=(cs==0x0400 && ip==0x2500);
                                const bool handoff=(diskIpl && !diskIplSeen) ||
                                                   (cbios && !cbiosSeen);
                                if(diskIpl)diskIplSeen=true;
                                if(cbios)cbiosSeen=true;
                                if(executed<96 || milestone || handoff ||
                                   (executed && executed%200000==0)) {
                                    std::cerr<<"8088 boot step="<<executed
                                             <<" CS:IP="<<std::hex<<std::setw(4)
                                             <<std::setfill('0')<<cs<<":"
                                             <<std::setw(4)<<ip
                                             <<" AX="<<std::setw(4)<<cpu.reg(0)
                                             <<" BX="<<std::setw(4)<<cpu.reg(3)
                                             <<" SP="<<std::setw(4)<<cpu.reg(4)
                                             <<" DS="<<std::setw(4)<<cpu.segment(3)
                                             <<" SS="<<std::setw(4)<<cpu.segment(2)
                                             <<" FLAGS="<<std::setw(4)<<cpu.flags()
                                             <<std::dec<<std::setfill(' ')<<"\n";
                                }
                            }
                        }
                        if(traceBoot && executed<64 && mainBoard) {
                            std::cerr<<"main step="<<executed
                                     <<" PC=$"<<std::hex
                                     <<std::setw(4)<<std::setfill('0')
                                     <<mainBoard->cpu().pc()
                                     <<" subCtrl=$"<<std::setw(2)
                                     <<unsigned(machine.mainSubCpuControl())
                                     <<std::dec<<std::setfill(' ')<<"\n";
                        }
                        machine.executeOne(ptmIrqSyncedBefore);
                        const auto guestCycles=machine.lastExecutionCycles();
                        pacer.consume(guestCycles,hz);
                        if(cpuSpeed) {
                            const auto card=activeType==CpuType::Z80
                                ?CpuSpeedMeter::Card::Z80
                                :(activeType==CpuType::I8088 || activeType==CpuType::NECV20)
                                    ?CpuSpeedMeter::Card::Main8088
                                    :CpuSpeedMeter::Card::Main6809;
                            cpuMeter.record(card,guestCycles);
                        }
                        const auto step=++executed;
                        if(traceCpm80) {
                            CpuBoard* owner=machine.activeBoard();
                            if(owner!=lastCpuOwner) {
                                traceCpmSnapshot("handoff");
                                lastCpuOwner=owner;
                            } else if(step%200000==0)
                                traceCpmSnapshot("progress");
                        }
                    }
                    publishedExecuted.store(executed,std::memory_order_release);
                }
                // Flush guest output before yielding.
                serviceSerial();
                // PTM and display CPU continue on their independent workers.
                if(ran==0 || !pacer.ready())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });

        // Command execution is serialized on the SDL/UI thread.
        auto applyPauseState=[&]() {
            if(paused==wasPaused)return;
            if(paused) {
                cpuWorker.pause();
                machine.pauseRealtimePtm();
                machine.pauseDisplayCpu();
            } else {
                machine.resumeDisplayCpu();
                machine.resumeRealtimePtm();
                cpuResumeGeneration.fetch_add(1,std::memory_order_release);
                cpuWorker.resume();
            }
            if(cpuSpeed) {
                {std::lock_guard<std::mutex> cpuLock(cpuMutex);cpuMeter.reset();}
                lastSpeedReport=std::chrono::steady_clock::now();
            }
            wasPaused=paused;
        };
        std::unique_ptr<DebugMonitor> monitor;
        std::unique_ptr<MonitorInput> monitorInput;
        if(monitorEnabled) {
            // Install memory access hooks only while every guest worker is
            // paused; std::function replacement is not thread-safe.
            const bool startupPaused=paused;
            paused=true;
            applyPauseState();
            monitor=std::make_unique<DebugMonitor>(machine,[&](bool stop) {
                paused=stop;applyPauseState();
            });
            monitorProbe.store(monitor.get(),std::memory_order_release);
            monitorInput=std::make_unique<MonitorInput>();
            monitor->emit("FM-11 Debug Monitor (Stage 3-C). Type help.\n"+monitor->prompt());
            paused=startupPaused;
            applyPauseState();
        }
        while(running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
#ifdef FM11_SETTINGS_GUI
                if(filePickerGui && filePickerGui->processEvent(event))continue;
                if(settingsGui && settingsGui->processEvent(event))continue;
                if(mainMenuGui && mainMenuGui->processEvent(event))continue;
                if(event.type==SDL_KEYDOWN && !event.key.repeat &&
                   event.key.keysym.sym==SDLK_F10 &&
                   (event.key.keysym.mod&KMOD_CTRL)) {
                    settingsGui->toggle();
                    continue;
                }
#endif
                if(event.type==SDL_QUIT)running=false;
#ifdef FM11_SETTINGS_GUI
                // With two SDL windows, closing the CRT does not necessarily
                // generate SDL_QUIT. Closing Settings only hides that window.
                if(event.type==SDL_WINDOWEVENT &&
                   event.window.event==SDL_WINDOWEVENT_CLOSE &&
                   event.window.windowID==SDL_GetWindowID(ui.window))running=false;
#endif
                if(event.type==SDL_TEXTINPUT && !paused && !testPattern && !testKanji) {
                    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(event.text.text);
                        *p;++p) {
                        // The original keyboard queue carries 8-bit codes;
                        // UTF-8 multibyte input needs a separate keymap.
                        if(*p>=0x20 && *p<0x7f) {
                            machine.enqueueDisplayKeyCode(*p);
                        }
                    }
                }
                if(event.type==SDL_KEYDOWN && !event.key.repeat) {
                    const auto sym=event.key.keysym.sym;
                    const auto mod=event.key.keysym.mod;
                    if(testKanji && sym==SDLK_F3) {
                        if(machine.displaySubsystem().kanji2Installed()) {
                            kanjiLevel=3-kanjiLevel;
                            kanjiPage=0;
                            KanjiTestPattern::paint(
                                machine.systemBoard().displaySubsystem(),kanjiPage,kanjiLevel);
                            const std::string title="FM-11 Kanji level "+
                                std::to_string(kanjiLevel)+" page 1/"+
                                std::to_string(KanjiTestPattern::pages);
                            SDL_SetWindowTitle(ui.window,title.c_str());
                        }
                    } else if(testKanji && (sym==SDLK_PAGEDOWN || sym==SDLK_PAGEUP)) {
                        const unsigned next=sym==SDLK_PAGEDOWN
                            ?(kanjiPage+1)%KanjiTestPattern::pages
                            :(kanjiPage+KanjiTestPattern::pages-1)%KanjiTestPattern::pages;
                        if(next!=kanjiPage) {
                            kanjiPage=next;
                            KanjiTestPattern::paint(
                                machine.systemBoard().displaySubsystem(),kanjiPage,kanjiLevel);
                            const unsigned first=kanjiPage*KanjiTestPattern::glyphsPerPage;
                            const unsigned last=std::min(
                                first+KanjiTestPattern::glyphsPerPage,
                                KanjiTestPattern::glyphCount)-1;
                            std::ostringstream title;
                            title<<"FM-11 Kanji level "<<kanjiLevel
                                 <<" page "<<(kanjiPage+1)
                                 <<"/"<<KanjiTestPattern::pages
                                 <<" (glyph "<<std::hex<<std::uppercase
                                 <<first<<"-"<<last<<")";
                            SDL_SetWindowTitle(ui.window,title.str().c_str());
                        }
                    } else if(sym==SDLK_F2 && !testKanji)paused=!paused;
                    else if(sym==SDLK_F1 && !testKanji) {
                        high=!high;
                        ui.setMode(high);
                    } else if(sym==SDLK_F12 && (mod&KMOD_CTRL)) {
                        running=false;
                    } else if(!paused && !testPattern && !testKanji) {
                        // Physical/special keys come from SDL_KEYDOWN; printable
                        // characters (including Space) come from SDL_TEXTINPUT.
                        // Non-ASCII FM-11 codes match the original KeyCode.h.
                        std::uint16_t code=0;
                        if(sym==SDLK_RETURN || sym==SDLK_KP_ENTER)code=0x000d;
                        else if(sym==SDLK_BACKSPACE)code=0x0008;
                        else if(sym==SDLK_TAB)code=0x0009;
                        else if(sym==SDLK_ESCAPE)code=0x001b;
                        else if(sym==SDLK_INSERT)code=0x0110;
                        else if(sym==SDLK_DELETE)code=0x0114;
                        else if(sym==SDLK_HOME)code=0x0115;
                        else if(sym==SDLK_UP)code=0x0116;
                        else if(sym==SDLK_DOWN)code=0x0117;
                        else if(sym==SDLK_LEFT)code=0x0118;
                        else if(sym==SDLK_RIGHT)code=0x0119;
                        else if(sym==SDLK_PAGEUP)code=0x0136;
                        else if(sym==SDLK_PAGEDOWN)code=0x0137;
                        else if(sym>=SDLK_F3 && sym<=SDLK_F10)
                            code=static_cast<std::uint16_t>(0x0100+sym-SDLK_F1);
                        else if(sym==SDLK_F11)code=0x0121;
                        else if(sym==SDLK_F12)code=0x0122;
                        else if((mod&KMOD_CTRL) && sym>=SDLK_a && sym<=SDLK_z)
                            code=static_cast<std::uint16_t>(sym-SDLK_a+1);
                        else if((mod&KMOD_CTRL) && sym==SDLK_LEFTBRACKET)code=0x001b;
                        else if((mod&KMOD_CTRL) && sym==SDLK_BACKSLASH)code=0x001c;
                        else if((mod&KMOD_CTRL) && sym==SDLK_RIGHTBRACKET)code=0x001d;
                        if(code) {
                            machine.enqueueDisplayKeyCode(code);
                        }
                    }
                }
            }
            if(monitor && monitor->breakPending()) {
                paused=true;
                applyPauseState();
                monitor->emit(monitor->takeBreak());
                monitor->emit(monitor->prompt());
            }
            applyPauseState();
#ifdef FM11_SETTINGS_GUI
            if(const auto request=settingsGui->takeFddRequest()) {
                const bool restorePaused=paused;
                paused=true;
                applyPauseState(); // CPU, display CPU and PTM are quiescent.
                std::string error;
                try {
                    if(!liveFloppies)
                        throw std::runtime_error("no FDD controller has been initialized");
                    liveFloppies->exchange(request->channel,request->drive,request->path);
                    std::cerr<<"Stage 4-B "
                             <<(request->path.empty()?"Ejected ":"Mounted ")
                             <<(request->channel?"2HD":"2D")<<" FDD"
                             <<request->drive<<": "<<request->path<<"\n";
                } catch(const std::exception& e) {
                    error=e.what();
                }
                settingsGui->finishFddRequest(*request,error);
                paused=restorePaused;
                applyPauseState();
            }
#endif
            if(monitorInput) {
                std::string line;
                while(monitorInput->pop(line)) {
                    monitor->recordInput(line);
                    monitor->emit(monitor->execute(line,paused));
                    monitor->emit(monitor->prompt());
                }
            }
            // CPU, PTM and display execution are independently scheduled.
            // The SDL thread only services host events and paints frames.
            if(!paused && !testPattern && !testKanji && !machine.boardCount())
                SDL_Delay(1);
            cpuWorker.rethrowWorkerFailure();
            if(cpuSpeed && !paused && machine.boardCount()) {
                const auto reportNow=std::chrono::steady_clock::now();
                const auto reportElapsed=reportNow-lastSpeedReport;
                if(reportElapsed>=std::chrono::seconds(1)) {
                    CpuSpeedMeter::Sample sample;
                    {std::lock_guard<std::mutex> cpuLock(cpuMutex);
                     sample=cpuMeter.take(reportElapsed);}
                    const auto hz6809=mainBoard?mainBoard->cpuClockHz():0;
                    const auto hzZ80=z80Enabled?machine.clockZ80Hz():0;
                    const auto hz8088=main8088Enabled?machine.clock8088Hz():0;
                    std::cerr<<std::fixed<<std::setprecision(3)
                             <<"CPU speed wall="<<sample.wallSeconds<<"s"
                             <<" 6809="<<sample.main6809.cycles<<" T"
                             <<" ("<<sample.mhz(CpuSpeedMeter::Card::Main6809)
                             <<" MHz, "<<sample.main6809.instructions<<" ins)"
                             ;
                    if(z80Enabled)
                        std::cerr<<" Z80="<<sample.z80.cycles<<" T"
                                 <<" ("<<sample.mhz(CpuSpeedMeter::Card::Z80)
                                 <<" MHz, "<<sample.z80.instructions<<" ins)";
                    if(main8088Enabled)
                        std::cerr<<" 8088/V20="<<sample.main8088.cycles<<" T"
                                 <<" ("<<sample.mhz(CpuSpeedMeter::Card::Main8088)
                                 <<" MHz, "<<sample.main8088.instructions<<" ins)";
                    std::cerr<<" target=";
                    if(speed==0)std::cerr<<"unlimited";
                    else std::cerr<<speed<<"x";
                    std::cerr<<" guest/wall="
                             <<sample.progressRatio(hz6809,hzZ80,hz8088)<<"x"
                             <<" [main CPU only; SYNC/CWAI lowers ratio]\n";
                    lastSpeedReport=reportNow;
                }
            }
            const auto frameTime=std::chrono::steady_clock::now();
            if(frameTime-lastFrame>=std::chrono::milliseconds(32)) {
                lastFrame=frameTime;
                const unsigned cursorTick=static_cast<unsigned>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        frameTime.time_since_epoch()).count()/32);
            // Sample main state before taking the display lock. The
            // CPU worker uses CPU -> display lock order for mailbox access.
            int sampledMainPc=-1;
            if(diagnostics) {
                std::lock_guard<std::mutex> cpuLock(cpuMutex);
                if(mainBoard)sampledMainPc=static_cast<int>(mainBoard->cpu().pc());
            }
            if(ui.glRenderer) {
                // Only the snapshot copy is serialized with the sub-CPU.
                // All OpenGL uploads, uniforms, drawing and swap occur after
                // releasing the display-bus lock.
#ifdef FM11_SETTINGS_GUI
                // The settings SDL_Renderer may switch the current OpenGL
                // context. Restore the guest renderer's context explicitly.
                if(SDL_GL_MakeCurrent(ui.window,ui.glContext)!=0)
                    throw std::runtime_error(SDL_GL_GetCurrentContext()?
                        "cannot restore guest OpenGL context":SDL_GetError());
#endif
                const auto snapshot=OpenGlRenderer::capture(machine,!ankPath.empty() && !testKanji);
                int outputW=0,outputH=0;
                SDL_GL_GetDrawableSize(ui.window,&outputW,&outputH);
#ifdef FM11_SETTINGS_GUI
                int menuPixels=MainMenuGui::menuHeight;
                int windowW=0,windowH=0;
                SDL_GetWindowSize(ui.window,&windowW,&windowH);
                menuPixels=windowH>0?MainMenuGui::menuHeight*outputH/windowH:
                    MainMenuGui::menuHeight;
#endif
                ui.glRenderer->render(snapshot,high,testPattern,!ankPath.empty() && !testKanji,
                                       outputW,outputH,cursorTick,scanlines,
#ifdef FM11_SETTINGS_GUI
                                       menuPixels
#else
                                       0
#endif
                                       );
#ifdef FM11_SETTINGS_GUI
                mainMenuGui->render(*settingsGui,paused,running);
#endif
                SDL_GL_SwapWindow(ui.window);
            } else {
                DisplayBitmap::Frame frame{};
                {
                auto displayGuard=machine.lockDisplay();
                frame=machine.captureDisplayFrame(high);
                // Original SubCpu::OnDraw derives both layers from FD A0.
                // In particular, loading a font does NOT enable text by itself.
                if(machine.boardCount()) {
                    const BYTE control=machine.mainDisplayControl();
                    if((control&0x0c)==0) {
                        std::fill(frame.argb.begin(),frame.argb.end(),0xff000000u);
                    } else {
                        // Original PalTab is a lookup of eight four-bit indices.
                        for(auto& pixel:frame.argb) {
                            unsigned index=0;
                            if(pixel&0x00ff0000u)index|=1;
                            if(pixel&0x0000ff00u)index|=2;
                            if(pixel&0x000000ffu)index|=4;
                            pixel=DisplayBitmap::referenceColor(
                                machine.mainDisplayPalette(index));
                        }
                    }
                    if((control&0x03)!=0 && !ankPath.empty())
                        DisplayBitmap::compositeText(frame,machine.displaySubsystem());
                } else if(!ankPath.empty() && !testKanji) {
                    DisplayBitmap::compositeText(frame,machine.displaySubsystem());
                }
                if(!testPattern && !testKanji && !ankPath.empty() &&
                   (!machine.boardCount() || (machine.mainDisplayControl()&0x03)!=0))
                    DisplayBitmap::compositeCursor(frame,machine.displaySubsystem(),cursorTick);
                if(testPattern) {
                    for(std::size_t y=0;y<frame.height;++y)
                        for(std::size_t x=0;x<frame.width;++x)
                            frame.argb[y*frame.width+x]=DisplayBitmap::colorArgb(
                                static_cast<unsigned>((x/80+y/50)%8));
                }
                const auto now=std::chrono::steady_clock::now();
                if(diagnostics && now-lastLog>=std::chrono::seconds(2)) {
                    std::cerr<<"display PC="<<std::hex<<machine.displayCpuPc()
                             <<std::dec<<" steps="<<publishedExecuted.load()
                             <<" delta="<<(publishedExecuted.load()-lastReported)
                             <<" ioWrites="<<ioWrites
                             <<" VRAM_nonzero="<<machine.displayNonzeroVramBytes()
                             <<" page="<<machine.displayPage()
                             <<" revision="<<machine.displayRevision()
                             <<" mainPC="<<sampledMainPc
                             <<" FDA0="<<unsigned(machine.mainDisplayControl())
                             <<" FD05="<<unsigned(machine.mainSubCpuControl())
                             <<" FD04_latch="<<unsigned(machine.mainEventStatus())
                             <<" BUSY="<<unsigned(machine.displayBusyStatus())
                             <<" sharedWrites="<<machine.displaySharedWriteRevision()
                             <<" shared0="<<unsigned(machine.displaySharedByte(0))
                             <<" halted="<<machine.displayHalted()
                             <<(machine.boardCount()?" main6809":" display-only")
                             <<(testPattern?" test-pattern":"")<<"\n";
                    lastReported=publishedExecuted.load();
                    lastLog=now;
                }
                } // release display bus before SDL texture upload/draw
                if(SDL_UpdateTexture(ui.texture,nullptr,frame.argb.data(),
                                     640*static_cast<int>(sizeof(std::uint32_t)))!=0)
                    throw std::runtime_error(SDL_GetError());
                // Integer-only pixel scaling: avoid uneven 1/2-pixel stripes
                // when a 640-wide frame is stretched to a 960-wide window.
                // For windows smaller than the native image, use a centered
                // clipped 1:1 image instead of fractional downscaling.
                int outputW=0,outputH=0;
                if(SDL_GetRendererOutputSize(ui.renderer,&outputW,&outputH)!=0)
                    throw std::runtime_error(SDL_GetError());
                const int nativeW=static_cast<int>(frame.width);
#ifdef FM11_SETTINGS_GUI
                const int menuPixels=MainMenuGui::menuHeight;
#else
                const int menuPixels=0;
#endif
                const int availableHeight=std::max(0,outputH-menuPixels);
                const int scale=std::max(1,std::min(outputW/nativeW,availableHeight/400));
                const SDL_Rect destination{
                    (outputW-nativeW*scale)/2,
                    menuPixels+(availableHeight-400*scale)/2,
                    nativeW*scale,400*scale
                };
                // The emulator image stays untouched. Paint the letterbox
                // margins dark gray, and draw a one-pixel border OUTSIDE
                // the screen rectangle whenever those pixels are visible.
                if(SDL_SetRenderDrawColor(ui.renderer,48,52,56,255)!=0)
                    throw std::runtime_error(SDL_GetError());
                if(SDL_RenderClear(ui.renderer)!=0)
                    throw std::runtime_error(SDL_GetError());
                if(SDL_RenderCopy(ui.renderer,ui.texture,nullptr,&destination)!=0)
                    throw std::runtime_error(SDL_GetError());
                // Optional CRT scanline mask on alternate doubled rows.
                if(!high && scanlines) {
                    SDL_SetRenderDrawBlendMode(ui.renderer,SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ui.renderer,0,0,0,128);
                    for(int line=0;line<200;++line) {
                        const SDL_Rect stripe{destination.x,
                            destination.y+(line*2+1)*scale,
                            destination.w,scale};
                        SDL_RenderFillRect(ui.renderer,&stripe);
                    }
                    SDL_SetRenderDrawBlendMode(ui.renderer,SDL_BLENDMODE_NONE);
                }
                const SDL_Rect border{
                    destination.x-1,destination.y-1,
                    destination.w+2,destination.h+2
                };
                if(SDL_SetRenderDrawColor(ui.renderer,144,148,152,255)!=0)
                    throw std::runtime_error(SDL_GetError());
                if(SDL_RenderDrawRect(ui.renderer,&border)!=0)
                    throw std::runtime_error(SDL_GetError());
#ifdef FM11_SETTINGS_GUI
                mainMenuGui->render(*settingsGui,paused,running);
#endif
                SDL_RenderPresent(ui.renderer);
                }
            }
            // The two renderers consume the same sub-CPU-written graphics
            // VRAM; report guest Kanji reads regardless of renderer mode.
            if(traceKanji) {
                const auto current=std::chrono::steady_clock::now();
                if(current-lastKanjiLog>=std::chrono::seconds(2)) {
                    const auto state=machine.displaySubsystem().kanjiStatistics();
                    std::cerr<<"KANJI selections="<<state.selections
                             <<" left="<<state.leftReads
                             <<" right="<<state.rightReads
                             <<" videoRevision="<<machine.displayRevision()
                             <<"\n";
                    lastKanjiLog=current;
                }
            }
            // The main CPU no longer depends on SDL paint cadence. Yield
            // host time to the three emulation workers between UI polls.
#ifdef FM11_SETTINGS_GUI
            if(settingsGui)settingsGui->render();
            if(filePickerGui)filePickerGui->render();
#endif
            SDL_Delay(1);
        }
        // The worker probes retain a non-owning monitor pointer. Stop
        // every worker before destroying the monitor to avoid use-after-free.
        cpuWorker.stop();
        machine.stopDisplayCpu();
        machine.stopRealtimePtm();
        monitorProbe.store(nullptr,std::memory_order_release);
        monitorInput.reset();
        monitor.reset();
        // No whole-image rewrite on exit. Ensure all immediately pwrite'd
        // disk updates are fdatasync'd after emulated writers have stopped.
        if(!imageSync.shutdown()) {
            std::cerr<<"fm11-sdl: final disk image sync failed\\n";
            return 1;
        }
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"fm11-sdl: "<<e.what()<<"\n";
        return 1;
    }
}
