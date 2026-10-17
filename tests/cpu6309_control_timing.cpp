#include "Sim09.h"
#include <array>
#include <cstddef>

class RamCpu final : public CSim09 {
public:
    std::array<BYTE,65536> ram{};
    BYTE GetMemByte(WORD address) override { return ram[address]; }
    void SetMemByte(WORD address, BYTE data) override { ram[address]=data; }
};

struct Case {
    std::array<BYTE,4> bytes;
    unsigned count;
    unsigned emulationCycles;
    unsigned nativeCycles;
};

int main()
{
    // Include both branch directions and every measured subroutine
    // addressing form. Mask $06 transfers A+B (2 extra cycles).
    constexpr Case cases[] = {
        {{{0x16,0x00,0x00,0}},3,5,4},   // LBRA
        {{{0x17,0x00,0x00,0}},3,9,7},   // LBSR
        {{{0x8d,0x00,0,0}},2,7,6},      // BSR
        {{{0x9d,0x80,0,0}},2,7,6},      // JSR direct
        {{{0xad,0x84,0,0}},2,7,6},      // JSR ,X
        {{{0xbd,0x20,0x00,0}},3,8,7},  // JSR extended
        {{{0x39,0,0,0}},1,5,4},         // RTS
        {{{0x34,0x06,0,0}},2,7,6},     // PSHS A,B
        {{{0x35,0x06,0,0}},2,7,6},     // PULS A,B
        {{{0x36,0x06,0,0}},2,7,6},     // PSHU A,B
        {{{0x37,0x06,0,0}},2,7,6},     // PULU A,B
        {{{0x26,0x00,0,0}},2,3,3},     // BNE same in all modes
        {{{0x27,0x00,0,0}},2,3,3},     // BEQ same in all modes
    };
    for (int mode=0;mode<3;++mode) {
        for (unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
            RamCpu cpu;
            cpu.Set6309Enabled(mode != 0);
            cpu.ram[0xfffe]=0x10;
            cpu.ram[0xffff]=0;
            for (unsigned j=0;j<cases[i].count;++j)
                cpu.ram[0x1000+j]=cases[i].bytes[j];
            cpu.ram[0x0080]=0x20; cpu.ram[0x0081]=0x00;
            cpu.SimRESET();
            cpu.regs.r.w.S=0x3000;
            cpu.regs.r.w.U=0x3200;
            cpu.regs.r.w.X=0x2000;
            if (mode==2) cpu.regs.MD |= 1;
            cpu.SimONE();
            const unsigned expected = mode==2 ? cases[i].nativeCycles
                                               : cases[i].emulationCycles;
            if (cpu.LastInstructionCycles()!=expected)
                return 1+mode*20+i;
        }
    }
    return 0;
}
