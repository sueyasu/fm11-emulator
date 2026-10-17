#include "Main6809Board.h"

namespace {
void setup(Main6809Board& b)
{
    Main6809Board::BootRomImage rom{};
    rom[0xffe] = 0x10;
    b.loadBootRom(rom);
    b.reset();
    b.setCpuClockHz(9700); // PTM advances by one tick per measured cycle
    b.writeCpu(0xfd39, 1);
    b.writeCpu(0xfd38, 0x50);
    b.writeCpu(0xfd3a, 0);
    b.writeCpu(0xfd3b, 100);
}
bool indexedWordCycles()
{
    Main6809Board b(CpuType::MC6809);
    setup(b);
    // LDX #$0200 (3), LDD ,X (5), ADDD 1,X (7),
    // STD 2,X (6), CMPX 8-bit 0,X (7), SUBD 16-bit 0,X (10)
    const BYTE program[] = {0x8e,0x02,0x00, 0xec,0x84, 0xe3,0x01,
                            0xed,0x02, 0xac,0x88,0x00, 0xa3,0x89,0x00,0x00};
    for (unsigned i=0;i<sizeof(program);++i)
        b.writeCpu(static_cast<WORD>(0x1000+i),program[i]);
    b.writeCpu(0x0200,0); b.writeCpu(0x0201,1);
    b.writeCpu(0x0202,0); b.writeCpu(0x0203,2);
    for (int i=0;i<6;++i) b.cpu().executeOne();
    if (b.cpu().pc()!=0x1000+sizeof(program)) return false;
    // 3+5+7+6+7+10 = 38 cycles.
    if (b.readCpu(0xfd3a)!=0 || b.readCpu(0xfd3b)!=62) return false;
    return true;
}
bool indirectWordCycles()
{
    Main6809Board b(CpuType::MC6809);
    setup(b);
    // LDX #$0300 (3), LDD [,X] (8), STU 5-bit,X (6)
    const BYTE program[] = {0x8e,0x03,0x00, 0xec,0x94, 0xef,0x00};
    for (unsigned i=0;i<sizeof(program);++i)
        b.writeCpu(static_cast<WORD>(0x1000+i),program[i]);
    b.writeCpu(0x0300,0x04); b.writeCpu(0x0301,0x00);
    b.writeCpu(0x0400,0x12); b.writeCpu(0x0401,0x34);
    for (int i=0;i<3;++i) b.cpu().executeOne();
    if (b.cpu().pc()!=0x1000+sizeof(program)) return false;
    // 3+8+6=17 cycles
    return b.readCpu(0xfd3a)==0 && b.readCpu(0xfd3b)==83;
}
}
int main() {
    if (!indexedWordCycles()) return 1;
    if (!indirectWordCycles()) return 2;
    return 0;
}
