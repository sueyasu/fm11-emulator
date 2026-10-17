#include "Main6809Board.h"

#include <stdexcept>

int main()
{
    Main6809Board board(CpuType::MC6809);

    // The current map is identity, but CPU accesses must pass through the
    // board logical-address interface rather than touching backing storage.
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

    // Physical addresses are wider than the current backing store so a
    // future MMR implementation can expand memory without changing the API.
    try {
        (void)board.readPhysical(0x10000);
        return 5;
    } catch (const std::out_of_range&) {
    }

    try {
        board.writePhysical(0x10000, 0xff);
        return 6;
    } catch (const std::out_of_range&) {
    }

    return 0;
}
