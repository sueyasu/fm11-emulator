#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <iostream>

// Reference timing: Intel 8088 short Jcc = 16 clocks if taken,
// 4 clocks otherwise. The original 8088 aliases 60h-6Fh to
// 70h-7Fh. Expected results use an independent condition table
// rather than the implementation's jumpCondition() helper.
namespace {
bool condition(unsigned c,WORD f) {
    const bool cf=(f&0x0001)!=0;
    const bool pf=(f&0x0004)!=0;
    const bool zf=(f&0x0040)!=0;
    const bool sf=(f&0x0080)!=0;
    const bool of=(f&0x0800)!=0;
    // Use explicit expressions for the compound branches rather
    // than inferring them from a canonical opcode execution.
    switch(c) {
    case 0:return of;
    case 1:return !of;
    case 2:return cf;
    case 3:return !cf;
    case 4:return zf;
    case 5:return !zf;
    case 6:return cf||zf;
    case 7:return !(cf||zf);
    case 8:return sf;
    case 9:return !sf;
    case 10:return pf;
    case 11:return !pf;
    case 12:return sf!=of;
    case 13:return sf==of;
    case 14:return zf||(sf!=of);
    case 15:return !zf&&(sf==of);
    }
    return false;
}
bool check(unsigned opcode,WORD flags,std::int8_t disp,bool prefix) {
    Main8088Board board(CpuType::I8088);
    board.bus().enableConformanceRam();
    const unsigned codeBase=0x10000u;
    const unsigned pos=prefix?1u:0u;
    if(prefix)board.bus().writeMemory(codeBase,0x26);
    board.bus().writeMemory(codeBase+pos,BYTE(opcode));
    board.bus().writeMemory(codeBase+pos+1,BYTE(disp));
    std::array<WORD,8> regs{};
    regs[0]=0x5678;
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,flags);
    cpu.executeOne();
    const bool taken=condition(opcode&15,flags);
    const WORD fallthrough=WORD(2+pos);
    const WORD expectedIp=taken
        ?WORD(fallthrough+disp):fallthrough;
    const unsigned expectedCycles=(taken?16u:4u)+(prefix?2u:0u);
    if(cpu.lastExecutionCycles()!=expectedCycles ||
       cpu.ip()!=expectedIp || cpu.cs()!=0x1000 ||
       cpu.flags()!=flags || cpu.reg(0)!=0x5678) {
        std::cerr<<"8088 Jcc opcode "<<std::hex<<opcode
                 <<" flags "<<flags<<std::dec
                 <<" displacement "<<int(disp)
                 <<" prefixed "<<prefix
                 <<" IP "<<cpu.ip()<<" expected "<<expectedIp
                 <<" cycles "<<cpu.lastExecutionCycles()
                 <<" expected "<<expectedCycles<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    // Construct flag words from independent bit positions, including
    // simultaneous flags to exercise compound predicates.
    constexpr WORD f[]={0x0002,0x0003,0x0006,0x0042,0x0082,
        0x0802,0x00c2,0x0842,0x0882,0x08c2,0x0847,0x0007};
    for(unsigned code=0;code<16;++code)
        for(unsigned high=0;high<2;++high)
            for(WORD flags:f)
                for(const std::int8_t disp:{std::int8_t(-7),
                                             std::int8_t(0),
                                             std::int8_t(7)})
                    for(unsigned pref=0;pref<2;++pref)
                        if(!check((high?0x70u:0x60u)+code,flags,
                                  disp,pref!=0))return 1;
    return 0;
}
