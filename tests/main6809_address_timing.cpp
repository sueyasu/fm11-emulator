#include "Main6809Board.h"
#include <array>

int main()
{
    constexpr std::array<BYTE, 11> a = {0x0,0x1,0x2,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb};
    constexpr std::array<BYTE, 2> high = {0x90, 0xd0};
    Main6809Board board(CpuType::MC6809);
    Main6809Board::BootRomImage rom{};
    rom[0xffe] = 0x10;
    board.loadBootRom(rom);
    board.reset();
    WORD addr = 0x1000;
    for (BYTE base : high)
        for (BYTE low : a) {
            board.writeCpu(addr++, static_cast<BYTE>(base | low));
            board.writeCpu(addr++, 0x80); // direct $0080
        }
    for (BYTE base : high)
        for (BYTE low : a) {
            board.writeCpu(addr++, static_cast<BYTE>((base + 0x20) | low));
            board.writeCpu(addr++, 0x00);
            board.writeCpu(addr++, 0x80); // extended $0080
        }
    board.writeCpu(0x0080, 0x01);
    board.setCpuClockHz(9700);
    board.writeCpu(0xfd39, 1);
    board.writeCpu(0xfd38, 0x50);
    board.writeCpu(0xfd3a, 1);
    board.writeCpu(0xfd3b, 0x00); // 256 PTM ticks
    for (unsigned i = 0; i < 44; ++i)
        board.cpu().executeOne();
    // 22 direct * 4 + 22 extended * 5 = 198 cycles.
    if (board.cpu().pc() != addr) return 1;
    if (board.readCpu(0xfd3a) != 0) return 2;
    if (board.readCpu(0xfd3b) != 58) return 3;
    return 0;
}
