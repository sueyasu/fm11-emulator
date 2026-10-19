#include "MainBoard.h"

int main()
{
    MainBoard board;
    DisplaySubsystem::RomImage rom{};
    // C000: LDA #55; STA $8000; NOP
    rom[0]=0x86; rom[1]=0x55;
    rom[2]=0xb7; rom[3]=0x80; rom[4]=0x00;
    rom[5]=0x12;
    rom[0x3ffe]=0xc0; rom[0x3fff]=0x00;
    board.displaySubsystem().loadRom(rom);
    board.reset();
    auto& cpu=board.displayCpu();
    if(cpu.pc()!=0xc000)return 1;
    if(!cpu.executeOne() || cpu.pc()!=0xc002)return 2;
    if(!cpu.executeOne() || cpu.pc()!=0xc005)return 3;
    if(board.displaySubsystem().readMemory(0x8000)!=0x55)return 4;
    // HALT request via shared FD05 prevents further instructions.
    board.writeIo(0xfd05,0x80);
    if(cpu.executeOne() || cpu.pc()!=0xc005)return 5;
    board.reset();
    if(cpu.pc()!=0xc000 || board.displaySubsystem().haltRequested())return 6;
    if(board.displaySubsystem().readMemory(0x8000)!=0x55)return 7;
    return 0;
}
