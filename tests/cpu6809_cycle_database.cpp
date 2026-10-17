#include "Sim09.h"
#include <array>
class Cpu : public CSim09 {
public:
    std::array<BYTE,65536> m{};
    BYTE GetMemByte(WORD a) override { return m[a]; }
    void SetMemByte(WORD a,BYTE v) override { m[a]=v; }
};
int main() {
    // Previously unmeasured unary register and memory instructions
    // must advance emulated time through the new database.
    const BYTE code[]={0x4f,0x5f,0x0f,0x80,0x7f,0x02,0x00,0x12};
    for(int mode=0; mode<3; ++mode) {
        Cpu cpu;cpu.Set6309Enabled(mode!=0);
        cpu.m[0xfffe]=0x10;cpu.m[0xffff]=0;
        for(unsigned i=0;i<sizeof(code);++i)cpu.m[0x1000+i]=code[i];
        cpu.SimRESET();if(mode==2)cpu.regs.MD=1;
        const unsigned expected[]={2,2,6,7,mode==2?1u:2u};
        for(unsigned j=0;j<5;++j) {
            cpu.SimONE();
            if(cpu.LastInstructionCycles()!=expected[j])return 1+mode*5+j;
        }
    }
    return 0;
}
