#include "Main6809Board.h"

#include <stdexcept>

int main()
{
    Main6809Board board(CpuType::MC6809);

    // The current map is identity while MMR is disabled, but CPU accesses
    // must pass through the board logical-address interface.
    board.writePhysical(0x1234, 0x12); // NOP
    if (board.readCpu(0x1234) != 0x12)
        return 1;

    board.writeCpu(0x2345, 0xa5);
    if (board.readPhysical(0x2345) != 0xa5)
        return 2;

    // Reset-vector fetch and instruction fetch both use the CPU address space.
    board.writePhysical(0xfffe, 0x12);
    board.writePhysical(0xffff, 0x34);
    board.reset();
    if (board.cpu().pc() != 0x1234)
        return 3;
    board.cpu().executeOne();
    if (board.cpu().pc() != 0x1235)
        return 4;

    // The FM-11 MMR uses an 8-bit physical page number with 4 KiB pages,
    // giving the board a 1 MiB physical address space.
    board.writePhysical(0xfffff, 0x5a);
    if (board.readPhysical(0xfffff) != 0x5a)
        return 5;

    try {
        (void)board.readPhysical(0x100000);
        return 6;
    } catch (const std::out_of_range&) {
    }

    try {
        board.writePhysical(0x100000, 0xff);
        return 7;
    } catch (const std::out_of_range&) {
    }

    return 0;
}
