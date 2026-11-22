// Resumable TFM with an interrupt between source read and destination write.
// HD6309 behavior: interrupted source read is repeated after RTI.
// Reference: hoglet67/6809Decoder, "Undocumented 6309 Behaviours".
#include "Sim09.h"
#include <array>
#include <cstdio>
struct Cpu : CSim09 {
    std::array<BYTE,65536> mem{};
    unsigned readCount=0;
    BYTE GetMemByte(WORD a) override {
        if(a==0x2000)++readCount;
        return mem[a];
    }
    void SetMemByte(WORD a,BYTE v) override {mem[a]=v;}
};
static bool run(unsigned kind,bool native,bool interruptAfterWrite) {
    Cpu c;
    c.Set6309Enabled(true);
    c.regs.r.w.PC=0x1000;
    c.regs.r.w.S=0x4000;
    c.regs.r.w.X=0x2000;
    c.regs.r.w.Y=0x2100;
    c.regs.r.w.W=2;
    c.regs.MD=native?1:0;
    c.mem[0x1000]=0x11; c.mem[0x1001]=0x38; c.mem[0x1002]=0x12; // TFM X+,Y+
    c.mem[0x1003]=0x12; // next NOP
    c.mem[0x2000]=0x52; c.mem[0x2001]=0x64;
    c.mem[0x1100]=0x3b; // RTI
    for(WORD vec: {WORD(0xfff6),WORD(0xfff8),WORD(0xfffc)}) {
        c.mem[vec]=0x11; c.mem[WORD(vec+1)]=0x00;
    }
    c.SimONE(); // first TFM source read
    if(c.regs.r.w.PC!=0x1000 || c.readCount!=1 ||
       c.mem[0x2100]!=0 || c.regs.r.w.W!=2 ||
       c.LastInstructionCycles()!=7) return false;
    if(interruptAfterWrite) {
        c.SimONE(); // first write
        if(c.regs.r.w.PC!=0x1000 || c.mem[0x2100]!=0x52 ||
           c.regs.r.w.W!=1 || c.LastInstructionCycles()!=2) return false;
    }
    if(kind==0)c.SimIRQ();
    else if(kind==1)c.SimFIRQ();
    else c.SimNMI();
    if(c.regs.r.w.PC!=0x1100)return false;
    c.SimONE(); // RTI
    if(c.regs.r.w.PC!=0x1000)return false;
    c.SimONE(); // repeat read if interrupted before write, otherwise read next
    const unsigned expectedReads=interruptAfterWrite?1:2;
    if(c.readCount!=expectedReads || c.regs.r.w.PC!=0x1000 ||
       c.LastInstructionCycles()!=7) return false;
    c.SimONE(); // next write
    if(interruptAfterWrite) {
        if(c.regs.r.w.PC!=0x1003 || c.regs.r.w.W!=0 ||
           c.mem[0x2101]!=0x64) return false;
    } else {
        if(c.regs.r.w.PC!=0x1000 || c.regs.r.w.W!=1 ||
           c.mem[0x2100]!=0x52) return false;
        c.SimONE(); // second read
        if(c.LastInstructionCycles()!=1)return false;
        c.SimONE(); // second write
        if(c.LastInstructionCycles()!=2 || c.regs.r.w.W!=0 ||
           c.regs.r.w.PC!=0x1003 || c.mem[0x2101]!=0x64)return false;
    }
    return c.regs.r.w.X==0x2002 && c.regs.r.w.Y==0x2102;
}
int main() {
    unsigned failures=0;
    for(unsigned kind=0;kind<3;++kind)
        for(unsigned native=0;native<2;++native)
            for(unsigned after=0;after<2;++after) {
                if(!run(kind,native!=0,after!=0)) {
                    ++failures;
                    std::fprintf(stderr,"TFM interrupt test failed kind=%u native=%u afterWrite=%u\n",
                                 kind,native,after);
                }
            }
    std::printf("HD6309 TFM IRQ/FIRQ/NMI read/write restart: %u failures\n",failures);
    return failures?1:0;
}
