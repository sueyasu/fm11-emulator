#include "Main8088Board.h"
#include <array>
#include <cstdio>

// On the original Intel 8088, opcode bit 4 is ignored by the short
// conditional-branch decoder: 60h..6Fh alias 70h..7Fh. Test every
// condition with the same FLAGS, forward/backward and untaken paths.
namespace {
bool check(BYTE opcode,WORD flags,std::int8_t displacement,WORD expectedIp) {
    Main8088Board board(CpuType::I8088);
    board.bus().enableConformanceRam();
    board.bus().writeMemory(0x10000,opcode);
    board.bus().writeMemory(0x10001,BYTE(displacement));
    std::array<WORD,8> regs{};
    std::array<WORD,4> seg{};
    regs[4]=0x8000;
    seg[1]=0x1000;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,flags);
    cpu.executeOne();
    if(cpu.ip()==expectedIp && cpu.cs()==0x1000 && cpu.flags()==flags)return true;
    std::fprintf(stderr,"8088 alias %02X FLAGS=%04X disp=%d: "
                 "IP=%04X CS=%04X FLAGS=%04X expected IP=%04X\n",
                 unsigned(opcode),unsigned(flags),int(displacement),
                 unsigned(cpu.ip()),unsigned(cpu.cs()),
                 unsigned(cpu.flags()),unsigned(expectedIp));
    return false;
}
}
int main() {
    constexpr WORD CF=0x0001,PF=0x0004,ZF=0x0040,SF=0x0080,OF=0x0800;
    constexpr std::array<WORD,9> flags={
        WORD(0x0002),WORD(0x0002|CF),WORD(0x0002|PF),
        WORD(0x0002|ZF),WORD(0x0002|SF),WORD(0x0002|OF),
        WORD(0x0002|SF|OF),WORD(0x0002|CF|ZF),
        WORD(0x0002|CF|ZF|SF|OF|PF)
    };
    for(unsigned n=0;n<16;++n)
        for(const WORD f:flags)
            for(const std::int8_t d:{std::int8_t(7),std::int8_t(-7)}) {
                // Compare the undocumented alias with the already
                // implemented canonical Jcc, then independently ensure
                // the canonical instruction leaves FLAGS unchanged.
                const auto canonical=[&](){
                    Main8088Board board(CpuType::I8088);
                    board.bus().enableConformanceRam();
                    board.bus().writeMemory(0x10000,BYTE(0x70+n));
                    board.bus().writeMemory(0x10001,BYTE(d));
                    std::array<WORD,8> regs{};
                    std::array<WORD,4> seg{};
                    seg[1]=0x1000;
                    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
                    cpu.setConformanceState(regs,seg,0,f);
                    cpu.executeOne();
                    return cpu.ip();
                }();
                if(!check(BYTE(0x60+n),f,d,canonical))return 1;
            }
    return 0;
}
