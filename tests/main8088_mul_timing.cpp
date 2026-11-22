#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
struct Case {
    const char* name;
    BYTE opcode;
    BYTE modrm;
    WORD ax;
    WORD operand;
    unsigned expected;
    bool memory;
};
bool run(const Case& t) {
    Main8088Board board(CpuType::I8088);
    std::array<WORD,8> regs{};
    regs[0]=t.ax;
    regs[3]=t.memory?0x0200:t.operand; // BX or BL
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
    if(cpu.lastExecutionCycles()!=t.expected) {
        std::cerr<<t.name<<": expected "<<t.expected
                 <<" got "<<cpu.lastExecutionCycles()<<"\n";
        return false;
    }
    const unsigned product=t.opcode==0xf6
        ? unsigned(BYTE(t.ax))*BYTE(t.operand)
        : unsigned(t.ax)*t.operand;
    if(cpu.reg(0)!=WORD(product) ||
       (t.opcode==0xf7 && cpu.reg(2)!=WORD(product>>16))) {
        std::cerr<<t.name<<": incorrect product\n";
        return false;
    }
    return true;
}
}
int main() {
    // The 8088 unsigned multiply microcode tests the multiplier
    // in AL/AX. The 7 or 15 conditional-add branches are modeled
    // independently of the operand fetched from r/m.
    const Case cases[]={
        {"MUL AL=0",0xf6,0xe3,0,3,70,false},
        {"MUL AL=1",0xf6,0xe3,1,3,71,false},
        {"MUL AL=7F",0xf6,0xe3,0x7f,3,77,false},
        {"MUL AL=80",0xf6,0xe3,0x80,3,70,false},
        {"MUL AL=FF",0xf6,0xe3,0xff,3,77,false},
        {"MUL AX=0",0xf7,0xe3,0,3,118,false},
        {"MUL AX=1",0xf7,0xe3,1,3,119,false},
        {"MUL AX=7FFF",0xf7,0xe3,0x7fff,3,133,false},
        {"MUL AX=8000",0xf7,0xe3,0x8000,3,118,false},
        {"MUL AX=FFFF",0xf7,0xe3,0xffff,3,133,false},
        {"MUL memory byte",0xf6,0x27,0x7f,3,88,true},
        {"MUL memory word",0xf7,0x27,0x7fff,3,144,true}
    };
    for(const auto& test:cases)
        if(!run(test))return 1;
    return 0;
}
