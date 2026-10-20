#include "Machine.h"
#include "Main6809Board.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
struct Terminal {
    termios saved{};
    bool restore=false;
    Terminal() {
        if(isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO,&saved)==0) {
            termios raw=saved;
            cfmakeraw(&raw);
            raw.c_oflag=saved.c_oflag;
            if(tcsetattr(STDIN_FILENO,TCSANOW,&raw)==0)restore=true;
        }
    }
    ~Terminal() {if(restore)tcsetattr(STDIN_FILENO,TCSANOW,&saved);}
};
struct Pty {
    int master=-1,slave=-1;
    std::vector<BYTE> pending;
    explicit Pty() {
        master=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
        if(master<0 || grantpt(master)!=0 || unlockpt(master)!=0)
            throw std::runtime_error("cannot create serial PTY");
        const char* path=ptsname(master);
        if(!path)throw std::runtime_error("cannot resolve serial PTY");
        slave=open(path,O_RDWR|O_NOCTTY);
        if(slave<0)throw std::runtime_error("cannot open serial PTY");
        termios raw{};
        if(tcgetattr(slave,&raw)==0) {
            cfmakeraw(&raw);
            tcsetattr(slave,TCSANOW,&raw);
        }
        std::cerr<<"Serial /T1 PTY: "<<path<<"\n";
    }
    ~Pty() {if(slave>=0)close(slave);if(master>=0)close(master);}
    void service(Main6809Board& board) {
        BYTE c;
        while(board.takeSerialOutput(c))pending.push_back(c);
        if(!pending.empty()) {
            const ssize_t n=write(master,pending.data(),pending.size());
            if(n>0)pending.erase(pending.begin(),pending.begin()+n);
            else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR && errno!=EIO)
                throw std::runtime_error("serial PTY write");
        }
        BYTE input[256];
        for(;;) {
            const ssize_t n=read(master,input,sizeof(input));
            if(n>0)for(ssize_t i=0;i<n;++i)board.queueSerialInput(input[i]);
            else if(n<0 && errno==EINTR)continue;
            else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EIO)
                throw std::runtime_error("serial PTY read");
            else break;
        }
    }
};
std::vector<BYTE> readFile(const std::string& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot open image: "+path);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
void usage() {
    std::cout<<"fm11-inkey-test --main-rom FILE --floppy-d88 FILE [--rom FILE]"
               " [--boot-dip HEX] [--no-trace] [--trace-sub-after-fd05 N]\n"
               "SDL-free real sub-ROM INKEY2 probe. Host stdin -> sub keyboard queue.\n"
               "Use a terminal for raw input (Enter, Backspace and ASCII).\n"
               "After host input, up to 1024 additional INKEY2/AFEC/AFED events are logged.\n"
               "Ctrl-] exits; /T1 serial is available through a separate PTY.\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string mainRom,floppy,rom="external/roms/display6809.bin";
        unsigned bootDip=0x9f;
        bool trace=true;
        unsigned subTraceLimit=0;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--help"){usage();return 0;}
            if(arg=="--no-trace"){trace=false;continue;}
            if(arg=="--trace-sub-after-fd05" && i+1<argc) {
                subTraceLimit=static_cast<unsigned>(std::stoul(argv[++i]));
                if(subTraceLimit>100000)throw std::invalid_argument("sub trace limit too large");
                continue;
            }
            if(i+1>=argc)throw std::invalid_argument("missing value for "+arg);
            const std::string value=argv[++i];
            if(arg=="--main-rom")mainRom=value;
            else if(arg=="--floppy-d88")floppy=value;
            else if(arg=="--rom")rom=value;
            else if(arg=="--boot-dip")bootDip=std::stoul(value,nullptr,0);
            else throw std::invalid_argument("unknown option: "+arg);
        }
        if(mainRom.empty() || floppy.empty() || bootDip>255) {
            usage();return 2;
        }
        Machine machine;
        machine.loadDisplayRomFile(rom);
        auto cpu=std::make_unique<Main6809Board>(CpuType::MC6809);
        cpu->loadBootRomFile(mainRom);
        cpu->setCpuClockHz(2000000);
        Main6809Board* board=cpu.get();
        machine.addBoard(std::move(cpu));
        board->loadFloppyD88(readFile(floppy));
        board->setFloppyDrive(0);
        board->setBootDip(static_cast<BYTE>(bootDip));
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        auto hostInputSeen=std::make_shared<bool>(false);
        if(trace || subTraceLimit) {
            auto remaining=std::make_shared<unsigned>(0);
            auto releaseCount=std::make_shared<unsigned>(0);
            // One ordered log for main FD05, sub AFE0 and shared RAM accesses.
            auto seq=std::make_shared<std::uint64_t>(0);
            auto postCount=std::make_shared<unsigned>(0);
            auto tracePost=[hostInputSeen,postCount]() {
                return *hostInputSeen && *postCount<1024;
            };
            machine.setSubKeyboardReadObserver(
                [seq,tracePost,postCount](std::uint16_t address,BYTE value,std::uint16_t pc) {
                    if(!tracePost())return;
                    ++*postCount;
                    std::cerr<<"EV "<<++*seq<<" SUB R $"<<std::hex<<std::uppercase
                             <<std::setw(4)<<std::setfill('0')<<address
                             <<"=$"<<std::setw(2)<<unsigned(value)
                             <<" PC=$"<<std::setw(4)<<pc<<std::dec<<"\n";
                });
            auto logState=[&machine,seq](const char* event) {
                std::cerr<<"EV "<<++*seq<<" "<<event
                         <<" subPC=$"<<std::hex<<std::uppercase
                         <<std::setw(4)<<std::setfill('0')<<machine.displayCpuPc()
                         <<" FD05=$"<<std::setw(2)<<unsigned(
                             (machine.displayHalted() || machine.displayBusyStatus())?0x80:0)
                         <<" HALT="<<machine.displayHalted()
                         <<" BUSY="<<(machine.displayBusyStatus()!=0)
                         <<" FC82=$"<<std::setw(2)<<unsigned(machine.displaySharedByte(2))
                         <<" FC83=$"<<std::setw(2)<<unsigned(machine.displaySharedByte(3))
                         <<" FC84=$"<<std::setw(2)<<unsigned(machine.displaySharedByte(4))
                         <<std::dec<<"\n";
            };
            if(subTraceLimit) {
                machine.setSubInstructionObserver([&machine,seq,remaining](std::uint16_t pc) {
                    if(!*remaining)return;
                    std::cerr<<"EV "<<++*seq<<" SUB STEP PC=$"<<std::hex<<std::uppercase
                             <<std::setw(4)<<std::setfill('0')<<pc
                             <<" AFE0=$"<<std::setw(2)<<unsigned(machine.subAfe0Status())
                             <<" FD05=$"<<std::setw(2)<<unsigned(machine.mainFd05Status())
                             <<" FC82=$"<<std::setw(2)<<unsigned(machine.displaySharedByte(2))
                             <<std::dec<<"\n";
                    --*remaining;
                });
                machine.setSubAfe0ReadObserver([seq,remaining](BYTE value,std::uint16_t pc) {
                    if(!*remaining)return;
                    std::cerr<<"EV "<<++*seq<<" SUB AFE0 R=$"<<std::hex<<std::uppercase
                             <<std::setw(2)<<std::setfill('0')<<unsigned(value)
                             <<" PC=$"<<std::setw(4)<<pc<<std::dec<<"\n";
                });
            }
            auto fd05Count=std::make_shared<unsigned>(0);
            machine.setFd05WriteObserver([&machine,seq,fd05Count,remaining,releaseCount,subTraceLimit](BYTE value) {
                if(subTraceLimit && (value&0x80)==0 && *releaseCount<4) {
                    ++*releaseCount;
                    *remaining=subTraceLimit;
                    std::cerr<<"EV "<<++*seq<<" FD05 RELEASE #"<<*releaseCount
                             <<" subPC=$"<<std::hex<<std::uppercase
                             <<std::setw(4)<<std::setfill('0')<<machine.displayCpuPc()
                             <<" FD05=$"<<std::setw(2)<<unsigned(machine.mainFd05Status())
                             <<" AFE0=$"<<std::setw(2)<<unsigned(machine.subAfe0Status())
                             <<" FC82=$"<<std::setw(2)<<unsigned(machine.displaySharedByte(2))
                             <<" FC83=$"<<std::setw(2)<<unsigned(machine.displaySharedByte(3))
                             <<" FC84=$"<<std::setw(2)<<unsigned(machine.displaySharedByte(4))
                             <<std::dec<<"\n";
                }
                if(++*fd05Count>256)return;
                std::cerr<<"EV "<<++*seq<<" MAIN FD05 W=$"
                         <<std::hex<<std::uppercase<<std::setw(2)<<std::setfill('0')
                         <<unsigned(value)<<" subPC=$"<<std::setw(4)
                         <<machine.displayCpuPc()<<" BUSY="
                         <<unsigned(machine.displayBusyStatus())<<std::dec<<"\n";
            });
            auto afe0Count=std::make_shared<unsigned>(0);
            machine.setDisplayIoWriteObserver([&machine,seq,afe0Count](DisplaySubsystem::IoWriteTrace e) {
                if(e.address!=0xafe0 || ++*afe0Count>256)return;
                std::cerr<<"EV "<<++*seq<<" SUB AFE0 W=$"
                         <<std::hex<<std::uppercase<<std::setw(2)<<std::setfill('0')
                         <<unsigned(e.value)<<" PC=$"<<std::setw(4)<<e.pc
                         <<" subPC=$"<<std::setw(4)<<machine.displayCpuPc()
                         <<std::dec<<"\n";
            });
            logState("START");
            auto active=std::make_shared<bool>(false);
            auto n=std::make_shared<unsigned>(0);
            machine.setDisplaySharedTraceObserver(
                [active,n,seq,&machine,tracePost,postCount](DisplaySubsystem::SharedTrace e) {
                    if(e.mainCpu && e.write && e.offset==2 && e.value==0x2d) {
                        *active=true;
                        ++*n;
                        if(*n<=128 || tracePost()) {
                            if(*n>128)++*postCount;
                            std::cerr<<"EV "<<++*seq<<" INKEY2 #"<<*n
                            <<" request subPC=$"<<std::hex<<std::uppercase
                            <<std::setw(4)<<std::setfill('0')<<machine.displayCpuPc()
                            <<std::dec<<"\n";
                        }
                    }
                    if(!*active)return;
                    if((e.offset==3 || e.offset==4) && (*n<=128 || tracePost())) {
                        if(*n>128)++*postCount;
                        std::cerr<<"EV "<<++*seq<<" INKEY2 #"<<*n<<" "<<(e.mainCpu?"MAIN":"SUB")
                                 <<(e.write?" W":" R")<<" FC8"<<unsigned(e.offset)
                                 <<"=$"<<std::hex<<std::uppercase<<std::setw(2)
                                 <<std::setfill('0')<<unsigned(e.value)<<std::dec<<"\n";
                    }
                    if(e.mainCpu && !e.write && e.offset==4)*active=false;
                });
        }
        Terminal terminal;
        Pty pty;
        std::cerr<<"Real display ROM, no INKEY2 HLE. Type into this terminal; Ctrl-] quits.\n";
        auto last=Clock::now();
        bool running=true,inputOpen=true;
        while(running) {
            const bool waiting=board->cpuWaitState()!=CSim09::WaitState::Running;
            pollfd fds[2]={{inputOpen?STDIN_FILENO:-1,POLLIN,0},{pty.master,POLLIN,0}};
            const int ret=poll(fds,2,waiting?1:0);
            if(ret<0 && errno!=EINTR)throw std::runtime_error("poll failed");
            if(fds[0].revents&(POLLIN|POLLHUP)) {
                unsigned char bytes[256];
                const ssize_t got=read(STDIN_FILENO,bytes,sizeof(bytes));
                if(got==0)inputOpen=false;
                else if(got>0)for(ssize_t i=0;i<got;++i) {
                    const unsigned char ch=bytes[i];
                    if(ch==0x1d){running=false;break;}
                    const BYTE key=ch==0x7f?0x08:ch==0x0a?0x0d:ch;
                    machine.enqueueDisplayKeyCode(key);
                    *hostInputSeen=true;
                    std::cerr<<"host key $"<<std::hex<<std::uppercase
                             <<std::setw(2)<<std::setfill('0')<<unsigned(key)
                             <<std::dec<<"\n";
                }
            }
            pty.service(*board);
            const auto now=Clock::now();
            const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(now-last).count();
            last=now;
            if(waiting && elapsed>0)
                machine.advanceTimeNanoseconds(static_cast<std::uint64_t>(
                    std::min<std::int64_t>(elapsed,50000000)));
            for(unsigned i=0;i<2000 && running;++i) {
                machine.executeOne();
                if(board->cpuWaitState()!=CSim09::WaitState::Running)break;
            }
            pty.service(*board);
        }
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"fm11-inkey-test: "<<e.what()<<"\n";
        return 1;
    }
}
