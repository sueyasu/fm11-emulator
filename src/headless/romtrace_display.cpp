#include "MainBoard.h"
#include "RomFiles.h"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void usage() {
    std::cerr << "Usage: fm11-display-romtrace [--rom SUBROM.bin] [--instructions N] [--trace]\n"
              << "Default: external/roms/display6809.bin (20480 bytes, mapped B000-FFFF).\n";
}
unsigned long long count(const char* text) {
    std::size_t used=0;
    const auto n=std::stoull(text,&used,0);
    if(text[used]!=0)throw std::invalid_argument("invalid instruction limit");
    return n;
}
}
int main(int argc,char** argv) {
    try {
        std::string path="external/roms/display6809.bin";
        unsigned long long limit=100000;
        bool trace=false;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--rom" && i+1<argc)path=argv[++i];
            else if(arg=="--instructions" && i+1<argc)limit=count(argv[++i]);
            else if(arg=="--trace")trace=true;
            else {usage();return 2;}
        }
        const auto rom=RomFiles::load<DisplaySubsystem::kRomSize>(path);
        MainBoard board;
        board.displaySubsystem().loadRom(rom);
        board.displaySubsystem().setIoWriteObserver([](DisplaySubsystem::IoWriteTrace entry) {
            std::cout<<"I/O WRITE PC="<<std::hex<<std::setw(4)<<std::setfill('0')
                     <<entry.pc<<" ADDR="<<std::setw(4)<<entry.address
                     <<" DATA="<<std::setw(2)<<unsigned(entry.value)
                     <<(entry.fromCpu?" CPU":" HOST")<<"\n";
        });
        board.reset();
        auto& cpu=board.displayCpu();
        std::cout<<"Display reset PC="<<std::hex<<std::setw(4)<<std::setfill('0')
                 <<cpu.pc()<<"\n";
        for(unsigned long long step=0;step<limit;++step) {
            const auto pc=cpu.pc();
            if(trace)
                std::cout<<std::dec<<step<<" PC="<<std::hex<<std::setw(4)
                         <<std::setfill('0')<<pc<<" OP="<<std::setw(2)
                         <<unsigned(board.displaySubsystem().readMemory(pc))<<"\n";
            try {
                if(!cpu.executeOne()) {
                    std::cout<<"Display CPU stopped (HALT/SYNC/CWAI) at PC="
                             <<std::hex<<std::setw(4)<<pc<<"\n";
                    return 0;
                }
            } catch(const std::exception& e) {
                std::cerr<<"Failed at instruction "<<std::dec<<step
                         <<" PC="<<std::hex<<std::setw(4)<<std::setfill('0')
                         <<pc<<": "<<e.what()<<"\n";
                return 1;
            }
        }
        std::cout<<"Instruction limit reached: "<<std::dec<<limit
                 <<" PC="<<std::hex<<std::setw(4)<<std::setfill('0')
                 <<cpu.pc()<<"\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 2;
    }
}
