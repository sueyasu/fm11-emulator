#include "Main6809Board.h"

namespace {

bool bootRomOverlay()
{
    Main6809Board board(CpuType::MC6809);
    Main6809Board::BootRomImage bootRom{};
    bootRom[0x0123] = 0x5a;
    board.loadBootRom(bootRom);

    board.writePhysical(0x5123, 0xa5);
    if (board.readCpu(0x5123) != 0x5a)
        return false;

    board.writeCpu(0x5123, 0x33); // ignored while ROM is visible
    if (board.readPhysical(0x5123) != 0xa5)
        return false;

    board.writeCpu(0xfd93, 0x01); // hide boot ROM
    if (board.readCpu(0x5123) != 0xa5)
        return false;

    board.writeCpu(0x5123, 0x66);
    return board.readPhysical(0x5123) == 0x66;
}

bool windowMapping()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd91, 0x02);
    board.writeCpu(0xfd92, 0x10);
    board.writePhysical(0x28c34, 0x6c);
    board.writeCpu(0xfd93, 0x40);

    if (board.readCpu(0x7c34) != 0x6c)
        return false;

    board.writeCpu(0x7d00, 0xd7);
    return board.readPhysical(0x28d00) == 0xd7;
}

bool fixedBoardRamAndIo()
{
    Main6809Board board(CpuType::MC6809);

    board.writePhysical(0xfc10, 0x11);
    board.writeCpu(0xfc10, 0x22);
    if (board.readCpu(0xfc10) != 0x22 || board.readPhysical(0xfc10) != 0x11)
        return false;

    board.writePhysical(0xfd20, 0x44);
    board.writeCpu(0xfd20, 0x33);
    if (board.readCpu(0xfd20) != 0x33 || board.readPhysical(0xfd20) != 0x44)
        return false;

    board.writeCpu(0xfd8f, 0x77);
    board.writeCpu(0xfd93, 0x80);
    board.writePhysical(0x77e20, 0xee);
    board.writeCpu(0xfe20, 0xfe);
    return board.readCpu(0xfe20) == 0xfe && board.readPhysical(0x77e20) == 0xee;
}

bool resetVectorComesFromBootRom()
{
    Main6809Board board(CpuType::MC6809);
    Main6809Board::BootRomImage bootRom{};
    bootRom[0x0ffe] = 0x12;
    bootRom[0x0fff] = 0x34;
    board.loadBootRom(bootRom);

    board.writeCpu(0xfffe, 0xaa);
    board.writeCpu(0xffff, 0xbb);
    board.writeCpu(0xfd93, 0xc1);
    board.reset();

    return board.cpu().pc() == 0x1234 &&
           board.readCpu(0xfffe) == 0x12 &&
           board.readCpu(0xffff) == 0x34 &&
           board.readCpu(0xfd93) == 0x00;
}

} // namespace

int main()
{
    if (!bootRomOverlay())
        return 1;
    if (!windowMapping())
        return 2;
    if (!fixedBoardRamAndIo())
        return 3;
    if (!resetVectorComesFromBootRom())
        return 4;
    return 0;
}
