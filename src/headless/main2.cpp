#include "Machine.h"
#include "Main6809Board.h"
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace {
volatile std::sig_atomic_t stopRequested=0;
void stop(int) {stopRequested=1;}
std::uint64_t number(const std::string& s) {
    std::size_t end=0;
    const auto n=std::stoull(s,&end,0);
    if(end!=s.size())throw std::invalid_argument("invalid number: "+s);
    return n;
}
std::vector<BYTE> readImage(const std::string& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot open image: "+path);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
void usage() {
    std::cout<<"usage: fm11-headless2 --main-rom FILE [--rom FILE]"
                " [--floppy-d88 FILE | --floppy2d FILE] [--floppy-drive 0..3]"
                " [--boot-dip BYTE] [--cycles N] [--instructions LIMIT]"
                " [--no-keyboard] [--ank-rom FILE] [--high] [--trace-boot] [--diagnostics]\n"
                "  Same Machine CPU interleave, 16ms timer and PTM clocking as fm11-sdl.\n"
                "  Monitor on stdin; guest UART on a separate /dev/pts/N.\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string mainRom,subRom="external/roms/display6809.bin",floppy,ank;
        bool d88=false,diagnostics=false,trace=false,noKeyboard=false;
        std::uint64_t cycles=2000,limit=0,bootDip=0x9f,drive=0;
        for(int i=1;i<argc;++i) {
            const std::string a=argv[i];
            if(a=="--help"){usage();return 0;}
            if(a=="--diagnostics"){diagnostics=true;continue;}
            if(a=="--trace-boot"){trace=true;continue;}
            if(a=="--no-keyboard"){noKeyboard=true;continue;}
            if(a=="--high"){continue;} // display mode has no effect without a window
            if(i+1>=argc){usage();return 2;}
            const std::string v=argv[++i];
            if(a=="--main-rom")mainRom=v;
            else if(a=="--rom")subRom=v;
            else if(a=="--ank-rom")ank=v;
            else if(a=="--floppy-d88"||a=="--floppy2d") {
                if(!floppy.empty())throw std::invalid_argument("only one floppy allowed");
                floppy=v;d88=a=="--floppy-d88";
            } else if(a=="--floppy-drive")drive=number(v);
            else if(a=="--boot-dip")bootDip=number(v);
            else if(a=="--cycles")cycles=number(v);
            else if(a=="--instructions")limit=number(v);
            else {usage();return 2;}
        }
        if(mainRom.empty()||drive>3||bootDip>255||cycles==0||cycles>1000000) {
            usage();return 2;
        }
        Machine machine;
        machine.loadDisplayRomFile(subRom);
        if(!ank.empty())machine.loadAnkRomFile(ank);
        auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
        board->loadBootRomFile(mainRom);
        auto* main=board.get();
        main->setCpuClockHz(2000000);
        machine.addBoard(std::move(board));
        if(!floppy.empty()) {
            auto image=readImage(floppy);
            if(d88)main->loadFloppyD88(image);
            else main->loadFloppy2d(image);
            main->setFloppyDrive(static_cast<unsigned>(drive));
        }
        main->setBootDip(static_cast<BYTE>(bootDip));
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        machine.setExternalDisplayVsync(true);
        machine.setReferenceWallClock(true);
        if(noKeyboard)machine.setDisplayKeyboardEnabled(false);

        const int master=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
        if(master<0||grantpt(master)!=0||unlockpt(master)!=0)
            throw std::runtime_error("cannot create serial PTY");
        const char* slaveName=ptsname(master);
        if(!slaveName)throw std::runtime_error("cannot resolve serial PTY");
        const std::string path=slaveName;
        const int slave=open(path.c_str(),O_RDWR|O_NOCTTY|O_NONBLOCK);
        if(slave<0)throw std::runtime_error("cannot open serial PTY");
        termios settings{};
        if(tcgetattr(slave,&settings)==0) {
            cfmakeraw(&settings);
            tcsetattr(slave,TCSANOW,&settings);
        }
        std::cout<<"Guest serial: "<<path<<"\n"
                 <<"Monitor: c/continue, stop, s/step, r/regs, q/quit\n";
        std::signal(SIGINT,stop);
        std::signal(SIGTERM,stop);
        bool running=true,inputOpen=true;
        std::string line;
        std::uint64_t executed=0;
        auto lastClock=std::chrono::steady_clock::now();
        auto lastVsync=lastClock;
        auto lastStats=lastClock;
        std::vector<BYTE> serialPending;
        auto drain=[&] {
            BYTE byte=0;
            while(main->takeSerialOutput(byte))serialPending.push_back(byte);
            if(!serialPending.empty()) {
                const ssize_t n=write(master,serialPending.data(),serialPending.size());
                if(n>0)serialPending.erase(serialPending.begin(),serialPending.begin()+n);
            }
        };
        auto regs=[&] {
            std::cout<<"main PC=$"<<std::hex<<std::setw(4)<<std::setfill('0')
                     <<main->cpu().pc()<<" sub PC=$"<<std::setw(4)
                     <<machine.displayCpuPc()<<std::dec<<std::setfill(' ')
                     <<" FD05="<<unsigned(machine.mainSubCpuControl())
                     <<" instructions="<<executed<<"\n";
        };
        while(!stopRequested) {
            // Same reference timebase as fm11-sdl: PTM runs from memory
            // accesses while the main CPU runs, and from steady_clock
            // during SYNC/CWAI (or without a 6809 main CPU).
            const auto now=std::chrono::steady_clock::now();
            const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(
                now-lastClock).count();
            lastClock=now;
            if(running && main->cpuWaitState()!=CSim09::WaitState::Running &&
               elapsed>0)
                machine.advanceTimeNanoseconds(static_cast<std::uint64_t>(elapsed));
            if(running && now-lastVsync>=std::chrono::milliseconds(16)) {
                const auto delta=now-lastVsync;
                lastVsync+=std::chrono::milliseconds(16)*
                    (delta/std::chrono::milliseconds(16));
                machine.signalReferenceTimer(static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()));
            }
            pollfd fds[2]={{inputOpen?STDIN_FILENO:-1,POLLIN,0},
                           {master,POLLIN,0}};
            // SYNC/CWAI means the guest main CPU has no instructions to
            // execute until an interrupt or external input arrives.
            // Poll with a bounded timeout so the PTM and 16ms timer keep
            // advancing on host time without burning an entire core.
            const bool waiting=running &&
                main->cpuWaitState()!=CSim09::WaitState::Running;
            const int timeout=waiting?1:(running?0:10);
            const int ready=poll(fds,2,timeout);
            if(ready<0 && errno!=EINTR)
                throw std::runtime_error("poll failed");
            if(ready>0 && (fds[0].revents&(POLLIN|POLLHUP))) {
                char buf[512];
                const ssize_t n=read(STDIN_FILENO,buf,sizeof(buf));
                if(n<=0)inputOpen=false;
                else for(ssize_t i=0;i<n;++i) {
                    if(buf[i]=='\n'||buf[i]=='\r') {
                        std::istringstream cmd(line);
                        std::string action;cmd>>action;line.clear();
                        if(action=="q"||action=="quit")stopRequested=1;
                        else if(action=="stop")running=false;
                        else if(action=="c"||action=="continue")running=true;
                        else if(action=="s"||action=="step") {
                            running=false;machine.executeOne();++executed;regs();
                        } else if(action=="r"||action=="regs")regs();
                        else if(!action.empty())std::cout<<"unknown command\n";
                    } else if(buf[i]==127||buf[i]=='\b') {
                        if(!line.empty())line.pop_back();
                    } else if(line.size()<1024)line+=buf[i];
                }
            }
            if(ready>0 && (fds[1].revents&POLLIN)) {
                BYTE buf[256];
                const ssize_t n=read(master,buf,sizeof(buf));
                if(n>0)for(ssize_t i=0;i<n;++i)main->queueSerialInput(buf[i]);
            }
            drain();
            if(running) {
                for(std::uint64_t i=0;i<cycles && !stopRequested;++i) {
                    if(limit && executed>=limit){running=false;break;}
                    if(trace && executed<64)
                        std::cerr<<"main step="<<executed<<" PC=$"<<std::hex
                                 <<main->cpu().pc()<<std::dec<<"\n";
                    machine.executeOne();
                    ++executed;
                    if(main->cpuWaitState()!=CSim09::WaitState::Running)
                        break; // return to timed poll immediately on wait
                }
            }
            drain();
            if(diagnostics && now-lastStats>=std::chrono::seconds(2)) {
                regs();lastStats=now;
            }
        }
        close(slave);
        close(master);
        return 0;
    } catch(const std::exception& ex) {
        std::cerr<<"fm11-headless2: "<<ex.what()<<'\n';
        return 1;
    }
}
