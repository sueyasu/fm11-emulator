#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
bool check(const char* name, std::array<BYTE,8> instruction,
           unsigned length, unsigned expected) {
    Main8088Board board(CpuType::I8088);
    std::array<WORD,8> regs{};
    regs[0]=0x0001; // AX
    regs[3]=0x0200; // BX, used for [BX]
    regs[4]=0x8000; // SP
    const std::array<WORD,4> segments{0,0x1000,0,0};
    for(unsigned i=0;i<length;++i)
        board.bus().writeMemory(0x10000+i,instruction[i]);
    // [BX] contains near target, and [BX+2] far target segment.
    board.bus().writeMemory(0x0200,0x05);
    board.bus().writeMemory(0x0201,0x00);
    board.bus().writeMemory(0x0202,0x00);
    board.bus().writeMemory(0x0203,0x10);
    board.bus().writeMemory(0x8000,0x01);
    board.bus().writeMemory(0x8001,0x00);
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,0x0002);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=expected) {
        std::cerr<<name<<": expected "<<expected
                 <<" got "<<cpu.lastExecutionCycles()<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    struct Case {
        const char* name;
        std::array<BYTE,8> bytes;
        unsigned length;
        unsigned clocks;
    };
    const Case cases[]={
        {"FE inc reg", {0xfe,0xc0},2,3},
        {"FE dec reg", {0xfe,0xc8},2,3},
        {"FE inc mem8", {0xfe,0x07},2,28},
        {"FE dec mem", {0xfe,0x0f},2,28},
        {"FF inc reg", {0xff,0xc0},2,3},
        {"FF dec reg", {0xff,0xc8},2,3},
        {"FF inc mem16", {0xff,0x07},2,28},
        {"FF dec mem", {0xff,0x0f},2,28},
        {"FF near call reg", {0xff,0xd0},2,24},
        {"FF near call mem", {0xff,0x17},2,34},
        {"FF near call reg BX", {0xff,0xd3},2,24},
        {"FF far call mem", {0xff,0x1f},2,58},
        {"FF near jmp reg", {0xff,0xe0},2,11},
        {"FF near jmp mem", {0xff,0x27},2,23},
        {"FF far jmp mem", {0xff,0x2f},2,29},
        {"FF push reg", {0xff,0xf0},2,15},
        {"FF push reg alias", {0xff,0xf8},2,15},
        {"FF push mem alias", {0xff,0x3f},2,29},
        {"FF push mem", {0xff,0x37},2,29},
        {"POP reg rm", {0x8f,0xc0},2,12},
        {"POP mem rm", {0x8f,0x07},2,30},
        {"LEA bx", {0x8d,0x07},2,7},
        {"LES bx", {0xc4,0x07},2,29},
        {"LDS bx", {0xc5,0x07},2,29},
        {"TEST reg", {0x84,0xc0},2,3},
        {"TEST mem8", {0x84,0x07},2,18},
        {"TEST mem16", {0x85,0x07},2,18},
        {"XCHG reg", {0x86,0xc0},2,4},
        {"XCHG mem8", {0x86,0x07},2,30},
        {"XCHG mem16", {0x87,0x07},2,30},
        {"PUSHF", {0x9c},1,14},
        {"POPF", {0x9d},1,12},
        {"IN AL,imm", {0xe4,0x30},2,14},
        {"IN AX,DX", {0xed},1,12},
        {"OUT imm,AL", {0xe6,0x30},2,14},
        {"OUT DX,AX", {0xef},1,12},
        {"CALL far imm", {0x9a,0x04,0,0,0x10},5,36},
        {"RETN", {0xc3},1,20},
        {"RETF", {0xcb},1,33},
        {"RETN imm", {0xc2,0x02,0},3,24},
        {"RETF imm", {0xca,0x02,0},3,34},
        {"RETF alias C9", {0xc9},1,33},
        {"RETF imm alias C8", {0xc8,0x02,0},3,34}
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
        if(!check(cases[i].name,cases[i].bytes,cases[i].length,cases[i].clocks))
            return int(i+1);
    return 0;
}
