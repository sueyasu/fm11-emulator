// Exhaustive MC6809 indexed postbyte timing/legality regression.
// Reference: Motorola M6809PM/AD Appendix F, Table F-2.
// The independent reference decoder below intentionally does not call the
// emulator's IsIndexedPostbyteAllowed() or its cycle helper.
#include "Sim09.h"
#include <array>
#include <cstdint>
#include <cstdio>

namespace {
struct RamCpu : CSim09 {
    std::array<BYTE,65536> memory{};
    BYTE GetMemByte(WORD address) override { return memory[address]; }
    void SetMemByte(WORD address,BYTE value) override { memory[address]=value; }
};

int extraCycles(unsigned pb) {
    if(pb<0x80)return 1; // all 128 five-bit signed displacements
    const unsigned mode=pb&15;
    const bool indirect=(pb&0x10)!=0;
    switch(mode) {
    case 0x00: case 0x02: return indirect?-1:2;
    case 0x01: case 0x03: return indirect?6:3;
    case 0x04: return indirect?3:0;
    case 0x05: case 0x06: case 0x08: case 0x0c:
        return indirect?4:1;
    case 0x09: case 0x0b:
        return indirect?7:4;
    case 0x0d: return indirect?8:5;
    case 0x0f: return pb==0x9f?5:-1;
    default: return -1; // E/F/W offsets are 6309-only
    }
}

struct Op {
    unsigned opcode;
    unsigned base;
};

unsigned execute(const Op& test,BYTE postbyte) {
    RamCpu cpu;
    cpu.Set6309Enabled(false);
    cpu.memory[0xfffe]=0x10;
    cpu.memory[0xffff]=0x00;
    cpu.SimRESET();
    cpu.regs.r.w.X=0x2000;
    cpu.regs.r.w.Y=0x2000;
    cpu.regs.r.w.U=0x2000;
    cpu.regs.r.w.S=0x4000;
    unsigned pos=0x1000;
    if(test.opcode>0xff)cpu.memory[pos++]=BYTE(test.opcode>>8);
    cpu.memory[pos++]=BYTE(test.opcode);
    cpu.memory[pos++]=postbyte;
    // Extra 8/16-bit displacement and [nn] operands default to zero.
    // A fresh CPU is used for each trial to avoid autoincrement state.
    cpu.SimONE();
    return cpu.LastInstructionCycles();
}
}

int main() {
    // Representative instructions from all indexed opcode families,
    // including the MC6809 prefixed instruction pages.
    constexpr Op ops[] = {
        {0x30,4},    // LEAX
        {0x60,6},    // NEG memory RMW
        {0x6e,3},    // JMP
        {0xa6,4},    // LDA
        {0xa7,4},    // STA
        {0xe7,4},    // STB
        {0x10ae,6},  // LDY
        {0x11a3,7},  // CMPU
    };
    unsigned legal=0,illegal=0,failed=0;
    for(unsigned pb=0;pb<256;++pb) {
        const int extra=extraCycles(pb);
        if(extra<0)++illegal; else ++legal;
        for(const auto& test:ops) {
            const unsigned got=execute(test,BYTE(pb));
            const unsigned expected=extra<0?0:test.base+unsigned(extra);
            if(got!=expected) {
                if(failed<30)
                    std::fprintf(stderr,
                      "FAIL opcode=%04X postbyte=%02X expected=%u got=%u\n",
                      test.opcode,pb,expected,got);
                ++failed;
            }
        }
    }
    std::printf("MC6809 indexed postbytes: %u legal, %u invalid, "
                "%zu opcode families, %u discrepancies\n",
                legal,illegal,sizeof(ops)/sizeof(ops[0]),failed);
    return legal==217 && illegal==39 && failed==0 ? 0 : 1;
}
