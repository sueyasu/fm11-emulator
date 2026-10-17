#include "Main6809Board.h"

int main()
{
    Main6809Board b(CpuType::MC6809);
    Main6809Board::BootRomImage rom{};
    rom[0xffe] = 0x10;
    b.loadBootRom(rom);
    b.reset();
    // LDS #$F000; LDX #$1100; JSR ,X; NOP; RTS at $1100
    const BYTE prog[] = {0x10,0xce,0xf0,0x00, 0x8e,0x11,0x00, 0xad,0x84, 0x12};
    for(unsigned i=0;i<sizeof(prog);++i) b.writeCpu(static_cast<WORD>(0x1000+i),prog[i]);
    b.writeCpu(0x1100,0x39);
    b.writeCpu(0xfff8,0x12);b.writeCpu(0xfff9,0x00);
    b.writeCpu(0x1200,0x3b); // RTI
    b.setCpuClockHz(9700);
    b.writeCpu(0xfd39,1);b.writeCpu(0xfd38,0x50);
    b.writeCpu(0xfd3a,0);b.writeCpu(0xfd3b,100);
    b.cpu().executeOne(); // LDS not timed
    b.cpu().executeOne(); // LDX # (3)
    b.cpu().executeOne(); // JSR ,X (7)
    if(b.cpu().pc()!=0x1100)return 1;
    b.cpu().executeOne(); // RTS (5)
    if(b.cpu().pc()!=0x1009)return 2;
    b.cpu().executeOne(); // NOP (2)
    if(b.cpu().pc()!=0x100a)return 3;
    // IRQ entry (19), full RTI (15).
    b.writeCpu(0xfd02,0x80); b.setMainIrqSource(0x40,true);
    b.cpu().executeOne();
    if(b.cpu().pc()!=0x1200)return 4;
    b.setMainIrqSource(0x40,false);
    b.cpu().executeOne();
    if(b.cpu().pc()!=0x100a)return 5;
    // Total 3+7+5+2+19+15=51.
    if(b.readCpu(0xfd3a)!=0||b.readCpu(0xfd3b)!=49)return 6;
    return 0;
}
