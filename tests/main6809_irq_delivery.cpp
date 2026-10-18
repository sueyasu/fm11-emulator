#include "Main6809Board.h"

namespace {
void init(Main6809Board& board)
{
    Main6809Board::BootRomImage boot{};
    boot[0x0ffe] = 0x10;
    boot[0x0fff] = 0x00;
    board.loadBootRom(boot);
    board.reset();
    // LDS #$FF00, ORCC #$10, NOP, ANDCC #$EF, NOP
    const BYTE program[] = {0x10, 0xce, 0xff, 0x00,
                            0x1a, 0x10, 0x12, 0x1c, 0xef, 0x12};
    for (unsigned i = 0; i < sizeof(program); ++i)
        board.writeCpu(static_cast<WORD>(0x1000 + i), program[i]);
    board.writeCpu(0xfff8, 0x11);
    board.writeCpu(0xfff9, 0x00);
    board.writeCpu(0x1100, 0x3b); // RTI
}

bool maskedIrqRemainsPendingAndReturns()
{
    Main6809Board board(CpuType::MC6809);
    init(board);
    board.cpu().executeOne(); // LDS
    if (board.cpu().pc() != 0x1004) return false;
    board.cpu().executeOne(); // ORCC #I
    if (board.cpu().pc() != 0x1006) return false;

    board.writeCpu(0xfd02, 0x80);
    board.setMainIrqSource(0x40, true);
    if (!board.irqPending()) return false;

    board.cpu().executeOne(); // masked, NOP executes
    if (board.cpu().pc() != 0x1007) return false;
    board.cpu().executeOne(); // masked, ANDCC clears I
    if (board.cpu().pc() != 0x1009) return false;
    board.cpu().executeOne(); // IRQ entry, no NOP execution
    if (board.cpu().pc() != 0x1100) return false;

    board.setMainIrqSource(0x40, false);
    board.cpu().executeOne(); // RTI
    if (board.cpu().pc() != 0x1009) return false;
    board.cpu().executeOne(); // NOP now executes
    return board.cpu().pc() == 0x100a;
}

bool maskedAtBoardDoesNotEnterCpu()
{
    Main6809Board board(CpuType::HD6309);
    init(board);
    board.cpu().executeOne(); // LDS
    board.setMainIrqSource(0x40, true); // no FD02 mask
    board.cpu().executeOne(); // ORCC
    if (board.irqPending()) return false;
    board.cpu().executeOne(); // NOP
    return board.cpu().pc() == 0x1007;
}

// Check both processors using the same waiting-state machinery.
bool waitInstructions(CpuType cpu)
{
    Main6809Board b(cpu);
    init(b);
    // LDS #$FF00 ; ANDCC #$EF ; CWAI #$EF ; NOP
    const BYTE program[] = {0x10,0xce,0xff,0x00,0x1c,0xef,0x3c,0xef,0x12};
    for (unsigned i=0;i<sizeof(program);++i)
        b.writeCpu(static_cast<WORD>(0x1000+i),program[i]);
    b.setCpuClockHz(2000000);
    b.cpu().executeOne();
    b.cpu().executeOne();
    b.cpu().executeOne();
    if (b.cpuWaitState()!=CSim09::WaitState::Cwai || b.cpu().pc()!=0x1008)
        return false;
    b.cpu().executeOne();
    if (b.cpu().pc()!=0x1008) return false;
    b.writeCpu(0xfd02,0x10);
    b.writeCpu(0xfd39,0x01);
    b.writeCpu(0xfd38,0x50);
    b.writeCpu(0xfd3a,0);
    b.writeCpu(0xfd3b,4);
    b.advanceWaitTime(500000); // 4 ticks at 9700Hz
    if (!b.irqPending()) return false;
    b.cpu().executeOne(); // vectors, without double pushing CWAI frame
    if (b.cpuWaitState()!=CSim09::WaitState::Running || b.cpu().pc()!=0x1100)
        return false;
    b.readCpu(0xfd39); // acknowledge PTM
    b.cpu().executeOne(); // RTI
    if (b.cpu().pc()!=0x1008) return false;

    Main6809Board c(cpu);
    init(c);
    const BYTE sync[] = {0x10,0xce,0xff,0x00,0x1a,0x10,0x13,0x12};
    for (unsigned i=0;i<sizeof(sync);++i)
        c.writeCpu(static_cast<WORD>(0x1000+i),sync[i]);
    c.cpu().executeOne();
    c.cpu().executeOne(); // mask IRQ
    c.cpu().executeOne(); // SYNC
    if (c.cpuWaitState()!=CSim09::WaitState::Sync || c.cpu().pc()!=0x1007)
        return false;
    c.cpu().executeOne();
    if (c.cpu().pc()!=0x1007) return false;
    c.writeCpu(0xfd02,0x80);
    c.setMainIrqSource(0x40,true);
    c.cpu().executeOne(); // masked IRQ wakes SYNC, executes NOP
    return c.cpuWaitState()==CSim09::WaitState::Running &&
           c.cpu().pc()==0x1008;
}
} // namespace

int main()
{
    if (!maskedIrqRemainsPendingAndReturns()) return 1;
    if (!maskedAtBoardDoesNotEnterCpu()) return 2;
    if (!waitInstructions(CpuType::MC6809)) return 3;
    if (!waitInstructions(CpuType::HD6309)) return 4;
    return 0;
}
