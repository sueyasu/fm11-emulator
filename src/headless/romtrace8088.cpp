#include "Main8088Board.h"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned long long parseCount(const char* s)
{
    std::size_t end=0;
    const auto n=std::stoull(s,&end,0);
    if(s[end]!=0)throw std::invalid_argument("invalid instruction limit");
    return n;
}
void usage()
{
    std::cerr<<"Usage: fm11-8088-romtrace --rom ROM.bin [--cpu 8088|v20] [--instructions N] [--trace]\n";
    std::cerr<<"ROM must be a raw 4096-byte FM-11 8088 boot-ROM dump.\n";
}
}
int main(int argc,char** argv)
{
    try {
        std::string path;
        CpuType type=CpuType::I8088;
        unsigned long long limit=100000;
        bool trace=false;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--rom" && i+1<argc)path=argv[++i];
            else if(arg=="--cpu" && i+1<argc) {
                const std::string value=argv[++i];
                if(value=="8088")type=CpuType::I8088;
                else if(value=="v20")type=CpuType::NECV20;
                else {usage();return 2;}
            } else if(arg=="--instructions" && i+1<argc)limit=parseCount(argv[++i]);
            else if(arg=="--trace")trace=true;
            else {usage();return 2;}
        }
        if(path.empty()) {usage();return 2;}
        std::ifstream in(path,std::ios::binary);
        if(!in)throw std::runtime_error("cannot open ROM: "+path);
        const std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                                      std::istreambuf_iterator<char>());
        if(bytes.size()!=Main8088Bus::kRomSize)
            throw std::runtime_error("8088 ROM must contain exactly 4096 bytes");
        Main8088Bus::Rom rom{};
        for(std::size_t i=0;i<rom.size();++i)rom[i]=static_cast<BYTE>(bytes[i]);
        Main8088Board board(type);
        board.bus().loadBootRom(rom);
        board.reset();
        auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
        std::cout<<"Reset PC="<<std::hex<<std::setw(5)<<std::setfill('0')<<cpu.pc()
                 <<" CPU="<<(type==CpuType::I8088?"8088":"v20")<<"\n";
        for(unsigned long long step=0;step<limit;++step) {
            const auto pc=cpu.pc();
            const auto op=board.bus().readMemory(pc);
            if(trace)std::cout<<std::dec<<step<<" PC="<<std::hex<<std::setw(5)<<pc
                               <<" OP="<<std::setw(2)<<unsigned(op)
                               <<" AX="<<std::setw(4)<<cpu.reg(0)
                               <<" CX="<<std::setw(4)<<cpu.reg(1)
                               <<" DX="<<std::setw(4)<<cpu.reg(2)
                               <<" BX="<<std::setw(4)<<cpu.reg(3)
                               <<" SP="<<std::setw(4)<<cpu.reg(4)<<"\n";
            try {cpu.executeOne();}
            catch(const std::exception& e) {
                std::cerr<<"Stopped at step "<<std::dec<<step
                         <<" PC="<<std::hex<<std::setw(5)<<std::setfill('0')<<pc
                         <<" opcode="<<std::setw(2)<<unsigned(op)
                         <<": "<<e.what()<<"\n";
                return 1;
            }
            if(cpu.halted()) {
                std::cout<<"HLT after "<<std::dec<<step+1<<" instruction(s)\n";
                return 0;
            }
        }
        std::cout<<"Instruction limit reached: "<<std::dec<<limit
                 <<" PC="<<std::hex<<std::setw(5)<<std::setfill('0')<<cpu.pc()<<"\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 2;
    }
}
