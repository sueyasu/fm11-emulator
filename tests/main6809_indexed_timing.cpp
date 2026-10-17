#include "Main6809Board.h"

int main()
{
    Main6809Board b(CpuType::MC6809);
    Main6809Board::BootRomImage rom{};
    rom[0xffe] = 0x10;
    b.loadBootRom(rom);
    b.reset();
    // LDX #$0080; LDA 0,X; LDA ,X; LDA 0,X (8-bit);
    // LDA 0,X (16-bit); LDA [,X] (indirect).
    const BYTE code[] = {0x8e,0x00,0x80, 0xa6,0x00, 0xa6,0x84,
                         0xa6,0x88,0x00, 0xa6,0x89,0x00,0x00,
                         0xa6,0x94};
    for (unsigned i=0;i<sizeof(code);++i)
        b.writeCpu(static_cast<WORD>(0x1000+i),code[i]);
    b.writeCpu(0x0080,0x00);
    b.writeCpu(0x0081,0x90);
    b.writeCpu(0x0090,0x42);
    b.setCpuClockHz(9700);
    b.writeCpu(0xfd39,1);
    b.writeCpu(0xfd38,0x50);
    b.writeCpu(0xfd3a,0);
    b.writeCpu(0xfd3b,100);
    const WORD pcs[]={0x1003,0x1005,0x1007,0x100a,0x100e,0x1010};
    for (WORD pc:pcs) {
        b.cpu().executeOne();
        if (b.cpu().pc()!=pc) return 1;
    }
    // 5 + 4 + 5 + 8 + 7 = 29 ticks (LDX is not yet timed).
    if (b.readCpu(0xfd3a)!=0 || b.readCpu(0xfd3b)!=71) return 2;
    return 0;
}
