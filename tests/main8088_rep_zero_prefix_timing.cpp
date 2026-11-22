#include "Main8088Board.h"
#include <array>
#include <iostream>
#include <vector>

// The REP startup time is charged once even with CX=0; extra
// non-REP prefixes have their own two-clock decode cost.
namespace {
bool check(BYTE repeat,BYTE opcode,unsigned extraPrefixes) {
    Main8088Board board(CpuType::I8088);
    std::vector<BYTE> instruction{};
    if(extraPrefixes>=1)instruction.push_back(0x26); // ES override
    if(extraPrefixes>=2)instruction.push_back(0xf0); // LOCK
    instruction.push_back(repeat);
    instruction.push_back(opcode);
    for(unsigned i=0;i<instruction.size();++i)
        board.bus().writeMemory(0x10000+i,instruction[i]);
    board.bus().writeMemory(0x0200,0x41);
    board.bus().writeMemory(0x0300,0x42);
    std::array<WORD,8> regs{};
    regs[0]=0x1234;
    regs[1]=0; // CX = zero, so no string transfer is permitted
    regs[4]=0x8000;
    regs[6]=0x0200;
    regs[7]=0x0300;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    cpu.executeOne();
    const unsigned expectedCycles=9u+2u*extraPrefixes;
    if(cpu.lastExecutionCycles()!=expectedCycles ||
       cpu.ip()!=instruction.size() ||
       cpu.reg(0)!=0x1234 || cpu.reg(1)!=0 ||
       cpu.reg(6)!=0x0200 || cpu.reg(7)!=0x0300 ||
       board.bus().readMemory(0x0200)!=0x41 ||
       board.bus().readMemory(0x0300)!=0x42) {
        std::cerr<<"REP zero-count opcode="<<std::hex<<unsigned(opcode)
                 <<" repeat="<<unsigned(repeat)<<std::dec
                 <<" extras="<<extraPrefixes<<" cycles="
                 <<cpu.lastExecutionCycles()<<" expected="<<expectedCycles
                 <<" IP="<<cpu.ip()<<" expected="<<instruction.size()<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    const BYTE stringOps[]={0xa4,0xa5,0xa6,0xa7,0xaa,
                             0xab,0xac,0xad,0xae,0xaf};
    for(BYTE repeat:{BYTE(0xf2),BYTE(0xf3)}) {
        for(BYTE opcode:stringOps) {
            for(unsigned extras=0;extras<=2;++extras) {
                if(!check(repeat,opcode,extras))return 1;
            }
        }
    }
    return 0;
}
