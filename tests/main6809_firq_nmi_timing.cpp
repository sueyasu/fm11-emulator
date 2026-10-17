#include "Main6809Board.h"

// Verify that a masked FIRQ does not charge cycles, a serviced FIRQ does,
// and the legacy core's NMI re-entry guard prevents double charging.
int main()
{
    Main6809Board b(CpuType::MC6809);
    Main6809Board::BootRomImage rom{};
    rom[0xffe] = 0x10;
    b.loadBootRom(rom);
    b.reset();
    b.writeCpu(0x1000, 0x10); b.writeCpu(0x1001, 0xce);
    b.writeCpu(0x1002, 0xf0); b.writeCpu(0x1003, 0x00); // LDS #$f000
    b.writeCpu(0xfff6, 0x12); b.writeCpu(0xfff7, 0x00);
    b.writeCpu(0xfffc, 0x13); b.writeCpu(0xfffd, 0x00);
    b.writeCpu(0x1200, 0x3b); // RTI after FIRQ
    b.writeCpu(0x1300, 0x3b); // RTI after NMI
    b.setCpuClockHz(9700);
    b.writeCpu(0xfd39, 1); b.writeCpu(0xfd38, 0x50);
    b.writeCpu(0xfd3a, 0); b.writeCpu(0xfd3b, 100);
    b.cpu().executeOne(); // LDS adds 6 cycles
    b.cpu().firq();
    if (b.cpu().pc() != 0x1200) return 1;
    b.cpu().firq(); // CC.F masks re-entry
    if (b.cpu().pc() != 0x1200) return 2;
    b.cpu().executeOne(); // short RTI, 6 cycles
    if (b.cpu().pc() != 0x1004) return 3;
    b.cpu().nmi(); // full NMI entry, 19 cycles
    if (b.cpu().pc() != 0x1300) return 4;
    b.cpu().nmi(); // ST_N blocks re-entry
    if (b.cpu().pc() != 0x1300) return 5;
    b.cpu().executeOne(); // full RTI, 15 cycles
    if (b.cpu().pc() != 0x1004) return 6;
    // 10 (FIRQ) + 6 (short RTI) + 19 (NMI) + 15 (full RTI) = 50.
    if (b.readCpu(0xfd3a) != 0 || b.readCpu(0xfd3b) != 44) return 7;
    return 0;
}
