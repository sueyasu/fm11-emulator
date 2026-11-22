// Hitachi HD6309 extended indexed addressing and timing checks.
// Reference: Atkinson 6809/6309 Programming Reference, indexed table p.150.
#include "Sim09.h"
#include <array>
#include <cstdio>

struct Ram6309 : CSim09 {
    std::array<BYTE,65536> mem{};
    BYTE GetMemByte(WORD a) override {return mem[a];}
    void SetMemByte(WORD a,BYTE b) override {mem[a]=b;}
};
struct Case {const char *name;BYTE pb;unsigned emul;unsigned native;};
static unsigned run(BYTE pb,bool native) {
    Ram6309 cpu;
    cpu.Set6309Enabled(true);
    cpu.regs.r.w.PC=0x1000;
    cpu.regs.r.w.S=0x4000;
    cpu.regs.r.w.X=0x2000;
    cpu.regs.r.w.Y=0x2100;
    cpu.regs.r.w.U=0x2200;
    cpu.regs.r.w.W=0x2300;
    cpu.regs.MD=native?1:0;
    cpu.mem[0x1000]=0xa6; // LDA indexed; base 4 cycles
    cpu.mem[0x1001]=pb;
    cpu.SimONE();
    return cpu.LastInstructionCycles();
}
int main() {
    const Case cases[]={
        {",X+",0x80,6,5}, {",X++",0x81,7,6},
        {"[,X++]",0x91,10,9},
        {"E,X",0x87,5,5}, {"[E,X]",0x97,8,8},
        {"F,X",0x8a,5,5}, {"[F,X]",0x9a,8,8},
        {"W,X",0x8e,5,5}, {"[W,X]",0x9e,8,8},
        {",W",0x8f,4,4}, {"[,W]",0x90,7,7},
        {"16,W",0xaf,6,6}, {"[16,W]",0xb0,9,9},
        {",W++",0xcf,5,5}, {"[,W++]",0xd0,8,8},
        {",--W",0xef,5,5}, {"[,--W]",0xf0,8,8},
        {"16,X",0x89,8,7}, {"[16,X]",0x99,11,10},
        {"D,X",0x8b,8,6}, {"[D,X]",0x9b,11,9},
        {"16,PCR",0x8d,9,7}, {"[16,PCR]",0x9d,12,10},
        {"[nn]",0x9f,9,8},
        {"invalid_92",0x92,0,0},
        {"invalid_BF",0xbf,0,0},
        {"invalid_DF",0xdf,0,0},
        {"invalid_FF",0xff,0,0},
    };
    unsigned bad=0;
    for(const auto& c:cases) {
        for(unsigned mode=0;mode<2;++mode) {
            const unsigned expected=mode?c.native:c.emul;
            const unsigned got=run(c.pb,mode!=0);
            if(got!=expected) {
                ++bad;
                std::fprintf(stderr,"%s pb=%02X native=%u expected=%u got=%u\n",
                             c.name,c.pb,mode,expected,got);
            }
        }
    }
    std::printf("HD6309 indexed timing: %zu postbytes x 2 modes, mismatches=%u\n",
                sizeof(cases)/sizeof(cases[0]),bad);
    return bad?1:0;
}
