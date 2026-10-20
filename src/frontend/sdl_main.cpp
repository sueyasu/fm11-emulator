#include "Machine.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include "RomFiles.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <sstream>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include <string>

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
    ~Window() {
        if(texture)SDL_DestroyTexture(texture);
        if(renderer)SDL_DestroyRenderer(renderer);
        if(window)SDL_DestroyWindow(window);
    }
    void setMode(bool high) {
        const int height=high?400:200;
        if(texture)SDL_DestroyTexture(texture);
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,640,height);
        if(!texture)throw std::runtime_error(SDL_GetError());
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
    std::cout<<"usage: fm11-sdl [--no-keyboard] [--keyboard-off-after-init] [--rom FILE] [--high] [--cycles N] [--test-pattern] [--main-rom FILE] [--trace-boot] [--diagnostics] [--ank-rom FILE] [--floppy2d FILE | --floppy-d88 FILE] [--floppy-drive 0..3] [--boot-dip VALUE]\n"
             <<"  ROM defaults to external/roms/display6809.bin\n"
             <<"  F12 quit; F10 pause/resume; F1 switch 200/400-line view\n"
             <<"  Mode is user-selected until hardware mode bits are decoded.\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string romPath="external/roms/display6809.bin", mainRomPath, ankPath, floppyPath;
        bool floppyD88=false;
        unsigned floppyDrive=0;
        unsigned bootDip=0x9f;
        bool high=false, testPattern=false, traceBoot=false, diagnostics=false;
        bool noKeyboard=false,keyboardOffAfterInit=false;
        std::uint64_t instructions=2000;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--help"){usage();return 0;}
            if(arg=="--high"){high=true;continue;}
            if(arg=="--no-keyboard"){noKeyboard=true;continue;}
            if(arg=="--keyboard-off-after-init"){keyboardOffAfterInit=true;continue;}
            if(arg=="--test-pattern"){testPattern=true;continue;}
            if(arg=="--trace-boot"){traceBoot=true;continue;}
            if(arg=="--diagnostics"){diagnostics=true;continue;}
            if((arg=="--rom" || arg=="--cycles" || arg=="--main-rom" || arg=="--ank-rom" || arg=="--floppy2d" || arg=="--floppy-d88" || arg=="--floppy-drive" || arg=="--boot-dip") && i+1<argc) {
                if(arg=="--rom")romPath=argv[++i];
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
        machine.setExternalDisplayVsync(true);
        machine.setReferenceWallClock(true);
        if(keyboardOffAfterInit)machine.disableDisplayKeyboardAfterStartupKey(true);
        Sdl sdl;
        Window ui;
        ui.window=SDL_CreateWindow("FM-11 display subsystem",
             SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,
             high?600:300,SDL_WINDOW_RESIZABLE);
        if(!ui.window)throw std::runtime_error(SDL_GetError());
        ui.renderer=SDL_CreateRenderer(ui.window,-1,
                       SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!ui.renderer)
            ui.renderer=SDL_CreateRenderer(ui.window,-1,SDL_RENDERER_SOFTWARE);
        if(!ui.renderer)throw std::runtime_error(SDL_GetError());
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
        ui.setMode(high);
        // Keep the host terminal for the monitor; expose the guest UART
        // on a separate raw PTY, as in fm11-headless.
        int serialMaster=-1,serialSlave=-1;
        std::string serialPath;
        if(mainBoard) {
            serialMaster=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
            if(serialMaster<0 || grantpt(serialMaster)!=0 || unlockpt(serialMaster)!=0)
                throw std::runtime_error("cannot create serial PTY");
            const char* name=ptsname(serialMaster);
            if(!name)throw std::runtime_error("cannot resolve serial PTY");
            serialPath=name;
            serialSlave=open(serialPath.c_str(),O_RDWR|O_NOCTTY);
            if(serialSlave<0)throw std::runtime_error("cannot open serial slave");
            termios uart{};
            if(tcgetattr(serialSlave,&uart)==0) {
                cfmakeraw(&uart);
                tcsetattr(serialSlave,TCSANOW,&uart);
            }
            std::cout<<"FM-11 SDL monitor\nSerial: "<<serialPath
                     <<"\nCPU halted. Type help for commands.\n";
        }
        bool monitorInputOpen=mainBoard!=nullptr;
        std::string monitorLine;
        std::vector<BYTE> serialPending;
        std::uint64_t serialRxBytes=0,serialTxBytes=0;
        BYTE serialLastRx=0;
        auto hex=[](unsigned value,unsigned width) {
            std::ostringstream o;
            o<<std::uppercase<<std::hex<<std::setw(width)<<std::setfill('0')<<value;
            return o.str();
        };
        auto showRegisters=[&]() {
            if(!mainBoard)return;
            const auto& r=dynamic_cast<CSim09&>(mainBoard->cpu()).regs;
            std::cout<<"PC="<<hex(r.r.w.PC,4)
                     <<" S="<<hex(r.r.w.S,4)<<" U="<<hex(r.r.w.U,4)
                     <<" X="<<hex(r.r.w.X,4)<<" Y="<<hex(r.r.w.Y,4)
                     <<" A="<<hex(r.r.b.A,2)<<" B="<<hex(r.r.b.B,2)
                     <<" DP="<<hex(r.DP,2)<<" CC="<<hex(r.CC,2)
                     <<" MD="<<hex(r.MD,2)<<"\n";
        };
        auto drainSerial=[&]() {
            if(serialMaster<0)return;
            BYTE ch=0;
            while(mainBoard->takeSerialOutput(ch))serialPending.push_back(ch);
            if(!serialPending.empty()) {
                const ssize_t n=write(serialMaster,serialPending.data(),serialPending.size());
                if(n>0) {
                    serialTxBytes+=static_cast<std::uint64_t>(n);
                    serialPending.erase(serialPending.begin(),serialPending.begin()+n);
                }
                else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EIO)
                    throw std::runtime_error("serial PTY write failed");
            }
            BYTE input[256];
            for(;;) {
                const ssize_t n=read(serialMaster,input,sizeof(input));
                if(n>0) {
                    for(ssize_t i=0;i<n;++i) {
                        serialLastRx=input[i];
                        ++serialRxBytes;
                        mainBoard->queueSerialInput(input[i]);
                    }
                } else if(n<0 && errno==EINTR)continue;
                else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EIO)
                    throw std::runtime_error("serial PTY read failed");
                else break;
            }
        };
        // Host printable keys enter the original sub-CPU keyboard queue.
        SDL_StartTextInput();
        bool running=true,paused=mainBoard!=nullptr;
        std::uint64_t executed=0,lastReported=0;
        std::uint64_t ioWrites=0;
        if(diagnostics)
            machine.setDisplayIoWriteObserver([&](DisplaySubsystem::IoWriteTrace){++ioWrites;});
        auto lastLog=std::chrono::steady_clock::now();
        auto lastFrame=lastLog-std::chrono::milliseconds(17);
        auto lastWaitTick=std::chrono::steady_clock::now();
        auto lastVsyncTick=lastWaitTick;
        auto lastPeripheralTick=lastWaitTick;
        auto monitorCommand=[&](const std::string& line) {
            if(!mainBoard)return;
            std::istringstream stream(line);
            std::string cmd,addrText,lenText;
            stream>>cmd;
            if(cmd=="h"||cmd=="help"||cmd=="?") {
                std::cout<<"c/continue, stop, s/step, r/regs, m/mem HEXADDR [LEN], "
                            "mp HEXADDR [LEN], keyboard on/off/status, serial, reset, q/quit, help\n";
            } else if(cmd=="c"||cmd=="continue") {
                paused=false;
                std::cout<<"CPU running. Type stop to pause.\n";
            } else if(cmd=="stop") {
                paused=true;
                std::cout<<"CPU halted.\n";
            } else if(cmd=="keyboard") {
                std::string state;
                stream>>state;
                if(state=="status" || state.empty()) {
                    const auto& kb=machine.displaySubsystem();
                    std::cout<<"Guest keyboard="
                             <<(kb.keyboardEnabled()?"ON":"OFF")
                             <<" autoOff="<<(kb.keyboardAutoDisableArmed()?"armed":"no")
                             <<" startupDelivered="<<(kb.keyboardStartupDelivered()?"yes":"no")
                             <<" queue="<<kb.keyboardQueueDepth()
                             <<" keyFIRQ="<<unsigned(kb.keyboardFirqLatch())
                             <<"\n";
                } else if(state=="off") {
                    machine.setDisplayKeyboardEnabled(false);
                    std::cout<<"Guest keyboard disabled (queues and key latches cleared).\n";
                } else if(state=="on") {
                    machine.setDisplayKeyboardEnabled(true);
                    std::cout<<"Guest keyboard enabled.\n";
                } else {
                    std::cout<<"usage: keyboard on|off|status\n";
                }
            } else if(cmd=="q"||cmd=="quit")running=false;
            else if(cmd=="r"||cmd=="regs")showRegisters();
            else if(cmd=="serial"||cmd=="uart") {
                std::cout<<"PTY rx="<<serialRxBytes<<" tx="<<serialTxBytes
                         <<" lastRx=$"<<hex(serialLastRx,2)
                         <<" USART status=$"<<hex(mainBoard->readCpu(0xfd07),2)
                         <<" FD02=$"<<hex(mainBoard->lastIoWrite(0x02),2)
                         <<" FD03=$"<<hex(mainBoard->readCpu(0xfd03),2)
                         <<" CPU IRQ="<<(mainBoard->irqPending()?"yes":"no")
                         <<"\n";
            }
            else if(cmd=="s"||cmd=="step") {
                if(!paused)std::cout<<"Stop CPU first.\n";
                else {machine.executeOne();++executed;drainSerial();showRegisters();}
            } else if(cmd=="reset") {
                if(!paused)std::cout<<"Stop CPU first.\n";
                else {
                    machine.reset();executed=0;serialPending.clear();
                    lastVsyncTick=std::chrono::steady_clock::now();
                    lastPeripheralTick=lastVsyncTick;
                    std::cout<<"CPU reset.\n";showRegisters();
                }
            } else if(cmd=="m"||cmd=="mem"||cmd=="mp") {
                stream>>addrText>>lenText;
                try {
                    if(addrText.empty())throw std::invalid_argument("missing address");
                    const auto addr=std::stoull(addrText,nullptr,16);
                    const auto len=lenText.empty()?64:std::stoull(lenText,nullptr,0);
                    const auto size=cmd=="mp"?0x100000ULL:0x10000ULL;
                    if(addr>=size||len>256||len>size-addr)
                        throw std::out_of_range("memory range");
                    for(std::uint64_t i=0;i<len;++i) {
                        if(i%16==0)std::cout<<hex(unsigned(addr+i),cmd=="mp"?5:4)<<":";
                        const BYTE value=cmd=="mp"
                            ?mainBoard->readPhysical(std::uint32_t(addr+i))
                            :mainBoard->readCpu(WORD(addr+i));
                        std::cout<<" "<<hex(value,2);
                        if(i%16==15 || i+1==len)std::cout<<"\n";
                    }
                }catch(const std::exception&) {
                    std::cout<<"usage: m/mp HEXADDR [LEN, up to 256]\n";
                }
            } else if(!cmd.empty())std::cout<<"Unknown command. Type help.\n";
            if(running&&paused)std::cout<<"fm11> "<<std::flush;
        };
        if(mainBoard)std::cout<<"fm11> "<<std::flush;
        while(running) {
            const auto peripheralNow=std::chrono::steady_clock::now();
            if(paused) {
                lastPeripheralTick=peripheralNow;
            } else {
                const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(
                    peripheralNow-lastPeripheralTick).count();
                lastPeripheralTick=peripheralNow;
                if(elapsed>0)
                    machine.advanceTimeNanoseconds(static_cast<std::uint64_t>(elapsed));
            }
            // Display VSync is sourced from real elapsed time in SDL only.
            // Do not feed the same event from guest-time advancement.
            const auto vsyncNow=std::chrono::steady_clock::now();
            if(paused) {
                lastVsyncTick=vsyncNow;
            } else if(vsyncNow-lastVsyncTick>=std::chrono::milliseconds(16)) {
                const auto elapsed=vsyncNow-lastVsyncTick;
                const auto ticks=elapsed/std::chrono::milliseconds(16);
                lastVsyncTick+=std::chrono::milliseconds(16)*ticks;
                // Windows WM_TIMER is coalesced; one notification is
                // dispatched, even if multiple intervals elapsed.
                machine.signalReferenceTimer(static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()));
            }
            if(monitorInputOpen) {
                pollfd fd{STDIN_FILENO,POLLIN,0};
                if(poll(&fd,1,0)>0 && (fd.revents&(POLLIN|POLLHUP))) {
                    char data[512];
                    const ssize_t n=read(STDIN_FILENO,data,sizeof(data));
                    if(n<=0)monitorInputOpen=false;
                    else for(ssize_t i=0;i<n;++i) {
                        const char c=data[i];
                        if(c=='\n'||c=='\r') {
                            const auto command=monitorLine;
                            monitorLine.clear();
                            monitorCommand(command);
                        } else if(c==0x7f||c=='\b') {
                            if(!monitorLine.empty())monitorLine.pop_back();
                        } else if(monitorLine.size()<1024)monitorLine+=c;
                    }
                }
            }
            drainSerial();
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_QUIT)running=false;
                if(event.type==SDL_TEXTINPUT && !paused && !testPattern && !noKeyboard) {
                    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(event.text.text);
                        *p;++p) {
                        // The original keyboard queue carries 8-bit codes;
                        // UTF-8 multibyte input needs a separate keymap.
                        if(*p>=0x20 && *p<0x7f)
                            machine.enqueueDisplayKeyCode(*p);
                    }
                }
                if(event.type==SDL_KEYDOWN && !event.key.repeat) {
                    if(event.key.keysym.sym==SDLK_F12)running=false;
                    if(event.key.keysym.sym==SDLK_F10)paused=!paused;
                    // SDL_TEXTINPUT provides printable keys (including Space).
                    // Control characters do not generate SDL_TEXTINPUT and
                    // must be sent on KEYDOWN to reach OS-9 /Term.
                    if(!paused && !testPattern && !noKeyboard) {
                        switch(event.key.keysym.sym) {
                        case SDLK_RETURN: case SDLK_KP_ENTER:
                            machine.enqueueDisplayKeyCode(0x0d); break;
                        case SDLK_BACKSPACE:
                            machine.enqueueDisplayKeyCode(0x08); break;
                        case SDLK_TAB:
                            machine.enqueueDisplayKeyCode(0x09); break;
                        case SDLK_ESCAPE:
                            machine.enqueueDisplayKeyCode(0x1b); break;
                        // Source FM-11 KeyCode.h / ChildView::OnKeyDown.
                        // Modifier flags are encoded in the 16-bit key code,
                        // not delivered as separate key events.
                        case SDLK_UP: case SDLK_DOWN:
                        case SDLK_LEFT: case SDLK_RIGHT:
                        case SDLK_INSERT: case SDLK_DELETE:
                        case SDLK_HOME: case SDLK_PAGEUP:
                        case SDLK_END: case SDLK_PAGEDOWN:
                        case SDLK_F2: case SDLK_F3: case SDLK_F4:
                        case SDLK_F5: case SDLK_F6: case SDLK_F7:
                        case SDLK_F8: case SDLK_F9:
                        case SDLK_PAUSE: {
                            std::uint16_t code=0;
                            switch(event.key.keysym.sym) {
                            case SDLK_UP: code=0x0116; break;
                            case SDLK_DOWN: code=0x0117; break;
                            case SDLK_LEFT: code=0x0118; break;
                            case SDLK_RIGHT: code=0x0119; break;
                            case SDLK_INSERT: code=0x0110; break;
                            case SDLK_PAGEUP: code=0x0111; break;
                            case SDLK_END: code=0x0112; break;
                            case SDLK_PAGEDOWN: code=0x0113; break;
                            case SDLK_DELETE: code=0x0114; break;
                            case SDLK_HOME: code=0x0115; break;
                            case SDLK_PAUSE: code=0x0201; break;
                            default:
                                code=static_cast<std::uint16_t>(
                                    0x0101+event.key.keysym.sym-SDLK_F1);
                                break;
                            }
                            if(code!=0x0201) {
                                if((event.key.keysym.mod & KMOD_SHIFT)!=0)
                                    code|=0x0020;
                                if((event.key.keysym.mod & KMOD_CTRL)!=0)
                                    code|=0x0040;
                            }
                            machine.enqueueDisplayKeyCode(code);
                            break;
                        }
                        default: break;
                        }
                    }
                    if(event.key.keysym.sym==SDLK_F1) {
                        high=!high;
                        ui.setMode(high);
                    }
                }
            }
            if(!paused && !testPattern) {
                if(machine.boardCount()) {
                    for(std::uint64_t i=0;i<instructions;++i) {
                        // Only inspect the PC before execution: reading memory
                        // via CPU bus here might acknowledge I/O registers.
                        if(traceBoot && executed+i<64 && mainBoard) {
                            std::cerr<<"main step="<<(executed+i)
                                     <<" PC=$"<<std::hex
                                     <<std::setw(4)<<std::setfill('0')
                                     <<mainBoard->cpu().pc()
                                     <<" subCtrl=$"<<std::setw(2)
                                     <<unsigned(machine.mainSubCpuControl())
                                     <<std::dec<<std::setfill(' ')<<"\n";
                        }
                        machine.executeOne();
                    }
                } else {
                    // 2 MHz display CPU: advance elapsed time without a main
                    // CPU. The Machine scheduler also handles HALT and waits.
                    machine.advanceTimeNanoseconds(instructions*500);
                }
                executed+=instructions;
            }
            drainSerial();
            const auto frameTime=std::chrono::steady_clock::now();
            if(frameTime-lastFrame>=std::chrono::microseconds(16667)) {
                lastFrame=frameTime;
            auto frame=machine.captureDisplayFrame(high);
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
                         <<" mainPC="<<(mainBoard?static_cast<int>(mainBoard->cpu().pc()):-1)
                         <<" FDA0="<<unsigned(machine.mainDisplayControl())
                         <<" FD05="<<unsigned(machine.mainSubCpuControl())
                         <<" FD04_latch="<<unsigned(machine.mainEventStatus())
                         <<" BUSY="<<unsigned(machine.displayBusyStatus())
                         <<" sharedWrites="<<machine.displaySharedWriteRevision()
                         <<" shared0="<<unsigned(machine.displaySharedByte(0))
                         <<" halted="<<machine.displayHalted()
                         <<(machine.boardCount()?" main6809":" display-only")
                         <<(testPattern?" test-pattern":"")<<"\n";
                lastReported=executed;
                lastLog=now;
            }
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
            // Keep the CPU executing between display refreshes. Rendering
            // every 10,000 guest instructions throttled ROM memory checks
            // with texture uploads, software composition and vsync waits.
        }
        if(serialSlave>=0)close(serialSlave);
        if(serialMaster>=0)close(serialMaster);
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"fm11-sdl: "<<e.what()<<"\n";
        return 1;
    }
}
