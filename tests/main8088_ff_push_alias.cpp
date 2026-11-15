#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <cstdio>

// Intel 8088 undocumented FF /7 aliases FF /6 (PUSH r/m16).
// Compare both encodings in register and memory modes, including SP.
namespace {
struct Result {
    WORD ax,sp,ip,cs,flags,stacked;
};
Result execute(unsigned group,unsigned rm,bool memoryForm,WORD sp) {
    Main8088Board board(CpuType::I8088);
    board.bus().enableConformanceRam();
    constexpr std::uint32_t code=0x10000;
    constexpr WORD ss=0x2200,ds=0x3000;
    constexpr WORD offset=0x1234;
    const BYTE modrm=BYTE((memoryForm?0x06:0xc0)|(group<<3)|rm);
    // Memory form uses the special mod=00 r/m=110, disp16 operand.
    // Register form uses mod=11, r/m selects the general register.
    board.bus().writeMemory(code,0xff);
    board.bus().writeMemory(code+1,modrm);
    if(memoryForm) {
        board.bus().writeMemory(code+2,BYTE(offset));
        board.bus().writeMemory(code+3,BYTE(offset>>8));
        const std::uint32_t address=std::uint32_t(ds)*16+offset;
        board.bus().writeMemory(address,0x78);
        board.bus().writeMemory(address+1,0x56);
    }
    std::array<WORD,8> regs={
        WORD(0x1234),WORD(0x2345),WORD(0x3456),WORD(0x4567),
        sp,WORD(0x6789),WORD(0x789a),WORD(0x89ab)
    };
    std::array<WORD,4> seg={WORD(0x4000),WORD(0x1000),ss,ds};
    constexpr WORD flags=0x0297;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,flags);
    cpu.executeOne();
    const WORD newSp=WORD(sp-2);
    const std::uint32_t stackAddr=(std::uint32_t(ss)*16+newSp)&0xfffff;
    const WORD stacked=WORD(board.bus().readRam(stackAddr)|
                         (WORD(board.bus().readRam((stackAddr+1)&0xfffff))<<8));
    return {cpu.reg(0),cpu.reg(4),cpu.ip(),cpu.cs(),cpu.flags(),stacked};
}
bool check(unsigned rm,bool memoryForm,WORD sp) {
    const Result alias=execute(7,rm,memoryForm,sp);
    const Result canonical=execute(6,rm,memoryForm,sp);
    const WORD expected=memoryForm ? WORD(0x5678) :
        (rm==4 ? WORD(sp-2) :
         std::array<WORD,8>{WORD(0x1234),WORD(0x2345),WORD(0x3456),
                            WORD(0x4567),sp,WORD(0x6789),
                            WORD(0x789a),WORD(0x89ab)}[rm]);
    const WORD nextIp=memoryForm?4:2;
    if(alias.sp==WORD(sp-2) && alias.stacked==expected &&
       alias.ip==nextIp && alias.cs==0x1000 && alias.flags==0x0297 &&
       alias.ax==0x1234 &&
       alias.sp==canonical.sp && alias.stacked==canonical.stacked &&
       alias.ip==canonical.ip && alias.cs==canonical.cs &&
       alias.flags==canonical.flags && alias.ax==canonical.ax)return true;
    std::fprintf(stderr,"FF /7 rm=%u mem=%d SP=%04X: pushed=%04X "
                 "expected=%04X nextSP=%04X IP=%04X\n",rm,int(memoryForm),
                 unsigned(sp),unsigned(alias.stacked),unsigned(expected),
                 unsigned(alias.sp),unsigned(alias.ip));
    return false;
}
}
int main() {
    for(const WORD sp:{WORD(0x8000),WORD(0x0000)}) {
        for(unsigned rm=0;rm<8;++rm)
            if(!check(rm,false,sp))return 1;
        if(!check(6,true,sp))return 2;
    }
    return 0;
}
