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

    // Two-cycle NOP at 19.4 kHz advances a 9.7 kHz board PTM by one tick.
    // Card-internal stepping is disabled when owned by Machine.
    board->setCpuClockHz(19400);
    board->writeCpu(0xfd02,0x10);
    board->writeCpu(0xfd39,0x01);
    board->writeCpu(0xfd38,0x50);
    board->writeCpu(0xfd3a,0);
    board->writeCpu(0xfd3b,1);
    machine.executeOne();
    if (machine.activeBoard()->cpu().pc() != 0x1235)
        return 5;
    // MC6809 NOP is exactly two clock cycles; Machine must report
    // those cycles without independently advancing the shared PTM.
    if (machine.lastExecutionCycles() != 2)
        return 6;
    if (!board->irqPending() || board->readCpu(0xfd39)!=0x81)
        return 7;

    return 0;
}
