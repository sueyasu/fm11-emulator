#include "Main8088Board.h"
#include <array>
#include <iostream>

// NEC V20 clock totals: Instruction User's Manual Table 2-8,
// V20/V20HL column. No 8088 EA surcharge for classified operations.
namespace {
bool check(const char* name,std::initializer_list<BYTE> code,
           unsigned expected,WORD cx=3) {
    Main8088Board board(CpuType::NECV20);
    board.bus().enableConformanceRam();
    unsigned offset=0;
    for(BYTE b:code)board.bus().writeMemory(0x10000+offset++,b);
    board.bus().writeMemory(0x30200,0x81);
    board.bus().writeMemory(0x30201,0x40);
    std::array<WORD,8> regs{};
    regs[0]=0x4081;
    regs[1]=cx;
    regs[3]=0x0200;
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0x3000};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=expected || cpu.ip()!=offset) {
        std::cerr<<name<<": clocks="<<cpu.lastExecutionCycles()
                 <<" expected="<<expected<<", IP="<<cpu.ip()
                 <<" expected="<<offset<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    for(unsigned ext=0;ext<8;++ext) {
        const BYTE mem=BYTE((ext<<3)|7u),reg=BYTE(0xc0|(ext<<3));
        // All rotate/shift group extensions follow the same V20
        // timing classes, including the undocumented /6 SHL alias.
        if(!check("D0 reg 1",{0xd0,reg},6))return 1;
        if(!check("D1 mem 1",{0xd1,mem},24))return 2;
        if(!check("D0 mem 1",{0xd0,mem},16))return 3;
        if(!check("D1 reg 1",{0xd1,reg},6))return 4;
        if(!check("D2 reg CL",{0xd2,reg},10))return 5;
        if(!check("D3 mem CL",{0xd3,mem},30))return 6;
        if(!check("D2 mem CL",{0xd2,mem},22))return 7;
        if(!check("D3 reg CL",{0xd3,reg},10))return 8;
        if(!check("C0 reg imm",{0xc0,reg,3},10))return 9;
        if(!check("C1 mem imm",{0xc1,mem,3},30))return 10;
        if(!check("C0 mem imm",{0xc0,mem,3},22))return 11;
        if(!check("C1 reg imm",{0xc1,reg,3},10))return 12;
    }
    for(unsigned ext=0x10;ext<=0x1f;++ext) {
        const BYTE bit=BYTE(ext);
        const BYTE mem=0x07,reg=0xc0;
        const bool word=ext&1,immediate=ext&8;
        const unsigned clocksReg=immediate?5u:4u;
        const unsigned clocksMem=word?(immediate?22u:21u)
                                      :(immediate?14u:13u);
        if(immediate) {
            if(!check("0F bit register immediate",
                       {0x0f,bit,reg,1},clocksReg))return 13;
            if(!check("0F bit memory immediate",
                       {0x0f,bit,mem,1},clocksMem))return 14;
        } else {
            if(!check("0F bit register CL",
                       {0x0f,bit,reg},clocksReg))return 15;
            if(!check("0F bit memory CL",
                       {0x0f,bit,mem},clocksMem))return 16;
        }
    }
    if(!check("ROL4 register",{0x0f,0x28,0xc0},13))return 17;
    if(!check("ROL4 memory",{0x0f,0x28,0x07},28))return 18;
    if(!check("ROR4 register",{0x0f,0x2a,0xc0},17))return 19;
    if(!check("ROR4 memory",{0x0f,0x2a,0x07},32))return 20;
    if(!check("segment override bit",{0x26,0x0f,0x18,0xc0,1},7))return 21;
    return 0;
}
