#include "Main6809Board.h"
#include <array>

int main()
{
    // Each tested base-6809 8-bit immediate ALU opcode must take two cycles.
    constexpr std::array<BYTE, 20> opcodes = {
        0x80, 0x81, 0x82, 0x84, 0x85, 0x86, 0x88, 0x89, 0x8a, 0x8b,
        0xc0, 0xc1, 0xc2, 0xc4, 0xc5, 0xc6, 0xc8, 0xc9, 0xca, 0xcb
    };
    Main6809Board board(CpuType::MC6809);
    Main6809Board::BootRomImage rom{};
    rom[0x0ffe] = 0x10;
    rom[0x0fff] = 0x00;
    board.loadBootRom(rom);
    board.reset();
    WORD addr = 0x1000;
    for (BYTE op : opcodes) {
        board.writeCpu(addr++, op);
        board.writeCpu(addr++, 0x01);
    }
    board.writeCpu(addr, 0x4f); // CLRA: two cycles from the opcode database
    // PTM tick input rate and CPU clock are equal, so one measured
    // CPU cycle maps to one PTM tick without a fractional conversion.
    board.setCpuClockHz(9700);
    board.writeCpu(0xfd39, 0x01);
    board.writeCpu(0xfd38, 0x50);
    board.writeCpu(0xfd3a, 0);
    board.writeCpu(0xfd3b, 100);
    for (unsigned i = 0; i < opcodes.size(); ++i) {
        board.cpu().executeOne();
        if (board.cpu().pc() != 0x1002 + i * 2) return 1;
    }
    if (board.readCpu(0xfd3a) != 0 || board.readCpu(0xfd3b) != 60)
        return 2;
    board.cpu().executeOne(); // CLRA is timed
    if (board.cpu().pc() != static_cast<WORD>(addr + 1)) return 3;
    if (board.readCpu(0xfd3a) != 0 || board.readCpu(0xfd3b) != 58) return 4;
    return 0;
}
