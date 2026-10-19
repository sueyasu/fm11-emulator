#include "Machine.h"
#include "Main6809Board.h"

#include <memory>

int main()
{
    Machine machine;
    auto main6809 = std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* board = main6809.get();

    Main6809Board::BootRomImage bootRom{};
    bootRom[0x0ffe] = 0x12;
    bootRom[0x0fff] = 0x34;
    board->loadBootRom(bootRom);
    board->writePhysical(0x1234, 0x12); // NOP

    const auto index = machine.addBoard(std::move(main6809));
    if (index != 0 || machine.boardCount() != 1)
        return 1;

    machine.setActiveBoard(index);
    machine.reset();

    if (machine.activeBoard() != board)
        return 2;
    if (machine.activeBoard()->cpu().type() != CpuType::MC6809)
        return 3;
    if (machine.activeBoard()->cpu().pc() != 0x1234)
        return 4;

    machine.executeOne();
    if (machine.activeBoard()->cpu().pc() != 0x1235)
        return 5;
    // MC6809 NOP is exactly two clock cycles; Machine must report
    // those cycles without independently advancing the shared PTM.
    if (machine.lastExecutionCycles() != 2)
        return 6;

    return 0;
}
