#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <cstdio>

// Intel 8088: C0h == C2h (RET imm16), C1h == C3h (RET).
// A successor's shift/rotate immediate opcodes must not be used on V20.
namespace {
struct State {WORD ip,sp,cs,flags;};
State execute(BYTE opcode,WORD initialSp,WORD adjustment,WORD target) {
    Main8088Board board(CpuType::I8088);
    board.bus().enableConformanceRam();
    constexpr WORD codeSeg=0x1000,stackSeg=0x2200;
    constexpr std::uint32_t codeBase=0x10000,stackBase=0x22000;
    board.bus().writeMemory(codeBase,opcode);
    board.bus().writeMemory(codeBase+1,BYTE(adjustment));
    board.bus().writeMemory(codeBase+2,BYTE(adjustment>>8));
    board.bus().writeMemory(stackBase+initialSp,BYTE(target));
    board.bus().writeMemory(stackBase+WORD(initialSp+1),BYTE(target>>8));
    std::array<WORD,8> regs{};
    std::array<WORD,4> segments{};
    regs[0]=0x1256; regs[1]=0x1234; regs[2]=0x4567; regs[3]=0x789a;
    regs[4]=initialSp; regs[5]=0x3456; regs[6]=0x5678; regs[7]=0x9abc;
    segments[1]=codeSeg; segments[2]=stackSeg;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    constexpr WORD initialFlags=0x0a97;
    cpu.setConformanceState(regs,segments,0,initialFlags);
    cpu.executeOne();
    for(unsigned i=0;i<8;++i)if(cpu.reg(i)!=regs[i] && i!=4)
        throw std::runtime_error("RET modified a non-SP general register");
    return {cpu.ip(),cpu.reg(4),cpu.cs(),cpu.flags()};
}
bool check(BYTE alias,BYTE canonical,WORD sp,WORD adjustment,WORD target) {
    const State a=execute(alias,sp,adjustment,target);
    const State b=execute(canonical,sp,adjustment,target);
    const WORD wantSp=WORD(sp+2+(alias==0xc0?adjustment:0));
    if(a.ip==target && a.sp==wantSp && a.cs==0x1000 &&
       a.flags==0x0a97 && a.ip==b.ip && a.sp==b.sp &&
       a.cs==b.cs && a.flags==b.flags)return true;
    std::fprintf(stderr,
        "RET alias=%02X SP=%04X adjust=%04X target=%04X: "
        "IP=%04X SP=%04X CS=%04X FLAGS=%04X "
        "expected IP=%04X SP=%04X\n",
        unsigned(alias),unsigned(sp),unsigned(adjustment),unsigned(target),
        unsigned(a.ip),unsigned(a.sp),unsigned(a.cs),unsigned(a.flags),
        unsigned(target),unsigned(wantSp));
    return false;
}
}
int main() {
    for(const WORD sp:{WORD(0x0100),WORD(0xfffe)})
        for(const WORD target:{WORD(0x0000),WORD(0x1234),WORD(0xffff)}) {
            if(!check(0xc1,0xc3,sp,0,target))return 1;
            for(const WORD adjustment:{WORD(0),WORD(2),WORD(0xfffc)})
                if(!check(0xc0,0xc2,sp,adjustment,target))return 2;
        }
    return 0;
}
