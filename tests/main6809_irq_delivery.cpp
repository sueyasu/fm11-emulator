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
} // namespace

int main()
{
    if (!maskedIrqRemainsPendingAndReturns()) return 1;
    if (!maskedAtBoardDoesNotEnterCpu()) return 2;
    return 0;
}
