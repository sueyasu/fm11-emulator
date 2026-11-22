#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
struct Case {
    const char* label;
    BYTE opcode;
    WORD adjust;
    unsigned cycles;
    bool immediate;
};
bool check(const Case& t) {
    Main8088Board board(CpuType::I8088);
    board.bus().writeMemory(0x10000,t.opcode);
    if(t.immediate) {
        board.bus().writeMemory(0x10001,BYTE(t.adjust));
        board.bus().writeMemory(0x10002,BYTE(t.adjust>>8));
    }
    // Far return pops IP and CS; imm16 then advances SP.
    board.bus().writeMemory(0x8000,0x34);
    board.bus().writeMemory(0x8001,0x12);
    board.bus().writeMemory(0x8002,0x00);
    board.bus().writeMemory(0x8003,0x20);
    std::array<WORD,8> regs{};
    regs[4]=0x8000;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    cpu.executeOne();
    const WORD expectedSp=WORD(0x8004+(t.immediate?t.adjust:0));
    if(cpu.lastExecutionCycles()!=t.cycles ||
       cpu.ip()!=0x1234 || cpu.cs()!=0x2000 ||
       cpu.reg(4)!=expectedSp) {
        std::cerr<<t.label<<": cycles "<<cpu.lastExecutionCycles()
                 <<", CS:IP "<<cpu.cs()<<":"<<cpu.ip()
                 <<", SP "<<cpu.reg(4)<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    const Case cases[]={
        {"RETF",0xcb,0,33,false},
        {"RETF imm16",0xca,6,34,true},
        {"8088 C9 alias of RETF",0xc9,0,33,false},
        {"8088 C8 alias of RETF imm16",0xc8,6,34,true}
    };
    for(const auto& c:cases)if(!check(c))return 1;

    // The V20 does not share C8/C9 aliases: C9 is LEAVE and
    // C8 is ENTER. Keep the 8088 timing adjustment variant-local.
    Main8088Board v20(CpuType::NECV20);
    v20.bus().writeMemory(0x10000,0xc9);
    std::array<WORD,8> regs{};
    regs[4]=0x8000; regs[5]=0x7000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    v20.bus().writeMemory(0x7000,0x34);
    v20.bus().writeMemory(0x7001,0x12);
    auto& c=static_cast<Main8088Board::Core&>(v20.cpu());
    c.setConformanceState(regs,seg,0,2);
    c.executeOne();
    if(c.ip()!=1 || c.reg(5)!=0x1234 || c.reg(4)!=0x7002)return 2;
    return 0;
}
