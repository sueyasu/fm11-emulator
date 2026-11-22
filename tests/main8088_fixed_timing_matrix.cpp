#include "Main8088Board.h"
#include <array>
#include <iostream>

// Verify fixed reference clock classes that historically happened to
// match the generic four-clock fallback. This is not a decoder inventory.
namespace {
bool check(const char* name,std::initializer_list<BYTE> bytes,
           unsigned expected,WORD ax=0x0201,WORD flags=2) {
    Main8088Board board(CpuType::I8088);
    std::uint32_t p=0x10000;
    for(BYTE b:bytes)board.bus().writeMemory(p++,b);
    std::array<WORD,8> regs{};
    regs[0]=ax;
    regs[1]=0x1234;
    regs[2]=0x5678;
    regs[3]=0x0200;
    regs[4]=0x8000;
    regs[5]=0x6000;
    regs[6]=0x0010;
    regs[7]=0x0020;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,flags);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=expected ||
       cpu.ip()!=bytes.size()) {
        std::cerr<<name<<": cycles "<<cpu.lastExecutionCycles()
                 <<" expected "<<expected<<" IP "<<cpu.ip()
                 <<" expected "<<bytes.size()<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    // The eight immediate-accumulator ALU families, byte and word.
    // Opcodes 04/05 0C/0D ... 3C/3D use 4 clocks on the 8088.
    for(unsigned op=0;op<8;++op) {
        const BYTE b=BYTE(op*8+4);
        const BYTE w=BYTE(op*8+5);
        if(!check("ALU AL,imm8",{b,0x01},4))return 1;
        if(!check("ALU AX,imm16",{w,0x01,0x00},4))return 2;
    }
    if(!check("TEST AL,imm8",{0xa8,0x01},4))return 3;
    if(!check("TEST AX,imm16",{0xa9,0x01,0x00},4))return 4;

    // XCHG AX,reg encodings are three clocks; this family has no
    // memory operand and must not inherit a ModR/M surcharge.
    for(unsigned op=0x90;op<=0x97;++op)
        if(!check("NOP/XCHG AX,reg",{BYTE(op)},3))return 5;

    // Single-byte flag/convert and decimal adjustments from the
    // published Intel 8088 opcode clock table.
    const struct {BYTE op;unsigned clocks;} fixed[]={
        {0x27,4},{0x2f,4},{0x37,8},{0x3f,8},
        {0x98,2},{0x99,5},{0x9b,4},{0x9e,4},{0x9f,4},
        {0xf5,2},{0xf8,2},{0xf9,2},{0xfa,2},{0xfb,2},
        {0xfc,2},{0xfd,2}
    };
    for(const auto& t:fixed)
        if(!check("fixed one-byte",{t.op},t.clocks))return 6;

    if(!check("AAM",{0xd4,0x0a},83))return 7;
    if(!check("AAD",{0xd5,0x0a},60))return 8;
    // A single segment prefix retains the existing two-clock surcharge.
    if(!check("ES TEST AL,imm8",{0x26,0xa8,0x01},6))return 9;
    if(!check("ES ADD AX,imm16",{0x26,0x05,0x01,0x00},6))return 10;
    return 0;
}
