#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
bool run(const char* label, std::array<BYTE,6> code, unsigned length,
         unsigned cx, WORD flags, unsigned expected, unsigned expectedIp) {
    Main8088Board board(CpuType::I8088);
    std::array<WORD,8> regs{};
    regs[1]=WORD(cx); // CX and CL
    regs[3]=0x2000; // BX for memory operands
    regs[4]=0x8000; // SP
    const std::array<WORD,4> segments{0,0x1000,0,0};
    for(unsigned i=0;i<length;++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,flags);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=expected || cpu.ip()!=expectedIp) {
        std::cerr<<label<<": cycles "<<cpu.lastExecutionCycles()
                 <<" (expected "<<expected<<"), IP "<<cpu.ip()
                 <<" (expected "<<expectedIp<<")\n";
        return false;
    }
    return true;
}
}
int main() {
    // Conditional loop cycles from the Intel iAPX 86/88 timing table.
    if(!run("LOOP taken",{0xe2,0x02},2,2,2,17,4))return 1;
    if(!run("LOOP untaken",{0xe2,0x02},2,1,2,5,2))return 2;
    if(!run("LOOPE taken",{0xe1,0x02},2,2,0x42,18,4))return 3;
    if(!run("LOOPE untaken",{0xe1,0x02},2,2,2,6,2))return 4;
    if(!run("LOOPNE taken",{0xe0,0x02},2,2,2,19,4))return 5;
    if(!run("LOOPNE untaken",{0xe0,0x02},2,2,0x42,7,2))return 6;
    if(!run("JCXZ taken",{0xe3,0x02},2,0,2,16,4))return 7;
    if(!run("JCXZ untaken",{0xe3,0x02},2,1,2,4,2))return 8;
    // D0-D3: implicit one-bit versus variable CL count.
    if(!run("ROL reg 1",{0xd0,0xc0},2,0,2,2,2))return 9;
    if(!run("ROL reg CL=3",{0xd2,0xc0},2,3,2,20,2))return 10;
    if(!run("ROL mem 1",{0xd0,0x07},2,0,2,20,2))return 11;
    if(!run("ROL word mem 1",{0xd1,0x07},2,0,2,28,2))return 12;
    if(!run("ROL mem CL=3",{0xd2,0x07},2,3,2,37,2))return 13;
    if(!run("ROL word mem CL=3",{0xd3,0x07},2,3,2,45,2))return 14;
    if(!run("CALL rel16",{0xe8,0,0},3,0,2,23,3))return 15;
    return 0;
}
