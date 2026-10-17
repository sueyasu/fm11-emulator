#include "Main6809Board.h"

int main()
{
    Main6809Board b(CpuType::MC6809);
    Main6809Board::BootRomImage rom{};
    rom[0xffe] = 0x10;
    b.loadBootRom(rom);
    b.reset();
    // LDD #1 (3), ADDD #2 (4), STD <$80 (5),
    // LDX #$0080 (3), LDD <$80 (5), SUBD <$80 (6),
    // STX >$0200 (6), LDU >$0200 (6), CMPX >$0200 (7).
    const BYTE program[] = {
        0xcc,0x00,0x01, 0xc3,0x00,0x02, 0xdd,0x80,
        0x8e,0x00,0x80, 0xdc,0x80, 0x93,0x80,
        0xbf,0x02,0x00, 0xfe,0x02,0x00, 0xbc,0x02,0x00
    };
    for (unsigned i = 0; i < sizeof(program); ++i)
        b.writeCpu(static_cast<WORD>(0x1000 + i), program[i]);
    b.setCpuClockHz(9700);
    b.writeCpu(0xfd39, 1);
    b.writeCpu(0xfd38, 0x50);
    b.writeCpu(0xfd3a, 0);
    b.writeCpu(0xfd3b, 100);
    for (int i = 0; i < 9; ++i) b.cpu().executeOne();
    if (b.cpu().pc() != 0x1000 + sizeof(program)) return 1;
    // 3+4+5+3+5+6+6+6+7 = 45 cycles.
    if (b.readCpu(0xfd3a) != 0 || b.readCpu(0xfd3b) != 55) return 2;
    if (b.readCpu(0x0200) != 0 || b.readCpu(0x0201) != 0x80) return 3;
    return 0;
}
