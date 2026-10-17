#include "Sim09.h"
#include <array>

// Run the identical opcode stream as an MC6809, a 6309 in emulation
// mode, and a 6309 in native mode (MD.N = 1).
class RamCpu final : public CSim09 {
public:
    std::array<BYTE, 65536> ram{};
    BYTE GetMemByte(WORD addr) override { return ram[addr]; }
    void SetMemByte(WORD addr, BYTE value) override { ram[addr] = value; }
};
int main()
{
    constexpr BYTE code[] = {
        0x12,             // NOP: 6809 2, 6309 native 1
        0x86, 0x01,       // LDA #$01: always 2
        0x96, 0x80,       // LDA <$80: 4 / 3
        0xb6, 0x02, 0x00,// LDA >$0200: 5 / 4
        0x9c, 0x80,       // CMPX <$80: 6 in baseline; not yet adjusted
        0xa6, 0x84       // LDA ,X: 4 in baseline; no native delta here
    };
    constexpr std::array<unsigned,6> baseline = {2,2,4,5,6,4};
    constexpr std::array<unsigned,6> native   = {1,2,3,4,6,4};
    for (int mode = 0; mode < 3; ++mode) {
        RamCpu cpu;
        cpu.Set6309Enabled(mode != 0);
        for (unsigned i=0;i<sizeof(code);++i) cpu.ram[0x1000+i] = code[i];
        cpu.ram[0xfffe]=0x10; cpu.ram[0xffff]=0x00;
        cpu.ram[0x0080]=0x00; cpu.ram[0x0081]=0x01;
        cpu.ram[0x0200]=0x01;
        cpu.SimRESET();
        if (mode == 2) cpu.regs.MD |= 1; // Native mode, MD.N
        for (unsigned i=0;i<6;++i) {
            cpu.SimONE();
            if (cpu.LastInstructionCycles() !=
                (mode == 2 ? native[i] : baseline[i]))
                return 1 + mode * 6 + i;
        }
        if (cpu.regs.r.w.PC != 0x1000+sizeof(code)) return 20+mode;
    }
    return 0;
}
