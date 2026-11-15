#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <cstdio>

// Intel 8088 C8h/C9h mirror CAh/CBh (far returns), whereas NEC V20
// uses C8h/C9h for ENTER/LEAVE.
namespace {
struct State {
    WORD ax,bp,sp,ip,cs,ss,flags;
};
State execute(BYTE opcode,WORD sp,WORD adjustment,WORD targetIp,WORD targetCs) {
    Main8088Board board(CpuType::I8088);
    board.bus().enableConformanceRam();
    constexpr WORD codeSeg=0x1000,stackSeg=0x2400;
    const std::uint32_t code=std::uint32_t(codeSeg)<<4;
    board.bus().writeMemory(code,opcode);
    board.bus().writeMemory(code+1,BYTE(adjustment));
    board.bus().writeMemory(code+2,BYTE(adjustment>>8));
    const auto stackByte=[&](WORD off,std::uint8_t value) {
        board.bus().writeMemory(((std::uint32_t(stackSeg)<<4)+off)&0xfffff,value);
    };
    stackByte(sp,BYTE(targetIp));
    stackByte(WORD(sp+1),BYTE(targetIp>>8));
    stackByte(WORD(sp+2),BYTE(targetCs));
    stackByte(WORD(sp+3),BYTE(targetCs>>8));
    std::array<WORD,8> regs{};
    std::array<WORD,4> seg{};
    regs[0]=0x1234;
    regs[4]=sp;regs[5]=0x5678;
    seg[1]=codeSeg;seg[2]=stackSeg;
    constexpr WORD flags=0x0297;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,flags);
    cpu.executeOne();
    return {cpu.reg(0),cpu.reg(5),cpu.reg(4),cpu.ip(),
            cpu.cs(),cpu.segment(2),cpu.flags()};
}
bool check(BYTE alias,BYTE canonical,WORD sp,WORD adjustment,
           WORD targetIp,WORD targetCs) {
    const auto a=execute(alias,sp,adjustment,targetIp,targetCs);
    const auto b=execute(canonical,sp,adjustment,targetIp,targetCs);
    const WORD expectedSp=WORD(sp+4+(alias==0xc8?adjustment:0));
    if(a.ax==0x1234 && a.bp==0x5678 && a.sp==expectedSp &&
       a.ip==targetIp && a.cs==targetCs && a.ss==0x2400 &&
       a.flags==0x0297 &&
       a.ax==b.ax && a.bp==b.bp && a.sp==b.sp &&
       a.ip==b.ip && a.cs==b.cs && a.ss==b.ss && a.flags==b.flags)
        return true;
    std::fprintf(stderr,
        "8088 %02X vs %02X initial SP=%04X adjust=%04X: "
        "IP=%04X CS=%04X SP=%04X (want %04X:%04X SP=%04X)\n",
        unsigned(alias),unsigned(canonical),unsigned(sp),
        unsigned(adjustment),unsigned(a.ip),unsigned(a.cs),unsigned(a.sp),
        unsigned(targetCs),unsigned(targetIp),unsigned(expectedSp));
    return false;
}
}
int main() {
    for(const WORD sp:{WORD(0x1000),WORD(0xfffe)})
        for(const WORD targetIp:{WORD(0),WORD(0x1234),WORD(0xffff)})
            for(const WORD targetCs:{WORD(0),WORD(0xabcd)}) {
                if(!check(0xc9,0xcb,sp,0,targetIp,targetCs))return 1;
                for(const WORD adjustment:{WORD(0),WORD(2),WORD(0xfffc)})
                    if(!check(0xc8,0xca,sp,adjustment,targetIp,targetCs))
                        return 2;
            }
    return 0;
}
