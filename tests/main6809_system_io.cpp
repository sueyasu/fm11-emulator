#include "Main6809Board.h"

namespace {

bool eventReadAcknowledgesBreakAndAttention()
{
    Main6809Board board(CpuType::MC6809);
    board.signalAttention();
    board.signalBreak();

    if (board.readCpu(0xfd04) != 0x03)
        return false;
    return board.readCpu(0xfd04) == 0x00;
}

bool atentAckWritePreservesBreakUntilRead()
{
    Main6809Board board(CpuType::MC6809);
    board.signalAttention();
    board.signalBreak();

    board.writeCpu(0xfd04, 0x00);
    if (board.readCpu(0xfd04) != 0x02)
        return false;
    return board.readCpu(0xfd04) == 0x00;
}

bool subCpuHaltRequestSetsVisibleStatus()
{
    Main6809Board board(CpuType::MC6809);

    board.writeCpu(0xfd05, 0x80);
    if (board.readCpu(0xfd05) != 0x80)
        return false;

    // In the original board, clearing the output HALT request does not itself
    // clear FD05 bit 7; the sub-CPU side changes the returned status later.
    board.writeCpu(0xfd05, 0x00);
    return board.readCpu(0xfd05) == 0x80;
}

bool ordinaryUnportedRegisterKeepsBackingValue()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd01, 0x5a);
    return board.readCpu(0xfd01) == 0x5a;
}

bool resetClearsSystemIoState()
{
    Main6809Board board(CpuType::MC6809);
    board.signalAttention();
    board.signalBreak();
    board.writeCpu(0xfd05, 0x80);
    board.writeCpu(0xfd01, 0x5a);

    board.reset();

    return board.readCpu(0xfd04) == 0x00 &&
           board.readCpu(0xfd05) == 0x00 &&
           board.readCpu(0xfd01) == 0x00;
}

} // namespace

int main()
{
    if (!eventReadAcknowledgesBreakAndAttention())
        return 1;
    if (!atentAckWritePreservesBreakUntilRead())
        return 2;
    if (!subCpuHaltRequestSetsVisibleStatus())
        return 3;
    if (!ordinaryUnportedRegisterKeepsBackingValue())
        return 4;
    if (!resetClearsSystemIoState())
        return 5;
    return 0;
}
