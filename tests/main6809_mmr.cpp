#include "Main6809Board.h"

namespace {

bool identityWhenDisabled()
{
    Main6809Board board(CpuType::MC6809);
    board.writePhysical(0x2345, 0x11);
    return board.readCpu(0x2345) == 0x11;
}

bool mapsFourKilobytePages()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd82, 0x34); // logical page 2 -> physical page 34h
    board.writePhysical(0x34567, 0x5a);
    board.writeCpu(0xfd93, 0x80); // MMR enable

    if (board.readCpu(0x2567) != 0x5a)
        return false;

    board.writeCpu(0x2678, 0xa5);
    return board.readPhysical(0x34678) == 0xa5;
}

bool switchesMmrContexts()
{
    Main6809Board board(CpuType::MC6809);

    board.writeCpu(0xfd90, 0x00);
    board.writeCpu(0xfd82, 0x12);
    board.writeCpu(0xfd90, 0x01);
    board.writeCpu(0xfd82, 0x56);

    board.writePhysical(0x12345, 0x12);
    board.writePhysical(0x56345, 0x56);
    board.writeCpu(0xfd93, 0x80);

    board.writeCpu(0xfd90, 0x00);
    if (board.readCpu(0x2345) != 0x12)
        return false;

    board.writeCpu(0xfd90, 0x01);
    return board.readCpu(0x2345) == 0x56;
}

bool masksContextRegisters()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd90, 0xf5);
    board.writeCpu(0xfd91, 0xa7);
    board.writeCpu(0xfd92, 0xbc);
    board.writeCpu(0xfd93, 0xc0);

    return board.readCpu(0xfd90) == 0x05 &&
           board.readCpu(0xfd91) == 0x07 &&
           board.readCpu(0xfd92) == 0xbc &&
           board.readCpu(0xfd93) == 0xc0;
}

bool keepsFc00ThroughFfffFixed()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd8f, 0x77); // logical page F -> physical page 77h
    board.writePhysical(0x77123, 0x77);
    board.writePhysical(0x0fc10, 0x7c);
    board.writeCpu(0xfc10, 0xfc);
    board.writeCpu(0xfd93, 0x80);

    if (board.readCpu(0xf123) != 0x77)
        return false;
    return board.readCpu(0xfc10) == 0xfc && board.readPhysical(0x0fc10) == 0x7c;
}

bool resetDisablesAndClearsMmr()
{
    Main6809Board board(CpuType::MC6809);
    board.writeCpu(0xfd90, 0x03);
    board.writeCpu(0xfd82, 0x44);
    board.writeCpu(0xfd93, 0x80);

    Main6809Board::BootRomImage bootRom{};
    bootRom[0x0ffe] = 0x12;
    bootRom[0x0fff] = 0x34;
    board.loadBootRom(bootRom);
    board.reset();

    if (board.cpu().pc() != 0x1234)
        return false;
    if (board.readCpu(0xfd90) != 0x00 || board.readCpu(0xfd93) != 0x00)
        return false;
    return board.readCpu(0xfd82) == 0x00;
}

} // namespace

int main()
{
    if (!identityWhenDisabled())
        return 1;
    if (!mapsFourKilobytePages())
        return 2;
    if (!switchesMmrContexts())
        return 3;
    if (!masksContextRegisters())
        return 4;
    if (!keepsFc00ThroughFfffFixed())
        return 5;
    if (!resetDisablesAndClearsMmr())
        return 6;
    return 0;
}
