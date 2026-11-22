// HD6309 timing checks against the HD63B09EP technical reference tables.
// Values are expected emulation/native cycles, including opcode prefixes.
// Some instructions are native-mode invariant. TFM includes byte count.
#include "Sim09.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

namespace {
class RamCpu : public CSim09 {
public:
    std::array<BYTE, 65536> ram{};
    BYTE GetMemByte(WORD addr) override { return ram[addr]; }
    void SetMemByte(WORD addr, BYTE val) override { ram[addr]=val; }
};
struct Case {
    const char* name;
    std::vector<BYTE> bytes;
    unsigned emulation;
    unsigned native;
    unsigned w=0;
};
bool check(const Case& test,bool native) {
    RamCpu cpu;
    cpu.Set6309Enabled(true);
    cpu.regs.r.w.PC=0x1000;
    cpu.regs.r.w.S=0x3000;
    cpu.regs.r.w.X=0x2000;
    cpu.regs.r.w.Y=0x2100;
    cpu.regs.r.w.U=0x2200;
    cpu.regs.r.w.W=WORD(test.w);
    cpu.regs.MD=native?1:0;
    for(std::size_t i=0;i<test.bytes.size();++i)
        cpu.ram[0x1000+i]=test.bytes[i];
    cpu.ram[0]=1; // nonzero direct operand for division reference cases
    cpu.ram[1]=1;
    cpu.ram[0x2000]=0x55;
    cpu.ram[0x2001]=0x66;
    cpu.ram[0x2002]=0x77;
    cpu.SimONE();
    unsigned got=cpu.LastInstructionCycles();
    // TFM exposes read and write boundaries for interrupt delivery.
    // Accumulate cycles across both phases of each transferred byte.
    if(test.w && test.bytes.size()>=2 && test.bytes[0]==0x11 &&
       test.bytes[1]>=0x38 && test.bytes[1]<=0x3b) {
        for(unsigned step=1;step<test.w*2;++step) {
            cpu.SimONE();
            got+=cpu.LastInstructionCycles();
        }
    }
    const unsigned expected=native?test.native:test.emulation;
    if(got!=expected) {
        std::fprintf(stderr,"%s %s expected=%u got=%u\n",
                     test.name,native?"native":"emulation",expected,got);
        return false;
    }
    if(test.w && test.bytes.size()>=2 && test.bytes[0]==0x11 &&
       test.bytes[1]>=0x38 && test.bytes[1]<=0x3b) {
        // TFM R+,R overwrites one fixed destination with the final
        // source byte; all other variants in this fixture leave its
        // initial byte at 2100h unchanged.
        const BYTE expectedByte =
            test.bytes[1] == 0x3a && test.w >= 2 ? 0x66 : 0x55;
        if(cpu.regs.r.w.W!=0 || cpu.ram[0x2100]!=expectedByte) {
            std::fprintf(stderr,"%s %s: TFM data/W mismatch (Y0=%02X expected=%02X W=%04X)\n",
                         test.name,native?"native":"emulation",
                         cpu.ram[0x2100],expectedByte,cpu.regs.r.w.W);
            return false;
        }
    }
    return true;
}
}
int main() {
    const Case cases[] = {
        {"NEG_direct", {0x00,0x10},6,5},
        {"TST_direct", {0x0d,0x10},6,4},
        {"JMP_direct", {0x0e,0x10},3,2},
        {"SEXW", {0x14},4,4},
        {"DAA", {0x19},2,1},
        {"EXG", {0x1e,0x12},8,5},
        {"TFR", {0x1f,0x12},6,4},
        {"NEGA", {0x40},2,1},
        {"TSTB", {0x5d},2,1},
        {"TST_indexed", {0x6d,0x84},6,5},
        {"COM_extended", {0x73,0x20,0x00},7,6},
        {"TST_extended", {0x7d,0x20,0x00},7,5},
        {"JMP_extended", {0x7e,0x20,0x00},4,3},
        {"STX_extended", {0xbf,0x20,0x00},6,5},
        {"LBNE_taken", {0x10,0x26,0,0},5,5},
        {"LBEQ_not_taken", {0x10,0x27,0,0},5,5},
        {"ADDR", {0x10,0x30,0x89},4,4},
        {"PSHSW", {0x10,0x38},6,6},
        {"NEGD", {0x10,0x40},3,2},
        {"COMW", {0x10,0x53},3,2},
        {"SUBW_immediate", {0x10,0x80,0,0},5,4},
        {"CMPD_immediate", {0x10,0x83,0,0},5,4},
        {"CMPD_direct", {0x10,0x93,0},7,5},
        {"CMPD_indexed", {0x10,0xa3,0x84},7,6},
        {"CMPD_extended", {0x10,0xb3,0x20,0},8,6},
        {"LDY_direct", {0x10,0x9e,0},6,5},
        {"LDY_indexed", {0x10,0xae,0x84},6,6},
        {"LDY_extended", {0x10,0xbe,0x20,0},7,6},
        {"LDS_immediate", {0x10,0xce,0x30,0},4,4},
        {"LDS_direct", {0x10,0xde,0},6,5},
        {"LDS_extended", {0x10,0xfe,0x20,0},7,6},
        {"BAND", {0x11,0x30,0,0},7,6},
        {"STBT", {0x11,0x37,0,0},8,7},
        {"BITMD", {0x11,0x3c,0},4,4},
        {"LDMD", {0x11,0x3d,0},5,5},
        {"COME", {0x11,0x43},3,2},
        {"COMF", {0x11,0x53},3,2},
        {"SUBE_immediate", {0x11,0x80,0},3,3},
        {"CMPU_immediate", {0x11,0x83,0,0},5,4},
        {"CMPU_direct", {0x11,0x93,0},7,5},
        {"CMPU_indexed", {0x11,0xa3,0x84},7,6},
        {"CMPU_extended", {0x11,0xb3,0x20,0},8,6},
        {"SUBE_direct", {0x11,0x90,0},5,4},
        {"SUBE_indexed", {0x11,0xa0,0x84},5,5},
        {"SUBE_extended", {0x11,0xb0,0x20,0},6,5},
        {"DIVD_imm", {0x11,0x8d,0x01},25,25},
        {"DIVQ_imm", {0x11,0x8e,0,1},36,36},
        {"MULD_imm", {0x11,0x8f,0,1},28,28},
        {"DIVD_direct", {0x11,0x9d,0},27,26},
        {"DIVQ_direct", {0x11,0x9e,0},36,35},
        {"MULD_direct", {0x11,0x9f,0},30,29},
        {"DIVD_indexed", {0x11,0xad,0x84},27,27},
        {"DIVQ_indexed", {0x11,0xae,0x84},36,36},
        {"MULD_indexed", {0x11,0xaf,0x84},30,30},
        {"DIVD_extended", {0x11,0xbd,0x20,0},28,27},
        {"DIVQ_extended", {0x11,0xbe,0x20,0},37,36},
        {"MULD_extended", {0x11,0xbf,0x20,0},31,30},
        {"TFM_1_byte", {0x11,0x38,0x12},9,9,1},
        {"TFM_3_bytes", {0x11,0x38,0x12},15,15,3},
        {"TFM_decrement", {0x11,0x39,0x12},12,12,2},
        {"TFM_source_inc", {0x11,0x3a,0x12},12,12,2},
        {"TFM_dest_inc", {0x11,0x3b,0x12},12,12,2},
    };
    unsigned failures=0;
    for(const auto& test:cases) {
        if(!check(test,false))++failures;
        if(!check(test,true))++failures;
    }
    std::printf("HD6309 cycle reference: %zu cases x 2 modes, failures=%u\n",
                sizeof(cases)/sizeof(cases[0]),failures);
    return failures?1:0;
}
