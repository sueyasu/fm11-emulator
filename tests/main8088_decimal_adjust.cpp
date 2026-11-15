#include "Main8088Board.h"
#include <array>
#include <cstdio>

namespace {
constexpr WORD CF=0x0001, PF=0x0004, AF=0x0010;
constexpr WORD ZF=0x0040, SF=0x0080;
bool check(CpuType type,BYTE opcode,WORD initialAx,WORD initialFlags,
           WORD expectedAx,WORD expectedFlags,WORD mask) {
    Main8088Board board(type);
    board.bus().enableConformanceRam();
    board.bus().writeMemory(0x10000,opcode);
    std::array<WORD,8> regs{};  // AX CX DX BX SP BP SI DI
    std::array<WORD,4> seg{};   // ES CS SS DS
    regs[0]=initialAx;
    regs[4]=0x8000;
    seg[1]=0x1000;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,initialFlags);
    cpu.executeOne();
    if(cpu.reg(0)==expectedAx && (cpu.flags()&mask)==expectedFlags
       && cpu.ip()==1)return true;
    std::fprintf(stderr,"%s opcode=%02X AX=%04X FLAGS=%04X -> "
                 "AX=%04X FLAGS=%04X, expected AX=%04X FLAGS=%04X\n",
                 type==CpuType::I8088?"8088":"V20",unsigned(opcode),
                 unsigned(initialAx),unsigned(initialFlags),unsigned(cpu.reg(0)),
                 unsigned(cpu.flags()&mask),unsigned(expectedAx),
                 unsigned(expectedFlags));
    return false;
}
}
int main() {
    for(const auto cpu:{CpuType::I8088,CpuType::NECV20}) {
        // Low-nibble DAA correction on 9Eh does not trigger a high
        // digit adjustment on the 8088 (the threshold is 9Fh).
        if(!check(cpu,0x27,0x369e,0x0002,0x36a4,SF|AF,CF|PF|AF|ZF|SF))return 1;
        if(!check(cpu,0x27,0x369e,0x0003,0x3604,CF|AF,CF|PF|AF|ZF|SF))return 2;
        // The 8088 adjusts AH separately from AL; AL FFh + 6 must
        // not increment AH twice via a 16-bit AX addition.
        if(!check(cpu,0x37,0x72ff,0x0002,0x7305,CF|AF,CF|AF))return 3;
        if(!check(cpu,0x3f,0x7300,0x0012,0x720a,CF|AF,CF|AF))return 4;
    }
    return 0;
}
