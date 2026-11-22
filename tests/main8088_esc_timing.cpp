#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
bool run(const char* name,std::initializer_list<BYTE> code,
         unsigned clocks,WORD expectedIp) {
    Main8088Board board(CpuType::I8088);
    std::uint32_t p=0x10000;
    for(BYTE b:code)board.bus().writeMemory(p++,b);
    std::array<WORD,8> regs{};
    regs[3]=0x0200; // BX for the [BX] form
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=clocks || cpu.ip()!=expectedIp) {
        std::cerr<<name<<" expected "<<clocks<<" clocks, IP="
                 <<expectedIp<<" got "<<cpu.lastExecutionCycles()
                 <<", IP="<<cpu.ip()<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    // ESC D8-DF consumes ModR/M/displacement without a coprocessor.
    // Intel 8088 instruction clock tables: register=2, memory=12+EA.
    for(unsigned op=0xd8;op<=0xdf;++op) {
        const BYTE opcode=BYTE(op);
        if(!run("ESC reg",{opcode,0xc0},2,2))return 1;
        if(!run("ESC [BX]",{opcode,0x07},17,2))return 2;
        if(!run("ESC direct",{opcode,0x06,0x00,0x02},18,4))return 3;
        if(!run("ESC [BX+disp8]",{opcode,0x47,0x01},21,3))return 4;
        if(!run("ES ESC [BX]",{0x26,opcode,0x07},19,3))return 5;
    }
    return 0;
}
