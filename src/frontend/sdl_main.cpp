#include "Machine.h"
#include "SdlCpuPacer.h"
#include "OpenGlRenderer.h"
#include "CpuSpeedMeter.h"
#include "Main6809Board.h"
#include "MainZ80Board.h"
#include "DisplayBitmap.h"
#include "RomFiles.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <memory>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>

namespace {
struct Sdl {
    Sdl() {
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0)
            throw std::runtime_error(SDL_GetError());
    }
    ~Sdl(){SDL_Quit();}
};
struct Window {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* texture=nullptr;
    SDL_GLContext glContext=nullptr;
    std::unique_ptr<OpenGlRenderer> glRenderer;
    ~Window() {
        // GL resources must be destroyed while their context is current.
        glRenderer.reset();
        if(texture)SDL_DestroyTexture(texture);
        if(renderer)SDL_DestroyRenderer(renderer);
        if(glContext)SDL_GL_DeleteContext(glContext);
        if(window)SDL_DestroyWindow(window);
    }
    void setMode(bool high) {
        const int height=high?400:200;
        if(renderer) {
            if(texture)SDL_DestroyTexture(texture);
            texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING,640,height);
            if(!texture)throw std::runtime_error(SDL_GetError());
        }
        SDL_SetWindowSize(window,960,height*3/2);
    }
};
std::vector<BYTE> readDiskImage(const std::string& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot open floppy image: "+path);
    return std::vector<BYTE>(std::istreambuf_iterator<char>(file),
                             std::istreambuf_iterator<char>());
}
std::uint64_t parseCount(const std::string& text) {
    std::size_t end=0;
    const auto value=std::stoull(text,&end,0);
    if(end!=text.size())throw std::invalid_argument("invalid cycle limit");
    return value;
}
void usage() {
    std::cout<<"usage: fm11-sdl [--rom FILE] [--high] [--cycles N] [--test-pattern] [--main-rom FILE] [--trace-boot] [--trace-inkey] [--diagnostics] [--ank-rom FILE] [--floppy2d FILE | --floppy-d88 FILE] [--floppy-drive 0..3] [--boot-dip VALUE] [--z80] [--z80-clock HZ] [--trace-cpm80] [--cpu-speed] [--speed 0..100] [--renderer auto|gl|software]\n"
             <<"  ROM defaults to external/roms/display6809.bin\n"
             <<"  F2 pause/resume; F1 switch 200/400-line view; Ctrl+F12 quit\n"
             <<"  --speed N: emulated-time multiplier (default 10, 1=real-time, 0=unlimited).\n"
             <<"  --renderer auto (default): prefer OpenGL 3.3, fall back to software.\n"
             <<"  Mode is user-selected until hardware mode bits are decoded.\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string romPath="external/roms/display6809.bin", mainRomPath, ankPath, floppyPath;
        bool floppyD88=false, z80Enabled=false;
        std::uint64_t z80ClockHz=4000000;
        unsigned speed=10;
        std::string rendererMode="auto";
        unsigned floppyDrive=0;
        unsigned bootDip=0x9f;
        bool high=false, testPattern=false, traceBoot=false, traceInkey=false, diagnostics=false, traceCpm80=false, cpuSpeed=false;
        std::uint64_t instructions=10000;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--help"){usage();return 0;}
            if(arg=="--high"){high=true;continue;}
            if(arg=="--z80"){z80Enabled=true;continue;}
            if(arg=="--trace-cpm80"){traceCpm80=true;continue;}
            if(arg=="--cpu-speed"){cpuSpeed=true;continue;}
            if(arg=="--test-pattern"){testPattern=true;continue;}
            if(arg=="--trace-boot"){traceBoot=true;continue;}
            if(arg=="--trace-inkey"){traceInkey=true;continue;}
            if(arg=="--diagnostics"){diagnostics=true;continue;}
            if((arg=="--rom" || arg=="--cycles" || arg=="--main-rom" || arg=="--ank-rom" || arg=="--floppy2d" || arg=="--floppy-d88" || arg=="--floppy-drive" || arg=="--boot-dip" || arg=="--z80-clock" || arg=="--speed" || arg=="--renderer") && i+1<argc) {
                if(arg=="--rom")romPath=argv[++i];
                else if(arg=="--z80-clock")z80ClockHz=parseCount(argv[++i]);
                else if(arg=="--renderer") {
                    rendererMode=argv[++i];
                    if(rendererMode!="auto" && rendererMode!="gl" &&
                       rendererMode!="software")
                        throw std::invalid_argument("--renderer must be auto, gl, or software");
                }
                else if(arg=="--speed") {
                    const auto value=parseCount(argv[++i]);
                    if(value>SdlCpuPacer::maxSpeed)
                        throw std::invalid_argument("--speed must be 0..100");
                    speed=static_cast<unsigned>(value);
                }
                else if(arg=="--main-rom")mainRomPath=argv[++i];
                else if(arg=="--ank-rom")ankPath=argv[++i];
                else if(arg=="--boot-dip") {
                    const auto v=parseCount(argv[++i]);
                    if(v>255)throw std::invalid_argument("--boot-dip must be a byte");
                    bootDip=static_cast<unsigned>(v);
                }
                else if(arg=="--floppy-drive") {
                    const auto d=parseCount(argv[++i]);
                    if(d>3)throw std::invalid_argument("--floppy-drive must be 0..3");
                    floppyDrive=static_cast<unsigned>(d);
                }
                else if(arg=="--floppy2d" || arg=="--floppy-d88") {
                    if(!floppyPath.empty())throw std::invalid_argument("only one floppy image may be mounted");
                    floppyD88=(arg=="--floppy-d88");
                    floppyPath=argv[++i];
                }
                else instructions=parseCount(argv[++i]);
            } else {usage();return 2;}
        }
        if(traceCpm80 && !z80Enabled)
            throw std::invalid_argument("--trace-cpm80 requires --z80");
        if(z80Enabled && (testPattern || mainRomPath.empty()))
            throw std::invalid_argument("--z80 requires --main-rom and cannot use --test-pattern");
        if(instructions>1000000)
            throw std::invalid_argument("--cycles must be <= 1000000");
        Machine machine;
        Main6809Board* mainBoard=nullptr;
        if(!testPattern) {
            machine.loadDisplayRomFile(romPath);
            if(!ankPath.empty())machine.loadAnkRomFile(ankPath);
            if(!mainRomPath.empty()) {
                auto mainCpu=std::make_unique<Main6809Board>(CpuType::MC6809);
                mainCpu->loadBootRomFile(mainRomPath);
                mainBoard=mainCpu.get();
                mainCpu->setCpuClockHz(2000000);
                machine.addBoard(std::move(mainCpu));
                if(z80Enabled) {
                    machine.addBoard(std::make_unique<MainZ80Board>());
                    machine.enableFd05Selection(true);
                    machine.setZ80ClockHz(z80ClockHz);
                }
                // addBoard() attaches the CPU to the machine's shared
                // FloppyDevice. Mount only afterwards: mounting on the
                // CPU's initial private device would lose the image.
                if(!floppyPath.empty()) {
                    auto image=readDiskImage(floppyPath);
                    if(floppyD88)mainBoard->loadFloppyD88(image);
                    else mainBoard->loadFloppy2d(image);
                    mainBoard->setFloppyDrive(floppyDrive);
                }
            }
        }
        if(mainBoard)mainBoard->setBootDip(static_cast<BYTE>(bootDip));
        if(!floppyPath.empty() && mainBoard==nullptr)
            throw std::invalid_argument("floppy image requires --main-rom");
        machine.setDisplayCpuClockHz(testPattern?0:2000000);
        machine.reset();
        Sdl sdl;
        Window ui;
        auto createWindow=[&](Uint32 flags) {
            return SDL_CreateWindow("FM-11 display subsystem",
                 SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,
                 high?600:300,flags|SDL_WINDOW_RESIZABLE);
        };
        if(rendererMode!="software") {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
            ui.window=createWindow(SDL_WINDOW_OPENGL);
            if(ui.window) {
                ui.glContext=SDL_GL_CreateContext(ui.window);
                if(ui.glContext) {
                    try {
                        SDL_GL_SetSwapInterval(0); // never throttle CPU on VSYNC
                        ui.glRenderer=std::make_unique<OpenGlRenderer>();
                    } catch(const std::exception& e) {
                        if(rendererMode=="gl")throw;
                        std::cerr<<"OpenGL unavailable: "<<e.what()<<"; using software renderer\n";
                    }
                }
            }
            if(!ui.glRenderer) {
                if(rendererMode=="gl")
                    throw std::runtime_error(std::string("OpenGL unavailable: ")+SDL_GetError());
                if(ui.glContext) {SDL_GL_DeleteContext(ui.glContext);ui.glContext=nullptr;}
                if(ui.window) {SDL_DestroyWindow(ui.window);ui.window=nullptr;}
            }
        }
        if(!ui.glRenderer) {
            ui.window=createWindow(0);
            if(!ui.window)throw std::runtime_error(SDL_GetError());
            ui.renderer=SDL_CreateRenderer(ui.window,-1,SDL_RENDERER_ACCELERATED);
            if(!ui.renderer)
                ui.renderer=SDL_CreateRenderer(ui.window,-1,SDL_RENDERER_SOFTWARE);
            if(!ui.renderer)throw std::runtime_error(SDL_GetError());
        }
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
        ui.setMode(high);
        std::cerr<<"FM-11 renderer: "<<(ui.glRenderer?"OpenGL 3.3":"software")<<"\n";
        // Standalone raw PTY for guest serial, independent of SDL input.
        int serialMaster=-1,serialSlave=-1;
        if(mainBoard) {
            serialMaster=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
            if(serialMaster<0 || grantpt(serialMaster)!=0 || unlockpt(serialMaster)!=0)
                throw std::runtime_error("cannot create serial PTY");
            const char* name=ptsname(serialMaster);
            if(!name)throw std::runtime_error("cannot resolve serial PTY");
            serialSlave=open(name,O_RDWR|O_NOCTTY);
            if(serialSlave<0)throw std::runtime_error("cannot open serial PTY slave");
            termios mode{};
            if(tcgetattr(serialSlave,&mode)==0) {
                cfmakeraw(&mode);
                tcsetattr(serialSlave,TCSANOW,&mode);
            }
            std::cout<<"Serial PTY: "<<name<<"\n"<<std::flush;
        }
        std::vector<BYTE> serialPending;
        // The main CPU worker owns CPU/FDC/USART state. SDL serial I/O
        // briefly takes this lock, never the display bus first.
        std::mutex cpuMutex;
        auto serviceSerial=[&]() {
            std::lock_guard<std::mutex> cpuLock(cpuMutex);
            if(serialMaster<0)return;
            BYTE ch=0;
            while(mainBoard->takeSerialOutput(ch))serialPending.push_back(ch);
            if(!serialPending.empty()) {
                const ssize_t n=write(serialMaster,serialPending.data(),serialPending.size());
                if(n>0)serialPending.erase(serialPending.begin(),serialPending.begin()+n);
                else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR && errno!=EIO)
                    throw std::runtime_error("serial PTY write failed");
            }
            BYTE input[256];
            for(;;) {
                const ssize_t n=read(serialMaster,input,sizeof(input));
                if(n>0) {
                    for(ssize_t i=0;i<n;++i)mainBoard->queueSerialInput(input[i]);
                } else if(n<0 && errno==EINTR)continue;
                else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EIO)
                    throw std::runtime_error("serial PTY read failed");
                else break;
            }
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
        std::atomic<std::uint64_t> executed{0};
        std::uint64_t lastReported=0;
        std::uint64_t ioWrites=0;
        if(diagnostics)
            machine.setDisplayIoWriteObserver([&](DisplaySubsystem::IoWriteTrace){++ioWrites;});
        auto lastLog=std::chrono::steady_clock::now();
        // Match the original emulator's approximately 32 ms paint cadence.
        auto lastFrame=lastLog-std::chrono::milliseconds(32);
        CpuSpeedMeter cpuMeter;
        WorkerGroup cpuWorker;
        std::atomic<unsigned> cpuResumeGeneration{0};
        auto lastSpeedReport=std::chrono::steady_clock::now();
        bool wasPaused=false;
        CpuBoard* lastCpuOwner=machine.activeBoard();
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
                unsigned ran=0;
                {
                    // One bounded batch is the main CPU's ownership boundary.
                    // No SDL renderer ever executes guest instructions.
                    std::lock_guard<std::mutex> cpuLock(cpuMutex);
                    for(;ran<std::min<std::uint64_t>(instructions,512) && pacer.ready();++ran) {
                        const auto* active=machine.activeBoard();
                        if(!active)break;
                        if(mainBoard && active==static_cast<CpuBoard*>(mainBoard)) {
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
                        const auto hz=active->cpu().type()==CpuType::Z80
                            ?machine.clockZ80Hz()
                            :mainBoard?mainBoard->cpuClockHz():2000000ULL;
                        if(traceBoot && executed.load()<64 && mainBoard) {
                            std::cerr<<"main step="<<executed.load()
                                     <<" PC=$"<<std::hex
                                     <<std::setw(4)<<std::setfill('0')
                                     <<mainBoard->cpu().pc()
                                     <<" subCtrl=$"<<std::setw(2)
                                     <<unsigned(machine.mainSubCpuControl())
                                     <<std::dec<<std::setfill(' ')<<"\n";
                        }
                        machine.executeOne();
                        const auto guestCycles=machine.lastExecutionCycles();
                        pacer.consume(guestCycles,hz);
                        if(cpuSpeed) {
                            const auto card=active->cpu().type()==CpuType::Z80
                                ?CpuSpeedMeter::Card::Z80
                                :CpuSpeedMeter::Card::Main6809;
                            cpuMeter.record(card,guestCycles);
                        }
                        const auto step=executed.fetch_add(1)+1;
                        if(traceCpm80) {
                            CpuBoard* owner=machine.activeBoard();
                            if(owner!=lastCpuOwner) {
                                traceCpmSnapshot("handoff");
                                lastCpuOwner=owner;
                            } else if(step%200000==0)
                                traceCpmSnapshot("progress");
                        }
                    }
                }
                // Yield while the guest is asleep or CPU pacing is ahead.
                // PTM and display CPU continue on their independent workers.
                if(ran==0 || !pacer.ready())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });

        while(running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_QUIT)running=false;
                if(event.type==SDL_TEXTINPUT && !paused && !testPattern) {
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
                    if(sym==SDLK_F2)paused=!paused;
                    else if(sym==SDLK_F1) {
                        high=!high;
                        ui.setMode(high);
                    } else if(sym==SDLK_F12 && (mod&KMOD_CTRL)) {
                        running=false;
                    } else if(!paused && !testPattern) {
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
            if(paused!=wasPaused) {
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
            }
            if(cpuSpeed && paused!=wasPaused) {
                // Pause/resume should not dilute a measurement window.
                {std::lock_guard<std::mutex> cpuLock(cpuMutex);
                 cpuMeter.reset();}
                lastSpeedReport=std::chrono::steady_clock::now();
            }
            wasPaused=paused;
            // CPU, PTM and display execution are independently scheduled.
            // The SDL thread only services host events and paints frames.
            if(!paused && !testPattern && !machine.boardCount())
                SDL_Delay(1);
            cpuWorker.rethrowWorkerFailure();
            serviceSerial();
            if(cpuSpeed && !paused && machine.boardCount()) {
                const auto reportNow=std::chrono::steady_clock::now();
                const auto reportElapsed=reportNow-lastSpeedReport;
                if(reportElapsed>=std::chrono::seconds(1)) {
                    CpuSpeedMeter::Sample sample;
                    {std::lock_guard<std::mutex> cpuLock(cpuMutex);
                     sample=cpuMeter.take(reportElapsed);}
                    const auto hz6809=mainBoard?mainBoard->cpuClockHz():0;
                    const auto hzZ80=z80Enabled?machine.clockZ80Hz():0;
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
                    std::cerr<<" target=";
                    if(speed==0)std::cerr<<"unlimited";
                    else std::cerr<<speed<<"x";
                    std::cerr<<" guest/wall="
                             <<sample.progressRatio(hz6809,hzZ80)<<"x"
                             <<" [main CPU only; SYNC/CWAI lowers ratio]\n";
                    lastSpeedReport=reportNow;
                }
            }
            const auto frameTime=std::chrono::steady_clock::now();
            if(frameTime-lastFrame>=std::chrono::milliseconds(32)) {
                lastFrame=frameTime;
            // Sample main state before taking the display lock. The
            // CPU worker uses CPU -> display lock order for mailbox access.
            int sampledMainPc=-1;
            if(diagnostics) {
                std::lock_guard<std::mutex> cpuLock(cpuMutex);
                if(mainBoard)sampledMainPc=static_cast<int>(mainBoard->cpu().pc());
            }
            if(ui.glRenderer) {
                {
                auto displayGuard=machine.lockDisplay();
                int outputW=0,outputH=0;
                SDL_GL_GetDrawableSize(ui.window,&outputW,&outputH);
                ui.glRenderer->render(machine,high,testPattern,!ankPath.empty(),
                                      outputW,outputH);
                }
                // GPU swap is outside the display bus critical section.
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
                } else if(!ankPath.empty()) {
                    DisplayBitmap::compositeText(frame,machine.displaySubsystem());
                }
                if(testPattern) {
                    for(std::size_t y=0;y<frame.height;++y)
                        for(std::size_t x=0;x<frame.width;++x)
                            frame.argb[y*frame.width+x]=DisplayBitmap::colorArgb(
                                static_cast<unsigned>((x/80+y/50)%8));
                }
                const auto now=std::chrono::steady_clock::now();
                if(diagnostics && now-lastLog>=std::chrono::seconds(2)) {
                    std::cerr<<"display PC="<<std::hex<<machine.displayCpuPc()
                             <<std::dec<<" steps="<<executed
                             <<" delta="<<(executed-lastReported)
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
                    lastReported=executed.load();
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
                const int nativeH=static_cast<int>(frame.height);
                const int scale=std::max(1,std::min(outputW/nativeW,outputH/nativeH));
                const SDL_Rect destination{
                    (outputW-nativeW*scale)/2,
                    (outputH-nativeH*scale)/2,
                    nativeW*scale,nativeH*scale
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
                const SDL_Rect border{
                    destination.x-1,destination.y-1,
                    destination.w+2,destination.h+2
                };
                if(SDL_SetRenderDrawColor(ui.renderer,144,148,152,255)!=0)
                    throw std::runtime_error(SDL_GetError());
                if(SDL_RenderDrawRect(ui.renderer,&border)!=0)
                    throw std::runtime_error(SDL_GetError());
                SDL_RenderPresent(ui.renderer);
                }
            }
            // The main CPU no longer depends on SDL paint cadence. Yield
            // host time to the three emulation workers between UI polls.
            SDL_Delay(1);
        }
        cpuWorker.stop();
        machine.stopDisplayCpu();
        machine.stopRealtimePtm();
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"fm11-sdl: "<<e.what()<<"\n";
        return 1;
    }
}
