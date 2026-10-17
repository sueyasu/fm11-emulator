#include "Main6809Board.h"

namespace {
void prepare(Main6809Board& b)
{
    Main6809Board::BootRomImage rom{};
    rom[0x0ffe] = 0x10;
    rom[0x0fff] = 0x00;
    b.loadBootRom(rom);
    b.reset();
    // NOP, NOP, LDA #$55, BRA +0, NOP, illegal timing: CLRA, NOP
    const BYTE code[] = {0x12, 0x12, 0x86, 0x55, 0x20, 0x00, 0x12, 0x4f, 0x12};
    for (unsigned i = 0; i < sizeof(code); ++i)
        b.writeCpu(static_cast<WORD>(0x1000 + i), code[i]);
    b.setCpuClockHz(9700); // one PTM tick per CPU cycle
    b.writeCpu(0xfd39, 0x01); // select first channel
    b.writeCpu(0xfd38, 0x50); // periodic, IRQ-enabled
    b.writeCpu(0xfd3a, 0);
    b.writeCpu(0xfd3b, 4);
}

bool verifiedTimingDrivesPtm()
{
    Main6809Board b;
    prepare(b);
    b.cpu().executeOne(); // 2 cycles NOP
    if (b.cpu().pc() != 0x1001 || b.readCpu(0xfd3b) != 2) return false;
    b.cpu().executeOne(); // another 2 cycles NOP
    if (b.cpu().pc() != 0x1002 || b.readCpu(0xfd39) != 0x81) return false;
    b.cpu().executeOne(); // 2 cycles LDA #
    if (b.cpu().pc() != 0x1004) return false;
    b.cpu().executeOne(); // 3 cycles BRA +0
    if (b.cpu().pc() != 0x1006) return false;
    b.cpu().executeOne(); // 2 cycles NOP
    if (b.cpu().pc() != 0x1007 || b.readCpu(0xfd39) != 0x81) return false;
    b.readCpu(0xfd3a); // clear channel status
    b.cpu().executeOne(); // CLRA (unverified; do not guess cycles)
    if (b.cpu().pc() != 0x1008 || b.readCpu(0xfd3b) != 1) return false;
    return true;
}

bool clockNotConfiguredIsSafe()
{
    Main6809Board b;
    Main6809Board::BootRomImage rom{};
    rom[0xffe] = 0x10;
    b.loadBootRom(rom);
    b.reset();
    b.writeCpu(0x1000, 0x12);
    b.cpu().executeOne();
    return b.cpu().pc() == 0x1001;
}
}

int main()
{
    if (!verifiedTimingDrivesPtm()) return 1;
    if (!clockNotConfiguredIsSafe()) return 2;
    return 0;
}
