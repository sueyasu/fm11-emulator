// Signed DIVD/DIVQ/MULD results, soft/hard overflow, and HD6309 CC flags.
// Results based on hoglet67 hardware characterization and Atkinson 6x09.
#include "Sim09.h"
#include <array>
#include <cstdint>
#include <cstdio>
struct Cpu : CSim09 {
    std::array<BYTE,65536> mem{};
    BYTE GetMemByte(WORD a) override {return mem[a];}
    void SetMemByte(WORD a,BYTE v) override {mem[a]=v;}
};
struct Case {
    const char *name;
    BYTE opcode;
    std::uint32_t dividend;
    WORD divisor;
    std::uint32_t result;
    BYTE nzvc;
    bool fault=false;
};
int main() {
    constexpr BYTE N=8,Z=4,V=2,C=1;
    const Case cases[]={
        {"divd_positive",0x8d,101,5,0x0114,0},
        {"divd_negative_dividend",0x8d,0xff9b,5,0xffec,N},
        {"divd_negative_divisor",0x8d,101,0xfb,0x01ec,N},
        {"divd_both_negative",0x8d,0xff9b,0xfb,0xff14,0},
        {"divd_odd",0x8d,15,5,0x0003,C},
        {"divd_zero_quotient",0x8d,2,5,0x0200,Z},
        {"divd_soft_overflow",0x8d,200,1,0x00c8,V},
        {"divd_hard_overflow",0x8d,300,1,300,V},
        {"divd_negative_hard",0x8d,0xfed4,1,300,BYTE(V|N)},
        {"divd_divzero",0x8d,100,0,100,Z,true},
        {"divq_positive",0x8e,30001,2,0x00013a98,0},
        {"divq_negative_dividend",0x8e,0xffff8acf,2,0xffffc568,N},
        {"divq_negative_divisor",0x8e,30000,0xfffe,0x0000c568,N},
        {"divq_odd",0x8e,21,3,7,C},
        {"divq_zero_quotient",0x8e,2,5,0x00020000,Z},
        {"divq_soft_overflow",0x8e,40000,1,0x00009c40,V},
        {"divq_hard_overflow",0x8e,100000,1,100000,V},
        {"divq_negative_hard",0x8e,0xfffe7960,1,100000,BYTE(V|N)},
        {"divq_divzero",0x8e,100,0,100,Z,true},
        {"muld_positive",0x8f,3,4,12,Z},
        {"muld_negative",0x8f,0xfffd,4,0xfffffff4,N},
        {"muld_both_negative",0x8f,0xfffd,0xfffc,12,Z},
        {"muld_zero",0x8f,0,4,0,Z},
    };
    unsigned failures=0;
    for(const auto &test:cases)
        for(int native=0;native<2;++native) {
            Cpu cpu;
            cpu.Set6309Enabled(true);
            cpu.regs.MD=BYTE(native);
            cpu.regs.r.w.PC=0x1000;
            cpu.regs.r.w.S=0x4000;
            cpu.regs.CC=0;
            if(test.opcode==0x8e)cpu.regs.r.q.Q=test.dividend;
            else cpu.regs.r.w.D=WORD(test.dividend);
            cpu.mem[0x1000]=0x11;cpu.mem[0x1001]=test.opcode;
            cpu.mem[0x1002]=BYTE(test.divisor>>8);
            cpu.mem[0x1003]=BYTE(test.divisor);
            if(test.opcode==0x8d)cpu.mem[0x1002]=BYTE(test.divisor);
            cpu.mem[0xfff0]=0x12;cpu.mem[0xfff1]=0x00;
            cpu.SimONE();
            const std::uint32_t actual=test.opcode==0x8d
                ?cpu.regs.r.w.D:cpu.regs.r.q.Q;
            const BYTE cc=cpu.regs.CC&0x0f;
            if(actual!=test.result || cc!=test.nzvc ||
               (test.fault && cpu.regs.r.w.PC!=0x1200)) {
                ++failures;
                std::fprintf(stderr,"%s native=%d result=%08X expected=%08X "
                    "CC=%02X expected=%02X PC=%04X\n",test.name,native,
                    actual,test.result,cc,test.nzvc,cpu.regs.r.w.PC);
            }
        }
    std::printf("HD6309 arithmetic results: %zu cases x2 modes, failures=%u\n",
                sizeof(cases)/sizeof(cases[0]),failures);
    return failures?1:0;
}
