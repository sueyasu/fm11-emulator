#include "Main6809Board.h"
int main()
{
    Main6809Board b(CpuType::MC6809);
    Main6809Board::BootRomImage rom{};
    rom[0xffe]=0x10;
    b.loadBootRom(rom); b.reset();
    // LDS #$F000 (unmeasured page-2 instruction), LDA #1 (2),
    // PSHS A,B,X (5+1+1+2=9), PULS A,B,X (9),
    // BSR +2 (7), BRA +3 (3), subroutine RTS (5), NOP (2).
    const BYTE code[]={0x10,0xce,0xf0,0x00, 0x86,0x01,
                       0x34,0x16, 0x35,0x16, 0x8d,0x03,
                       0x20,0x03, 0x12,0x39, 0x12,0x12};
    for(unsigned i=0;i<sizeof(code);++i)b.writeCpu(static_cast<WORD>(0x1000+i),code[i]);
    b.setCpuClockHz(9700);
    b.writeCpu(0xfd39,1);b.writeCpu(0xfd38,0x50);
    b.writeCpu(0xfd3a,0);b.writeCpu(0xfd3b,100);
    const WORD pcs[]={0x1004,0x1006,0x1008,0x100a,0x100f,0x100c,0x1011,0x1012};
    for(WORD pc:pcs){b.cpu().executeOne();if(b.cpu().pc()!=pc)return 1;}
    // 2+9+9+7+5+3+2 = 37; LDS is not yet timed.
    if(b.readCpu(0xfd3a)!=0||b.readCpu(0xfd3b)!=63)return 2;
    return 0;
}
