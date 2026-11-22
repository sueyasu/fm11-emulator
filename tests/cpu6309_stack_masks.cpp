// Exhaustive HD6309 PSHS/PSHU/PULS/PULU byte-mask cycle regression.
// The cycle count depends on the set of 8-bit/16-bit registers in the mask.
#include "Sim09.h"
#include <array>
#include <cstdio>
struct Ram : CSim09 {
    std::array<BYTE,65536> mem{};
    BYTE GetMemByte(WORD a) override {return mem[a];}
    void SetMemByte(WORD a,BYTE x) override {mem[a]=x;}
};
static unsigned transferredBytes(unsigned mask) {
    unsigned bytes=0;
    for(unsigned bit=0;bit<8;++bit)if(mask&(1u<<bit))
        bytes+=(bit>=4)?2:1;
    return bytes;
}
int main() {
    unsigned fail=0;
    for(unsigned mode=0;mode<2;++mode)
        for(unsigned op=0x34;op<=0x37;++op)
            for(unsigned mask=0;mask<256;++mask) {
                Ram cpu;
                cpu.Set6309Enabled(true);
                cpu.regs.MD=BYTE(mode);
                cpu.regs.r.w.PC=0x1000;
                cpu.regs.r.w.S=0x4000;
                cpu.regs.r.w.U=0x5000;
                cpu.regs.r.w.X=0x2000;
                cpu.regs.r.w.Y=0x2100;
                cpu.mem[0x1000]=BYTE(op);
                cpu.mem[0x1001]=BYTE(mask);
                cpu.SimONE();
                const unsigned expected=(mode?4u:5u)+transferredBytes(mask);
                const unsigned actual=cpu.LastInstructionCycles();
                if(actual!=expected) {
                    if(fail<20)std::fprintf(stderr,
                       "HD6309 mode=%u opcode=%02X mask=%02X expected=%u actual=%u\n",
                       mode,op,mask,expected,actual);
                    ++fail;
                }
            }
    std::printf("HD6309 stack masks: 2 modes x 4 opcodes x 256 masks, mismatches=%u\n",fail);
    return fail?1:0;
}
