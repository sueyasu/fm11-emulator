#include "Main6809Board.h"

// Verify an unknown opcode is still unmeasured and a timed opcode resumes
// accurate PTM accounting after extraction from the execution decoder.
int main()
{
    Main6809Board b(CpuType::MC6809);
    Main6809Board::BootRomImage rom{};
    rom[0xffe] = 0x10;
    b.loadBootRom(rom);
    b.reset();
    // CLRA (unmeasured); NOP (2); LDB #$01 (2); BRA +0 (3).
    const BYTE code[] = {0x4f, 0x12, 0xc6, 0x01, 0x20, 0x00};
    for (unsigned i=0; i<sizeof(code); ++i)
        b.writeCpu(static_cast<WORD>(0x1000+i), code[i]);
    b.setCpuClockHz(9700);
    b.writeCpu(0xfd39,1);
    b.writeCpu(0xfd38,0x50);
    b.writeCpu(0xfd3a,0);
    b.writeCpu(0xfd3b,100);
    b.cpu().executeOne();
    b.cpu().executeOne();
    b.cpu().executeOne();
    b.cpu().executeOne();
    if (b.cpu().pc()!=0x1006) return 1;
    // CLRA unknown = 0, NOP + LDB immediate + BRA = 7.
    return b.readCpu(0xfd3a)==0 && b.readCpu(0xfd3b)==93 ? 0 : 2;
}
