// Signed operand-dependent HD6309 math timing regression.
// Hardware-derived rules: hoglet67/6809Decoder "Undocumented 6309 Behaviours".
// Tests use native/emulation, signs, magnitude and overflow, separately.
#include "Sim09.h"
#include <array>
#include <cstdint>
#include <cstdio>
struct Ram : CSim09 {
    std::array<BYTE,65536> mem{};
    BYTE GetMemByte(WORD a) override {return mem[a];}
    void SetMemByte(WORD a,BYTE v) override {mem[a]=v;}
};
struct Case {
    const char *name;
    BYTE opcode;
    std::uint32_t dividend;
    std::uint16_t divisor;
    int deltaEmulation;
    int deltaNative;
};
int main() {
    const Case cases[] = {
        {"divd_normal",0x8d,100,5,0,0},
        {"divd_negative_dividend",0x8d,0xff9c,5,1,1},
        {"divd_negative_divisor",0x8d,100,0xfb,1,1},
        {"divd_both_negative",0x8d,0xff9c,0xfb,2,2},
        {"divd_soft_overflow",0x8d,200,1,-1,-1},
        {"divd_hard_overflow",0x8d,1000,1,-13,-13},
        {"divd_zero",0x8d,100,0,-2,0},
        {"divq_normal",0x8e,30000,2,0,0},
        {"divq_negative_dividend",0x8e,0xffff8ad0,2,1,1},
        {"divq_negative_divisor",0x8e,30000,0xfffe,1,1},
        {"divq_both_negative",0x8e,0xffff8ad0,0xfffe,2,2},
        {"divq_soft_overflow",0x8e,40000,1,0,0},
        {"divq_hard_overflow",0x8e,100000,1,-21,-21},
        {"divq_zero",0x8e,30000,0,-10,-8},
        {"muld_normal",0x8f,3,4,0,0},
        {"muld_negative_dividend",0x8f,0xffff,4,2,2},
        {"muld_negative_multiplicand",0x8f,3,0xfffc,2,2},
        {"muld_both_negative",0x8f,0xffff,0xfffc,2,2},
    };
    unsigned fails=0;
    for(const auto& c:cases) {
        for(int native=0;native<2;++native) {
            Ram cpu;
            cpu.Set6309Enabled(true);
            cpu.regs.r.w.PC=0x1000;
            cpu.regs.r.w.S=0x4000;
            cpu.regs.MD=BYTE(native);
            if(c.opcode==0x8e)
                cpu.regs.r.q.Q=c.dividend;
            else
                cpu.regs.r.w.D=WORD(c.dividend);
            cpu.mem[0x1000]=0x11;
            cpu.mem[0x1001]=c.opcode;
            cpu.mem[0x1002]=BYTE(c.divisor>>8);
            cpu.mem[0x1003]=BYTE(c.divisor);
            // DIVD takes an 8-bit operand, not a 16-bit operand.
            if(c.opcode==0x8d)cpu.mem[0x1002]=BYTE(c.divisor);
            cpu.SimONE();
            const int base=c.opcode==0x8d?25:(c.opcode==0x8e?36:28);
            const int expected=base+(native?c.deltaNative:c.deltaEmulation);
            const int actual=int(cpu.LastInstructionCycles());
            if(actual!=expected) {
                std::fprintf(stderr,"FAIL %s mode=%d expected=%d got=%d\n",
                    c.name,native,expected,actual);
                ++fails;
            }
        }
    }
    std::printf("HD6309 variable math timing: %zu cases in both modes, %u mismatches\n",
                sizeof(cases)/sizeof(cases[0]),fails);
    return fails?1:0;
}
