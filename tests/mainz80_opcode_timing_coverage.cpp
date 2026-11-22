// Stage 1-D: reference opcode timing coverage, including 256 CB, 256 ED,
// 256 indexed CB, and all 252 non-prefix base opcodes.
// NMOS Z80 timings: Zilog Z80 CPU User Manual, instruction set appendix.
#include "SimZ80.h"
#include <array>
#include <cstdint>
#include <cstdio>

namespace {
struct Cpu: CSimZ80 {
    std::array<BYTE,65536> mem{};
    BYTE GetMem(WORD a) override {return mem[a];}
    void SetMem(WORD a,BYTE v) override {mem[a]=v;}
};
constexpr unsigned baseExpected[256]={
 // 00           01 02 03 04 05 06 07   08 09 0A 0B 0C 0D 0E 0F
   4,10, 7, 6, 4, 4, 7, 4,   4,11, 7, 6, 4, 4, 7, 4,
  13,10, 7, 6, 4, 4, 7, 4,  12,11, 7, 6, 4, 4, 7, 4,
  12,10,16, 6, 4, 4, 7, 4,   7,11,16, 6, 4, 4, 7, 4,
  12,10,13, 6,11,11,10, 4,   7,11,13, 6, 4, 4, 7, 4,
   4, 4, 4, 4, 4, 4, 7, 4,   4, 4, 4, 4, 4, 4, 7, 4,
   4, 4, 4, 4, 4, 4, 7, 4,   4, 4, 4, 4, 4, 4, 7, 4,
   4, 4, 4, 4, 4, 4, 7, 4,   4, 4, 4, 4, 4, 4, 7, 4,
   7, 7, 7, 7, 7, 7, 4, 7,   4, 4, 4, 4, 4, 4, 7, 4,
   4, 4, 4, 4, 4, 4, 7, 4,   4, 4, 4, 4, 4, 4, 7, 4,
   4, 4, 4, 4, 4, 4, 7, 4,   4, 4, 4, 4, 4, 4, 7, 4,
   4, 4, 4, 4, 4, 4, 7, 4,   4, 4, 4, 4, 4, 4, 7, 4,
   4, 4, 4, 4, 4, 4, 7, 4,   4, 4, 4, 4, 4, 4, 7, 4,
  11,10,10,10,17,11, 7,11,   5,10,10, 0,10,17, 7,11,
  11,10,10,11,17,11, 7,11,   5, 4,10,11,10, 0, 7,11,
  11,10,10,19,17,11, 7,11,   5, 4,10, 4,10, 0, 7,11,
  11,10,10, 4,17,11, 7,11,   5, 6,10, 4,10, 0, 7,11
};
unsigned cbExpected(unsigned op,bool indexed) {
    if(indexed)return (op>=0x40&&op<0x80)?20:23;
    if((op&7)!=6)return 8;
    return (op>=0x40&&op<0x80)?12:15;
}
unsigned edExpected(unsigned op) {
    if((op&0xc7)==0x40 || (op&0xc7)==0x41)return 12;
    if((op&0xcf)==0x42 || (op&0xcf)==0x4a)return 15;
    if((op&0xcf)==0x43 || (op&0xcf)==0x4b)return 20;
    if((op&0xc7)==0x44)return 8;
    if((op&0xc7)==0x45)return 14;
    if((op&0xc7)==0x46)return 8;
    switch(op) {
    case 0x47:case 0x4f:case 0x57:case 0x5f:return 9;
    case 0x67:case 0x6f:return 18;
    case 0xa0:case 0xa1:case 0xa2:case 0xa3:
    case 0xa8:case 0xa9:case 0xaa:case 0xab:
    case 0xb0:case 0xb1:case 0xb2:case 0xb3:
    case 0xb8:case 0xb9:case 0xba:case 0xbb:return 16;
    default:return 8;
    }
}
unsigned indexExpected(unsigned op) {
    // Times include one DD/FD fetch. Where the prefix is ignored,
    // it adds exactly four T-states to the ordinary opcode time.
    switch(op) {
    case 0x09:case 0x19:case 0x29:case 0x39:return 15;
    case 0x21:return 14;
    case 0x22:case 0x2a:return 20;
    case 0x23:case 0x2b:return 10;
    case 0x34:case 0x35:return 23;
    case 0x36:return 19;
    case 0x46:case 0x4e:case 0x56:case 0x5e:
    case 0x66:case 0x6e:case 0x7e:
    case 0x70:case 0x71:case 0x72:case 0x73:
    case 0x74:case 0x75:case 0x77:
    case 0x86:case 0x8e:case 0x96:case 0x9e:
    case 0xa6:case 0xae:case 0xb6:case 0xbe:return 19;
    case 0xe1:return 14;
    case 0xe3:return 23;
    case 0xe5:return 15;
    case 0xe9:return 8;
    case 0xf9:return 10;
    case 0xdd:case 0xfd:return 12; // following NOP
    case 0xed:return 12; // ED 00, ignored DD/FD plus two NOP fetches
    case 0xcb:return 23; // indexed RLC B: DD CB 00 00
    default:return 4+baseExpected[op];
    }
}

unsigned run(unsigned op,unsigned prefix,bool indexedBit=false) {
    Cpu c;
    c.regs.SP=0x4000;
    c.regs.IX=0x2000;c.regs.IY=0x2100;
    c.regs.r.w.HL=0x2200;c.regs.r.w.BC=1;
    // LDIR/CPIR use BC as a 16-bit repeat counter; INIR/OTIR use B.
    // Set each to one iteration so the final pass takes 16 T-states.
    if(prefix==0xed) {
        if(op==0xb0 || op==0xb1 || op==0xb8 || op==0xb9)
            c.regs.r.w.BC=1;
        else
            c.regs.r.b.B=1;
    } else {
        c.regs.r.b.B=2; // DJNZ taken
    }
    c.regs.r.b.A=1; // make CPI/CPIR zero comparison false/true by memory defaults
    c.mem[0]=BYTE(prefix);
    c.mem[1]=BYTE(op);
    if(prefix==0) {
        c.mem[0]=BYTE(op);c.mem[1]=0;c.mem[2]=0;
    } else if(indexedBit && (prefix==0xdd || prefix==0xfd)) {
        // DD CB d op sequence
        c.mem[1]=0xcb;c.mem[2]=0;c.mem[3]=BYTE(op);
    }
    c.z80emu();
    return unsigned(c.LastInstructionCycles());
}
}
int main() {
    unsigned failed=0,cases=0;
    const auto compare=[&](const char* family,unsigned op,unsigned expect,unsigned actual) {
        ++cases;
        if(expect!=actual) {
            if(failed<25)
                std::fprintf(stderr,"%s %02X expected=%u got=%u\n",
                    family,op,expect,actual);
            ++failed;
        }
    };
    for(unsigned op=0;op<256;++op) {
        if(op!=0xcb && op!=0xdd && op!=0xed && op!=0xfd)
            compare("base",op,baseExpected[op],run(op,0));
        compare("CB",op,cbExpected(op,false),run(op,0xcb));
        compare("ED",op,edExpected(op),run(op,0xed));
        compare("DDCB",op,cbExpected(op,true),run(op,0xdd,true));
        compare("FDCB",op,cbExpected(op,true),run(op,0xfd,true));
        compare("DD",op,indexExpected(op),run(op,0xdd));
        compare("FD",op,indexExpected(op),run(op,0xfd));
    }
    std::printf("Z80 opcode timing: %u cases, %u failed\n",cases,failed);
    return failed?1:0;
}
