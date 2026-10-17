#include "Sim09.h"
#include <array>

class RamCpu final : public CSim09 {
public:
    std::array<BYTE,65536> memory{};
    BYTE GetMemByte(WORD a) override { return memory[a]; }
    void SetMemByte(WORD a, BYTE d) override { memory[a]=d; }
};
int main() {
    // LDX #$0200, LEAX ,X, LEAY 5,X, LEAS 8-bit,X,
    // LEAU 16-bit,X, ORCC #0, ANDCC #$ff, TFR A,B, EXG A,B
    constexpr BYTE program[] = {
        0x8e,0x02,0x00, 0x30,0x84, 0x31,0x05,
        0x32,0x88,0x00, 0x33,0x89,0x00,0x00,
        0x1a,0x00, 0x1c,0xff, 0x1f,0x89, 0x1e,0x89
    };
    constexpr std::array<unsigned,9> cycles={3,4,5,5,8,3,3,6,8};
    for (int mode=0;mode<3;++mode) {
        RamCpu cpu;
        cpu.Set6309Enabled(mode != 0);
        cpu.memory[0xfffe]=0x10; cpu.memory[0xffff]=0;
        for(unsigned i=0;i<sizeof(program);++i)
            cpu.memory[0x1000+i]=program[i];
        cpu.SimRESET();
        if(mode==2) cpu.regs.MD|=1; // unverified native deltas remain untouched
        for(unsigned i=0;i<cycles.size();++i) {
            cpu.SimONE();
            if(cpu.LastInstructionCycles()!=cycles[i])return 1+mode*9+i;
        }
        if(cpu.regs.r.w.PC!=0x1000+sizeof(program))return 30+mode;
    }
    return 0;
}
