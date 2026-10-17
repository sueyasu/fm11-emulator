#include "Main6809Board.h"

namespace {
void setup(Main6809Board& b, const BYTE* code, unsigned size)
{
    Main6809Board::BootRomImage rom{};
    rom[0xffe] = 0x10;
    b.loadBootRom(rom);
    b.reset();
    for (unsigned i = 0; i < size; ++i)
        b.writeCpu(static_cast<WORD>(0x1000 + i), code[i]);
    b.setCpuClockHz(9700);
    b.writeCpu(0xfd39, 1);
    b.writeCpu(0xfd38, 0x50);
    b.writeCpu(0xfd3a, 0);
    b.writeCpu(0xfd3b, 100);
}
bool timedLongBranches()
{
    Main6809Board b(CpuType::MC6809);
    // LBEQ +0 (not taken, 5), LBNE +0 (taken, 6),
    // LBRN +0 (never, 5), LBRA +0 (always, 5),
    // BEQ +0 (not taken, 3), BNE +0 (taken, 3).
    const BYTE code[] = {
        0x10,0x27,0x00,0x00, 0x10,0x26,0x00,0x00,
        0x10,0x21,0x00,0x00, 0x16,0x00,0x00,
        0x27,0x00, 0x26,0x00
    };
    setup(b,code,sizeof(code));
    for (int i=0;i<6;++i) b.cpu().executeOne();
    if (b.cpu().pc() != 0x1000+sizeof(code)) return false;
    // Total 5+6+5+5+3+3 = 27 cycles.
    return b.readCpu(0xfd3a) == 0 && b.readCpu(0xfd3b) == 73;
}
bool takenNonzeroOffset()
{
    Main6809Board b(CpuType::MC6809);
    // LDA #0 sets Z, LBEQ +2 skips two NOPs, LBNE not taken.
    const BYTE code[] = {
        0x86,0x00, 0x10,0x27,0x00,0x02, 0x12,0x12,
        0x10,0x26,0x00,0x00
    };
    setup(b,code,sizeof(code));
    b.cpu().executeOne();
    b.cpu().executeOne();
    if (b.cpu().pc()!=0x1008) return false;
    b.cpu().executeOne();
    if (b.cpu().pc()!=0x100c) return false;
    return b.readCpu(0xfd3a)==0 && b.readCpu(0xfd3b)==87; // 2+6+5
}
}
int main()
{
    if (!timedLongBranches()) return 1;
    if (!takenNonzeroOffset()) return 2;
    return 0;
}
