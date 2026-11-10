// FM-11 CP/M-80 ROM boot runner. The 6809 starts at its actual ROM
// reset vector, loads the on-disk IPL, and yields to the optional Z80 via FD05.
#include "Machine.h"
#include "Main6809Board.h"
#include "MainZ80Board.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace {
volatile std::sig_atomic_t stopping=0;
void stop(int) {stopping=1;}

std::uint64_t parseNumber(const char* value) {
    const std::string text(value);
    std::size_t end=0;
    const auto result=std::stoull(text,&end,0);
    if(end!=text.size())throw std::invalid_argument("invalid numeric argument");
    return result;
}
std::vector<BYTE> readDisk(const std::string& filename) {
    std::ifstream file(filename,std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error("cannot open disk: "+filename);
    const auto size=file.tellg();
    if(size<0 || size>4*1024*1024)
        throw std::runtime_error("unsupported floppy disk image size");
    file.seekg(0);
    std::vector<BYTE> image(static_cast<std::size_t>(size));
    if(!file.read(reinterpret_cast<char*>(image.data()),size))
        throw std::runtime_error("cannot read floppy disk: "+filename);
    return image;
}
void usage() {
    std::cerr
       <<"usage: fm11-cpm80 --main-rom FILE --floppy-d88 FILE"
       <<" [--display-rom FILE] [--ank-rom FILE]"
       <<" [--cpu 6809|6309] [--clock HZ] [--z80-clock HZ]"
       <<" [--instructions COUNT] [--trace-handoff]\n"
       <<"  Boots using the 6809 ROM and disk IPL; no CP/M binary is injected.\n"
       <<"  CP/M local console requires --display-rom and --ank-rom.\n"
       <<"  The serial 8251 is exposed on the printed PTY path.\n";
}
}

int main(int argc,char** argv) {
    try {
        std::string mainRom,displayRom,ankRom,floppy,cpu="6809";
        std::uint64_t clock6809=2000000,clockZ80=4000000,limit=0;
        bool trace=false;
        for(int i=1;i<argc;++i) {
            const std::string arg(argv[i]);
            if(arg=="--help") {usage();return 0;}
            if(arg=="--trace-handoff") {trace=true;continue;}
            if(i+1>=argc) {usage();return 2;}
            const char* value=argv[++i];
            if(arg=="--main-rom")mainRom=value;
            else if(arg=="--display-rom")displayRom=value;
            else if(arg=="--ank-rom")ankRom=value;
            else if(arg=="--floppy-d88")floppy=value;
            else if(arg=="--cpu")cpu=value;
            else if(arg=="--clock")clock6809=parseNumber(value);
            else if(arg=="--z80-clock")clockZ80=parseNumber(value);
            else if(arg=="--instructions")limit=parseNumber(value);
            else {usage();return 2;}
        }
        if(mainRom.empty() || floppy.empty() || (cpu!="6809" && cpu!="6309")) {
            usage();return 2;
        }
        Machine machine;
        auto primary=std::make_unique<Main6809Board>(
            cpu=="6809"?CpuType::MC6809:CpuType::HD6309);
        auto* main=primary.get();
        primary->loadBootRomFile(mainRom);
        primary->setCpuClockHz(clock6809);
        machine.addBoard(std::move(primary));
        auto optional=std::make_unique<MainZ80Board>();
        auto* z80=optional.get();
        machine.addBoard(std::move(optional));
        machine.enableFd05Selection(true);
        machine.setZ80ClockHz(clockZ80);
        // Mount after attaching boards, so the disk belongs to MainBoard.
        main->loadFloppyD88(readDisk(floppy));
        if(!displayRom.empty()) {
            machine.loadDisplayRomFile(displayRom);
            machine.setDisplayCpuClockHz(2000000);
        }
        if(!ankRom.empty())machine.loadAnkRomFile(ankRom);
        machine.reset();

        // Leave the serial port on a PTY even when using the local display.
        const int master=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
        if(master<0 || grantpt(master)!=0 || unlockpt(master)!=0)
            throw std::runtime_error("cannot create PTY");
        const char* name=ptsname(master);
        if(!name)throw std::runtime_error("cannot resolve PTY");
        const int slave=open(name,O_RDWR|O_NOCTTY);
        if(slave<0)throw std::runtime_error("cannot open PTY slave");
        termios tty{};
        if(tcgetattr(slave,&tty)==0) {
            cfmakeraw(&tty);
            tcsetattr(slave,TCSANOW,&tty);
        }
        std::cout<<"FM-11 CP/M-80 ROM boot\nSerial PTY: "<<name
                 <<"\nStarting main 6809 CPU\n"<<std::flush;
        std::signal(SIGINT,stop);
        std::signal(SIGTERM,stop);
        CpuType previous=CpuType::MC6809;
        std::vector<BYTE> output;
        std::uint64_t executed=0;
        while(!stopping && (!limit || executed<limit)) {
            for(int i=0;i<512 && !stopping && (!limit || executed<limit);++i) {
                const auto* active=machine.activeBoard();
                if(!active)throw std::runtime_error("no active CPU");
                if(trace && active->cpu().type()!=previous) {
                    std::cerr<<"CPU switch to "<<
                        (active->cpu().type()==CpuType::Z80?"Z80":"6809")
                        <<" PC=$"<<std::hex<<active->cpu().pc()<<std::dec
                        <<" after "<<executed<<" instructions\n";
                    previous=active->cpu().type();
                }
                machine.executeOne();
                ++executed;
            }
            BYTE b;
            while(main->takeSerialOutput(b))output.push_back(b);
            if(!output.empty()) {
                const auto n=write(master,output.data(),output.size());
                if(n>0)output.erase(output.begin(),output.begin()+n);
                else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK &&
                        errno!=EINTR && errno!=EIO)
                    throw std::runtime_error("PTY write failed");
            }
            BYTE input[256];
            for(;;) {
                const auto n=read(master,input,sizeof(input));
                if(n>0)for(ssize_t i=0;i<n;++i)main->queueSerialInput(input[i]);
                else if(n<0 && errno==EINTR)continue;
                else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK &&
                        errno!=EIO)
                    throw std::runtime_error("PTY read failed");
                else break;
            }
        }
        if(limit)std::cout<<"Instruction limit reached: "<<executed
                         <<" active="<<(machine.activeBoard() &&
                            machine.activeBoard()->cpu().type()==CpuType::Z80
                            ?"Z80":"6809")
                         <<" PC=$"<<std::hex
                         <<(machine.activeBoard()?machine.activeBoard()->cpu().pc():0)
                         <<std::dec<<"\n";
        // FloppyDevice writes to the mounted media in memory; persist those
        // writes only after an orderly exit, never for a rejected boot image.
        if(main->floppyDirty()) {
            const auto image=main->floppyImage();
            std::ofstream out(floppy,std::ios::binary|std::ios::trunc);
            if(!out || !out.write(reinterpret_cast<const char*>(image.data()),image.size()))
                throw std::runtime_error("cannot save modified floppy image");
        }
        close(slave);
        close(master);
        (void)z80;
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"fm11-cpm80: "<<e.what()<<"\n";
        return 1;
    }
}
