#include "Main8088Board.h"
#include <array>
#include <cstdio>

// NEC V20 TEST1 r/m8,CL (0F 10): bit selects ZF, clears CF/OF.
// The V20 hardware test suite includes all ModR/M.reg values /0../7.
namespace {
bool check(unsigned extension,bool memoryForm,bool selectedBitSet) {
    Main8088Board board(CpuType::NECV20);
    board.bus().enableConformanceRam();
    constexpr std::uint32_t code=0x10000, target=0x32000;
    constexpr WORD sourceOffset=0x2000;
    auto& bus=board.bus();
    bus.writeMemory(code,0x0f);
    bus.writeMemory(code+1,0x10);
    // mod=11 rm=000 (AL), or mod=00 rm=110 [disp16]
    bus.writeMemory(code+2,BYTE((memoryForm?0x06:0xc0)|(extension<<3)));
    if(memoryForm) {
        bus.writeMemory(code+3,BYTE(sourceOffset));
        bus.writeMemory(code+4,BYTE(sourceOffset>>8));
        bus.writeMemory(target,selectedBitSet?0x08:0x00);
    }
    std::array<WORD,8> regs{};
    std::array<WORD,4> segments{};
    regs[0]=WORD(0x1200|(selectedBitSet?0x08:0x00));
    regs[1]=0x0003; // CL: select bit 3
    regs[4]=0x8000;
    segments[1]=0x1000;
    segments[3]=0x3000;
    constexpr WORD initialFlags=0x0a93; // OF|CF|AF|SF and IF set
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,initialFlags);
    cpu.executeOne();
    constexpr WORD CF=0x0001,OF=0x0800,ZF=0x0040;
    const WORD expectedFlags=WORD((initialFlags&~(CF|OF|ZF))|
                                  (selectedBitSet?0:ZF));
    const WORD expectedIp=memoryForm?5:3;
    if(cpu.reg(0)==regs[0] && cpu.reg(1)==regs[1] &&
       cpu.ip()==expectedIp && cpu.cs()==0x1000 &&
       cpu.flags()==expectedFlags &&
       (!memoryForm || bus.readRam(target)==(selectedBitSet?0x08:0x00)))
        return true;
    std::fprintf(stderr,
        "V20 0F10 /%u %s bitset=%d: IP=%04X FLAGS=%04X "
        "expected IP=%04X FLAGS=%04X\n",
        extension,memoryForm?"memory":"register",int(selectedBitSet),
        unsigned(cpu.ip()),unsigned(cpu.flags()),
        unsigned(expectedIp),unsigned(expectedFlags));
    return false;
}
}
int main() {
    for(unsigned extension=0;extension<8;++extension)
        for(bool memoryForm:{false,true})
            for(bool selectedBitSet:{false,true})
                if(!check(extension,memoryForm,selectedBitSet))return 1;
    return 0;
}
