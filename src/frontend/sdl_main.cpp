#include "Machine.h"
#include "DebugMonitor.h"
#include "SdlCpuPacer.h"
#include "OpenGlRenderer.h"
#include "CpuSpeedMeter.h"
#include "Main6809Board.h"
#include "MainZ80Board.h"
#include "Main8088Board.h"
#include "MachineConfig.h"
#ifdef FM11_SETTINGS_GUI
#include "SettingsGui.h"
#include "MainMenuGui.h"
#endif
#include "SerialBackend.h"
#include "DisplayBitmap.h"
#include "KanjiTestPattern.h"
#include "RomFiles.h"
#include "HostImageSync.h"
#include "SdlBeeper.h"
#include <SDL.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <memory>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <poll.h>
#include <deque>

namespace {
// Poll stdin without blocking SDL shutdown while no line is entered.
class MonitorInput {
public:
    MonitorInput():thread_([this]{readLoop();}) {}
    ~MonitorInput() {stop_=true;if(thread_.joinable())thread_.join();}
    bool pop(std::string& line) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(queue_.empty())return false;
        line=std::move(queue_.front());queue_.pop_front();return true;
    }
private:
    void readLoop() {
        std::string partial;
        while(!stop_) {
            pollfd fd{STDIN_FILENO,POLLIN|POLLHUP,0};
            const int ready=::poll(&fd,1,100);
            if(ready<0) {if(errno==EINTR)continue;return;}
            if(ready==0)continue;
            if(!(fd.revents&(POLLIN|POLLHUP)))return;
            char buf[256];
            const ssize_t n=::read(STDIN_FILENO,buf,sizeof(buf));
            if(n<=0)return;
            for(ssize_t i=0;i<n;++i) {
                if(buf[i]=='\n') {
                    if(!partial.empty() && partial.back()=='\r')partial.pop_back();
                    {std::lock_guard<std::mutex> lock(mutex_);queue_.push_back(std::move(partial));}
                    partial.clear();
                } else if(partial.size()<4096)partial+=buf[i];
            }
        }
    }
    std::atomic<bool> stop_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::deque<std::string> queue_;
};
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
        SDL_SetWindowSize(window,960,600);
    }
};
std::vector<BYTE> readDiskImage(const std::string& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot open floppy image: "+path);
    return std::vector<BYTE>(std::istreambuf_iterator<char>(file),
                             std::istreambuf_iterator<char>());
}
// Preserve D88 sector headers and commit the whole image atomically.
// Never truncate the mounted file before every byte of the replacement
// has reached a temporary file in the same directory.
std::uint64_t parseCount(const std::string& text) {
    std::size_t end=0;
    const auto value=std::stoull(text,&end,0);
    if(end!=text.size())throw std::invalid_argument("invalid cycle limit");
    return value;
}
void usage() {
    std::cout<<"usage: fm11-sdl [--beep-test] [--rom FILE] [--6809-rom FILE] [--high] [--cycles N] [--test-pattern] [--test-kanji] [--main-rom FILE] [--trace-boot] [--trace-fdc] [--trace-inkey] [--diagnostics] [--ank-rom FILE] [--kanji-rom FILE] [--kanji2-rom FILE] [--test-kanji-level 1|2] [--trace-kanji] [--floppy2d FILE | --floppy-d88 FILE] [--floppy-drive 0..3] [--boot-dip VALUE] [--config FILE] [--8088-rom FILE] [--trace-cpm80] [--cpu-speed] [--speed 0..100] [--renderer auto|gl|software] [--scanlines] [--monitor]\n"
             <<"  ROM defaults to [display_cpu].rom (external/roms/display6809.bin); CPU ROM paths may be set in the config file\n"
             <<"  Initial window height: [display].height=200|400 (default 400); --high forces 400\n"
             <<"  File / Settings / Help in CRT menu; Ctrl+F10 settings; F2 pause; Ctrl+F12 quit\n"
             <<"  --beep-test: audible 205ms pulse, pause, continuous tone, then stop.\n"
             <<"  --test-kanji: ROM glyph gallery, PgUp/PgDn change page (40x25 glyphs).\n"
             <<"                Uses --kanji-rom FILE or external/roms/kanji.bin.\n"
             <<"                F3 switches level 1/2 when --kanji2-rom is loaded.\n"
             <<"  --kanji2-rom FILE: optional JIS level-2 128-KiB ROM.\n"
             <<"  --test-kanji-level 1|2: initial gallery ROM bank.\n"
             <<"  --speed N: main CPU multiplier (default 10, 1=real-time);\n"
             <<"             display CPU uses the same multiplier, capped at 8 MHz.\n"
             <<"             0=unlimited main CPU, 8 MHz display CPU; PTM stays real-time.\n"
             <<"  --trace-kanji: report sub-CPU AFF4/5/6/7 activity every 2 seconds.\n"
             <<"  --renderer auto (default): prefer OpenGL 3.3, fall back to software.\n"
             <<"  200-line display uses 2x vertical pixels; --scanlines adds CRT-style dark rows.\n"
             <<"  Mode is user-selected until hardware mode bits are decoded.\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string displayRomOverride, mainRomPath, main6809RomOverride, ankPath, kanjiPath, kanji2Path, floppyPath;
        std::string configPath,main8088RomOverride;
        unsigned speed=10;
        std::string rendererMode="auto";
        bool scanlines=false;
        bool beepTest=false;
        bool traceKanji=false;
        bool testKanji=false;
        unsigned kanjiPage=0;
        unsigned kanjiLevel=1;
        unsigned floppyDrive=0;
        unsigned bootDip=0x9f;
        bool bootDipOverridden=false;
        bool high=false, highOverridden=false, testPattern=false, traceBoot=false, traceFdc=false, traceInkey=false, diagnostics=false, traceCpm80=false, cpuSpeed=false, monitorEnabled=false;
        std::uint64_t instructions=10000;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--help"){usage();return 0;}
            if(arg=="--high"){high=true;highOverridden=true;continue;}
            if(arg=="--scanlines"){scanlines=true;continue;}
            if(arg=="--beep-test"){beepTest=true;continue;}
            if(arg=="--test-kanji"){testKanji=true;continue;}
            if(arg=="--trace-kanji"){traceKanji=true;continue;}
             if(arg=="--trace-cpm80"){traceCpm80=true;continue;}
            if(arg=="--cpu-speed"){cpuSpeed=true;continue;}
            if(arg=="--monitor"){monitorEnabled=true;continue;}
            if(arg=="--test-pattern"){testPattern=true;continue;}
            if(arg=="--trace-boot"){traceBoot=true;continue;}
            if(arg=="--trace-fdc"){traceFdc=true;continue;}
            if(arg=="--trace-inkey"){traceInkey=true;continue;}
            if(arg=="--diagnostics"){diagnostics=true;continue;}
            if((arg=="--rom" || arg=="--cycles" || arg=="--main-rom" || arg=="--6809-rom" || arg=="--ank-rom" || arg=="--kanji-rom" || arg=="--kanji2-rom" || arg=="--test-kanji-level" || arg=="--floppy2d" || arg=="--floppy-d88" || arg=="--floppy-drive" || arg=="--boot-dip" || arg=="--config" || arg=="--8088-rom" || arg=="--speed" || arg=="--renderer") && i+1<argc) {
                if(arg=="--rom")displayRomOverride=argv[++i];
                else if(arg=="--config")configPath=argv[++i];
                else if(arg=="--8088-rom")main8088RomOverride=argv[++i];
                else if(arg=="--6809-rom")main6809RomOverride=argv[++i];
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
                else if(arg=="--kanji-rom")kanjiPath=argv[++i];
                else if(arg=="--kanji2-rom")kanji2Path=argv[++i];
                else if(arg=="--test-kanji-level") {
                    const auto level=parseCount(argv[++i]);
                    if(level!=1 && level!=2)
                        throw std::invalid_argument("--test-kanji-level must be 1 or 2");
                    kanjiLevel=static_cast<unsigned>(level);
                }
                else if(arg=="--boot-dip") {
                    const auto v=parseCount(argv[++i]);
                    if(v>255)throw std::invalid_argument("--boot-dip must be a byte");
                    bootDip=static_cast<unsigned>(v);
                    bootDipOverridden=true;
                }
                else if(arg=="--floppy-drive") {
                    const auto d=parseCount(argv[++i]);
                    if(d>3)throw std::invalid_argument("--floppy-drive must be 0..3");
                    floppyDrive=static_cast<unsigned>(d);
                }
                else if(arg=="--floppy2d" || arg=="--floppy-d88") {
                    if(!floppyPath.empty())throw std::invalid_argument("only one floppy image may be mounted");
                    floppyPath=argv[++i];
                }
                else instructions=parseCount(argv[++i]);
            } else {usage();return 2;}
        }
        const MachineConfig config=loadMachineConfig(configPath);
        if(!highOverridden)high=config.displayHeight==400;
        if(testPattern && testKanji)
            throw std::invalid_argument("--test-pattern and --test-kanji are mutually exclusive");
        if(testKanji)high=true; // 16x16 glyphs at native 640x400, no vertical doubling
        // Explicit raw DIP override remains available for diagnostics.
        if(!bootDipOverridden)bootDip=config.bootDip();
        if(traceCpm80 && (!config.z80Installed || !config.main6809Installed))
            throw std::invalid_argument("--trace-cpm80 requires an installed Z80 and main 6809");
        // Card-specific settings provide defaults. CLI --6809-rom/--8088-rom
        // override those defaults; --main-rom is the boot-ROM override for
        // the DIP-selected main card regardless of its architecture.
        std::string romPath=displayRomOverride.empty()?config.displayRom:displayRomOverride;
        std::string rom6809=main6809RomOverride.empty()?config.main6809Rom:main6809RomOverride;
        std::string rom8088=main8088RomOverride.empty()?config.main8088Rom:main8088RomOverride;
        if(!mainRomPath.empty()) {
            if(config.mainSelected==MachineConfig::MainSelection::Cpu6809)
                rom6809=mainRomPath;
            else rom8088=mainRomPath;
        }
        if(ankPath.empty())ankPath=config.ankRom;
        if(kanjiPath.empty())kanjiPath=config.kanjiRom;
        if(kanji2Path.empty())kanji2Path=config.kanji2Rom;
        if((testPattern || testKanji) &&
           (!mainRomPath.empty() || !main6809RomOverride.empty() || !main8088RomOverride.empty()))
            throw std::invalid_argument("CPU ROMs cannot be combined with display test modes");
        if(testKanji && kanjiPath.empty())
            kanjiPath="external/roms/kanji.bin";
        if(instructions>1000000)
            throw std::invalid_argument("--cycles must be <= 1000000");
        // Declare before Machine: CPU callbacks must be destroyed before
        // mounted host image descriptors and their sync worker.
        HostImageSync imageSync;
        Machine machine;
        machine.setDisplayCpu6309Enabled(config.displayCpu==CpuType::HD6309);
        machine.systemBoard().hddDevice()->enableTiming(config.hddTimed);
        Main6809Board* mainBoard=nullptr;
        bool z80Enabled=false;
        bool main8088Enabled=false;
        if(!testPattern && !testKanji) {
            machine.loadDisplayRomFile(romPath);
            if(!ankPath.empty())machine.loadAnkRomFile(ankPath);
            if(!kanjiPath.empty())machine.loadKanjiRomFile(kanjiPath);
            if(!kanji2Path.empty())machine.loadKanji2RomFile(kanji2Path);
            if(!rom6809.empty() || !rom8088.empty()) {
                std::size_t cpu6809Index=0,cpu8088Index=0,z80Index=0;
                if(config.main6809Installed) {
                    auto mainCpu=std::make_unique<Main6809Board>(config.main6809Cpu);
                    if(!rom6809.empty())mainCpu->loadBootRomFile(rom6809);
                    else if(config.mainSelected==MachineConfig::MainSelection::Cpu6809)
                        throw std::invalid_argument("selected 6809 card requires [main_6809].rom or --main-rom/--6809-rom");
                    mainBoard=mainCpu.get();
                    mainCpu->setCpuClockHz(config.main6809ClockHz);
                    cpu6809Index=machine.addBoard(std::move(mainCpu));
                }
                machine.setRamKiB(config.ramKiB);
                if(config.main8088Installed) {
                    auto mainCpu=std::make_unique<Main8088Board>(config.main8088Cpu);
                    if(!rom8088.empty())mainCpu->bus().loadBootRomFile(rom8088);
                    else if(config.mainSelected==MachineConfig::MainSelection::Cpu8088)
                        throw std::invalid_argument("selected 8088 card requires [main_8088].rom or --main-rom/--8088-rom");
                    cpu8088Index=machine.addBoard(std::move(mainCpu));
                    machine.set8088ClockHz(config.main8088ClockHz);
                    main8088Enabled=true;
                }
                const auto selectedIndex=config.mainSelected==MachineConfig::MainSelection::Cpu6809
                    ?cpu6809Index:cpu8088Index;
                // DIP selects the active main card even if both are fitted.
                // Only the optional Z80 is under FD05 arbitration.
                machine.setActiveBoard(selectedIndex);
                if(config.z80Installed) {
                    auto z80=std::make_unique<MainZ80Board>();
                    z80Index=machine.addBoard(std::move(z80));
                    machine.setZ80ClockHz(config.z80ClockHz);
                    machine.configureFd05Boards(selectedIndex,z80Index);
                    machine.enableFd05Selection(true);
                    z80Enabled=true;
                }
                // addBoard() attaches the CPU to the machine's shared
                // FloppyDevice. Mount only afterwards: mounting on the
                // CPU's initial private device would lose the image.
                {
                    auto floppy=machine.systemBoard().floppyDevice();
                    if(traceFdc)floppy->setTimingTrace([](const std::string& message) {
                        std::cerr<<"FDC-TIME "<<message<<"\n";
                    });
                    auto paths2d=config.floppy2d, paths2hd=config.floppy2hd;
                    if(!floppyPath.empty()) {
                        // Legacy command-line image overrides this 2D slot.
                        paths2d[floppyDrive]=floppyPath;
                    }
                    std::array<std::array<std::shared_ptr<HostImageSync::Image>,4>,2> opened{};
                    std::array<std::string,8> allPaths{};
                    unsigned imageCount=0;
                    for(unsigned channel=0;channel<2;++channel) {
                        const auto& paths=channel==0?paths2d:paths2hd;
                        for(unsigned drive=0;drive<4;++drive) {
                            const auto& path=paths[drive];
                            if(path.empty())continue;
                            // Two virtual drives must not write the same host image.
                            if(std::find(allPaths.begin(),allPaths.begin()+imageCount,path)
                               !=allPaths.begin()+imageCount)
                                throw std::runtime_error("disk image mounted twice: "+path);
                            allPaths[imageCount++]=path;
                            const auto bytes=readDiskImage(path);
                            if(channel==0) {
                                if(bytes.size()==40u*2*16*256)
                                    floppy->load2d(bytes,drive);
                                else
                                    floppy->loadD88(bytes,drive);
                            } else {
                                if(bytes.size()==77u*2*8*1024 || bytes.size()==77u*2*26*256)
                                    floppy->load2hd(bytes,drive);
                                else
                                    floppy->loadD882hd(bytes,drive);
                            }
                            opened[channel][drive]=imageSync.open(path);
                            std::cerr<<"Mounted "<<(channel==0?"2D":"2HD")
                                     <<" FDD"<<drive<<": "<<path<<"\n";
                        }
                    }
                    std::weak_ptr<FloppyDevice> weakFloppy=floppy;
                    floppy->setSectorWriter([opened,weakFloppy](
                        unsigned channel,unsigned drive,std::size_t rawOffset,
                        const BYTE* data,std::size_t length) {
                        if(channel>=2 || drive>=4 || !opened[channel][drive])
                            return false;
                        auto disk=weakFloppy.lock();
                        if(!disk)return false;
                        const std::size_t imageOffset=
                            disk->hostImageOffset(channel,rawOffset,drive);
                        return opened[channel][drive]->write(imageOffset,data,length);
                    });
                    // WRITE TRACK may resize D88: update the full image
                    // immediately; fdatasync is deferred like sector writes.
                    floppy->setImageWriter([opened](unsigned channel,unsigned drive,
                                                     const std::vector<BYTE>& image) {
                        if(channel>=2 || drive>=4 || !opened[channel][drive])
                            return false;
                        return opened[channel][drive]->replace(image);
                    });
                }
                if(!config.hddImage.empty()) {
                    const auto image=readDiskImage(config.hddImage);
                    auto hdd=machine.systemBoard().hddDevice();
                    hdd->load(image);
                    auto ownedImage=imageSync.open(config.hddImage);
                    hdd->setHostWriter([ownedImage](std::size_t offset,
                                                   const BYTE* data,std::size_t length) {
                        return ownedImage->write(offset,data,length);
                    });
                    std::cerr<<"Mounted MDC HDD0: "<<config.hddImage<<"\n";
                }
            }
        }
        // FD23 boot-device DIP is on the shared system board. An FM-11 BS
        // has no 6809 main CPU card, but its 8088 boot ROM still reads FD23.
        machine.systemBoard().setBootDip(static_cast<BYTE>(bootDip));
        if(mainBoard)mainBoard->setBootDip(static_cast<BYTE>(bootDip));

        machine.setDisplayCpuClockHz((testPattern || testKanji)?0:config.displayClockHz);
        machine.setDisplayCpuSpeedMultiplier(speed);
        machine.reset();
        if(testKanji) {
            machine.loadKanjiRomFile(kanjiPath);
            if(!kanji2Path.empty())machine.loadKanji2RomFile(kanji2Path);
            KanjiTestPattern::paint(machine.systemBoard().displaySubsystem(),
                                    kanjiPage,kanjiLevel);
            std::cout<<"Kanji ROM gallery: "<<kanjiPath
                     <<(kanji2Path.empty()?"":" + "+kanji2Path)
                     <<" level="<<kanjiLevel
                     <<" ("<<KanjiTestPattern::glyphCount<<" glyphs, "
                     <<KanjiTestPattern::pages<<" pages)\n"
                     <<"PageUp/PageDown: change ROM range; F3: switch ROM level; Ctrl+F12: quit\n"
                     <<std::flush;
        }
        machine.systemBoard().usartDevice()->requireCarrierAndDsr(
            config.dipSerialCd==MachineConfig::SerialCd::CdAndDsr);
        machine.systemBoard().floppyDevice()->enableSeekTiming(config.floppyTimed);
        if(config.serialClocked) {
            if(config.serial0!="pty" && config.serial0!="none")
                throw std::invalid_argument(
                    "serial.timing=clocked cannot be used with physical TTY");
            if(config.dipSerialMode!=MachineConfig::SerialMode::Async)
                throw std::invalid_argument(
                    "serial.timing=clocked requires DIP async mode");
            const auto clock=Fm11SerialClock::asyncClockHz(
                config.dipSerialBaudSwitches);
            if(!clock)throw std::invalid_argument(
                "serial.timing=clocked requires a defined DIP baud rate");
            machine.systemBoard().usartDevice()->setExternalClockHz(clock);
        }
        Sdl sdl;
        Window ui;
        auto createWindow=[&](Uint32 flags) {
            return SDL_CreateWindow("FM-11 display subsystem",
                 SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,
                 600,flags|SDL_WINDOW_RESIZABLE);
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
#ifdef FM11_SETTINGS_GUI
        // Separate SDL2 renderer: the emulated CRT's GL/software backend and
        // guest framebuffer are unaffected by the editor window.
        MachineConfig effectiveConfig=config;
        effectiveConfig.main6809Rom=rom6809;
        effectiveConfig.main8088Rom=rom8088;
        effectiveConfig.displayRom=romPath;
        effectiveConfig.ankRom=ankPath;
        effectiveConfig.kanjiRom=kanjiPath;
        effectiveConfig.kanji2Rom=kanji2Path;
        effectiveConfig.displayHeight=high?400:200;
        if(!floppyPath.empty())effectiveConfig.floppy2d[floppyDrive]=floppyPath;
        if(bootDipOverridden) {
            effectiveConfig.boot2dFdd=(bootDip&0x20)==0;
            effectiveConfig.boot2hdFdd=(bootDip&0x40)==0;
            effectiveConfig.bootHdd=(bootDip&0x80)==0;
        }
        auto settingsGui=std::make_unique<SettingsGui>(effectiveConfig);
        auto filePickerGui=std::make_unique<FilePickerGui>();
        settingsGui->setFilePicker(filePickerGui.get());
        auto mainMenuGui=std::make_unique<MainMenuGui>(ui.window,ui.renderer,ui.glContext);
        mainMenuGui->setFilePicker(filePickerGui.get());
#endif
        std::cerr<<"FM-11 renderer: "<<(ui.glRenderer?"OpenGL 3.3":"software")<<"\n";
        if(testKanji) {
            const std::string title="FM-11 Kanji level "+
                std::to_string(kanjiLevel)+" page 1/"+
                std::to_string(KanjiTestPattern::pages);
            SDL_SetWindowTitle(ui.window,title.c_str());
        }
        // Audio device owns an SDL callback but never touches guest CPU
        // state. A missing host audio device must not block OS boot.
        SdlBeeper audio(machine.systemBoard().beeperDevice());
        if(beepTest) {
            if(!audio.available())
                throw std::runtime_error("--beep-test requires an SDL audio output device");
            std::cout<<"Beeper self-test: pulse, pause, continuous tone\n"<<std::flush;
            auto& bus=machine.systemBoard();
            bus.writeIo(0xfd03,0x41);
            SDL_Delay(270);
            bus.writeIo(0xfd03,0);
            SDL_Delay(120);
            bus.writeIo(0xfd03,0x81);
            SDL_Delay(350);
            bus.writeIo(0xfd03,0);
            std::cout<<"Beeper self-test finished\n"<<std::flush;
        }
        // CPU worker alone owns the host endpoint and 8251A queues.
        std::unique_ptr<SerialBackend> serial;
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
