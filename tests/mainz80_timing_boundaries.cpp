// Stage 1-D: NMOS Z80 cycle-accurate instruction boundaries and interrupts.
// Expected times from Zilog Z80 CPU User Manual, Instruction Set and
// Interrupt Response timing tables. All values are T-states.
#include "SimZ80.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
struct Cpu : CSimZ80 {
    std::array<BYTE,65536> ram{};
    BYTE GetMem(WORD a) override {return ram[a];}
    void SetMem(WORD a,BYTE b) override {ram[a]=b;}
};
static bool instruction(const char* name,Cpu& cpu,unsigned expected,
                        unsigned expectedPC) {
    cpu.z80emu();
    if(cpu.LastInstructionCycles()!=expected || cpu.regs.PC!=expectedPC) {
        std::fprintf(stderr,"%s: clocks=%llu expected=%u pc=%04X expected=%04X\n",
            name,static_cast<unsigned long long>(cpu.LastInstructionCycles()),
            expected,cpu.regs.PC,expectedPC);
        return false;
    }
    return true;
}
int main() {
    int failure=0;
    {
        Cpu c;
        c.regs.r.w.HL=0x1234;c.regs.r.b.A=0x56;
        c.ram[0]=0x22;c.ram[1]=0x00;c.ram[2]=0x20;
        c.ram[3]=0x2a;c.ram[4]=0x00;c.ram[5]=0x20;
        c.ram[6]=0x32;c.ram[7]=0x02;c.ram[8]=0x20;
        c.ram[9]=0x3a;c.ram[10]=0x02;c.ram[11]=0x20;
        if(!instruction("LD (nn),HL",c,16,3))++failure;
        if(!instruction("LD HL,(nn)",c,16,6))++failure;
        if(!instruction("LD (nn),A",c,13,9))++failure;
        if(!instruction("LD A,(nn)",c,13,12))++failure;
        if(c.regs.r.b.A!=0x56 || c.regs.r.w.HL!=0x1234)++failure;
    }
    {
        Cpu c;
        c.ram[0]=0xdd;c.ram[1]=0x00; // prefix ignored by NOP
        c.ram[2]=0xfd;c.ram[3]=0xdd;c.ram[4]=0x21;
        c.ram[5]=0x34;c.ram[6]=0x12; // last DD takes effect (IX)
        c.ram[7]=0xdd;c.ram[8]=0xed;c.ram[9]=0x44; // prefix ignored by NEG
        if(!instruction("DD NOP",c,8,2))++failure;
        if(!instruction("FD DD LD IX",c,18,7))++failure;
        if(c.regs.IX!=0x1234)++failure;
        if(!instruction("DD ED NEG",c,12,10))++failure;
        if((c.regs.r.b.R&0x7f)!=7)++failure; // 2+3+2 M1 fetches
    }
    {
        Cpu c;
        c.regs.r.w.HL=0x2000;c.regs.r.w.DE=0x2100;c.regs.r.w.BC=3;
        c.ram[0]=0xed;c.ram[1]=0xb0; // LDIR
        c.ram[0x2000]=0x12;c.ram[0x2001]=0x23;c.ram[0x2002]=0x34;
        if(!instruction("LDIR first",c,21,0))++failure;
        if(c.ram[0x2100]!=0x12||c.regs.r.w.BC!=2)++failure;
        if(!instruction("LDIR second",c,21,0))++failure;
        if(!instruction("LDIR last",c,16,2))++failure;
        if(c.ram[0x2102]!=0x34 || (c.regs.r.b.R&0x7f)!=6)++failure;
    }
    {
        Cpu c;
        c.ram[0]=0xfb;c.ram[1]=0x00;c.ram[2]=0x76;
        c.regs.SP=0x4000;
        if(!instruction("EI",c,4,1))++failure;
        c.z80irq(); // suppressed immediately after EI
        if(c.regs.PC!=1||c.regs.SP!=0x4000)++failure;
        if(!instruction("instruction after EI",c,4,2))++failure;
        c.z80irq(); // IM0, FFh data bus => RST 38h
        if(c.regs.PC!=0x0038 || c.LastInstructionCycles()!=13 ||
           c.regs.SP!=0x3ffe || c.GetWord(0x3ffe)!=2)++failure;
    }
    {
        Cpu c;
        c.ram[0]=0xed;c.ram[1]=0x5e; // IM 2
        c.ram[2]=0xfb; c.ram[3]=0x00;
        c.ram[0x12ff]=0x56;c.ram[0x1300]=0x34;
        c.regs.r.b.I=0x12;c.regs.SP=0x4000;
        if(!instruction("IM2",c,8,2))++failure;
        if(!instruction("EI for IM2",c,4,3))++failure;
        if(!instruction("NOP before IM2",c,4,4))++failure;
        c.z80irq();
        if(c.regs.PC!=0x3456 || c.LastInstructionCycles()!=19 ||
           c.GetWord(0x3ffe)!=4)++failure;
    }
    {
        Cpu c;
        c.ram[0]=0x76; // HALT
        c.regs.SP=0x4000;
        c.regs.f.IFF1=1;c.regs.f.IFF2=1;
        if(!instruction("HALT",c,4,0))++failure;
        if(!instruction("HALT repeat",c,4,0))++failure;
        c.z80irq();
        if(c.regs.PC!=0x38 || c.GetWord(0x3ffe)!=1 ||
           c.LastInstructionCycles()!=13)++failure;
    }
    {
        Cpu c;
        c.regs.SP=0x4000;c.regs.PC=0x2000;
        c.regs.f.IFF1=1;c.regs.f.IFF2=1;
        c.ram[0x66]=0xed;c.ram[0x67]=0x45; // RETN
        c.z80nmi();
        if(c.regs.PC!=0x66||c.LastInstructionCycles()!=11 ||
           c.regs.f.IFF1!=0||c.regs.f.IFF2!=1)++failure;
        if(!instruction("RETN",c,14,0x2000))++failure;
        if(c.regs.f.IFF1!=1)++failure;
    }
    std::printf("Z80 timing boundaries: %d failure(s)\n",failure);
    return failure?1:0;
}
