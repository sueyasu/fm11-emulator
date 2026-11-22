#include "Main8088Board.h"
#include <array>
#include <iostream>
#include <cstdint>

namespace {
struct Case {
    const char* name;
    BYTE opcode;
    BYTE modrm;
    WORD ax,dx,operand;
    unsigned clocks;
    bool memory;
    WORD resultAx,resultDx;
};
bool check(const Case& t) {
    Main8088Board board(CpuType::I8088);
    std::array<WORD,8> regs{};
    regs[0]=t.ax;
    regs[2]=t.dx;
    regs[3]=t.memory?0x0200:t.operand; // BX or [BX]
    regs[4]=0x8000;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    board.bus().writeMemory(0x10000,t.opcode);
    board.bus().writeMemory(0x10001,t.modrm);
    if(t.memory) {
        board.bus().writeMemory(0x0200,BYTE(t.operand));
        board.bus().writeMemory(0x0201,BYTE(t.operand>>8));
    }
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=t.clocks ||
       cpu.reg(0)!=t.resultAx || cpu.reg(2)!=t.resultDx) {
        std::cerr<<t.name<<": clocks="<<cpu.lastExecutionCycles()
                 <<" expected="<<t.clocks
                 <<" AX="<<cpu.reg(0)<<" expected="<<t.resultAx
                 <<" DX="<<cpu.reg(2)<<" expected="<<t.resultDx<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    // Model regression, not an independent hardware timing oracle.
    const Case cases[]={
        {"IMUL8 positive",0xf6,0xeb,3,0,4,82,false,12,0},
        {"IMUL8 negative AX",0xf6,0xeb,0xfffd,0,4,88,false,0xfff4,0},
        {"IMUL8 negative source",0xf6,0xeb,3,0,0xfffc,85,false,0xfff4,0},
        {"IMUL8 both negative",0xf6,0xeb,0xfffd,0,0xfffc,88,false,12,0},
        {"IMUL16 positive",0xf7,0xeb,7,0,3,131,false,21,0},
        {"IMUL16 negative",0xf7,0xeb,0xfff9,0,3,137,false,0xffeb,0xffff},
        {"IMUL8 memory",0xf6,0x2f,3,0,4,93,true,12,0},
        {"DIV8 quotient6",0xf6,0xf3,36,0,6,82,false,6,0},
        {"DIV8 quotient7",0xf6,0xf3,42,0,6,83,false,7,0},
        {"DIV16 quotient6",0xf7,0xf3,36,0,6,146,false,6,0},
        {"DIV16 memory",0xf7,0x37,36,0,6,157,true,6,0},
        {"IDIV8 negative dividend",0xf6,0xfb,0xffdc,0,6,105,false,0x00fa,0},
        {"IDIV16 negative divisor",0xf7,0xfb,36,0,0xfffa,168,false,0xfffa,0},
        {"IDIV16 positive",0xf7,0xfb,36,0,6,167,false,6,0},
        {"IDIV8 memory",0xf6,0x3f,36,0,6,114,true,6,0}
    };
    for(const auto& t:cases)
        if(!check(t))return 1;
    // Divide by zero takes INT 0, not the successful-division path.
    Main8088Board b(CpuType::I8088);
    std::array<WORD,8> regs{};regs[3]=0;regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    b.bus().writeMemory(0x10000,0xf6);
    b.bus().writeMemory(0x10001,0xf3);
    b.bus().writeMemory(0,0x00);
    b.bus().writeMemory(1,0x02);
    b.bus().writeMemory(2,0x00);
    b.bus().writeMemory(3,0x10);
    auto& cpu=static_cast<Main8088Board::Core&>(b.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    cpu.executeOne();
    if(cpu.ip()!=0x0200 || cpu.reg(4)!=0x7ffa)return 2;
    return 0;
}
