#include "Sim09.h"
#include <array>
class RamCpu : public CSim09 {
public:
    std::array<BYTE,65536> memory{};
    BYTE GetMemByte(WORD a) override { return memory[a]; }
    void SetMemByte(WORD a,BYTE v) override { memory[a]=v; }
};
int main()
{
    // SUBD #1, ADDD <$80, CMPX >$0200, LDD <$80,
    // STX >$0200, ADDD ,X, LDU #$0200, LDU ,X
    constexpr BYTE program[] = {
        0x83,0x00,0x01, 0xd3,0x80, 0xbc,0x02,0x00,
        0xdc,0x80, 0xbf,0x02,0x00, 0xe3,0x84,
        0xce,0x02,0x00, 0xee,0x84
    };
    constexpr std::array<unsigned,8> baseline={4,6,7,5,6,6,3,5};
    constexpr std::array<unsigned,8> native={3,4,5,4,5,5,3,5};
    for (int mode=0;mode<3;++mode) {
        RamCpu cpu;
        cpu.Set6309Enabled(mode!=0);
        cpu.memory[0xfffe]=0x10;
        cpu.memory[0xffff]=0x00;
        for (unsigned i=0;i<sizeof(program);++i)
            cpu.memory[0x1000+i]=program[i];
        cpu.memory[0x80]=0;cpu.memory[0x81]=2;
        cpu.memory[0x200]=0;cpu.memory[0x201]=1;
        cpu.SimRESET();
        if (mode==2) cpu.regs.MD|=1;
        for (unsigned i=0;i<baseline.size();++i) {
            cpu.SimONE();
            if (cpu.LastInstructionCycles() !=
                 (mode==2?native[i]:baseline[i]))
                return 1+8*mode+i;
        }
        if (cpu.regs.r.w.PC != 0x1000+sizeof(program)) return 30+mode;
    }
    return 0;
}
