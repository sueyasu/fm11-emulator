#include "MainBoard.h"
#include "Machine.h"
#include "Main6809Board.h"
#include <memory>

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
    // Elapsed-time scheduling is opt-in, and preserves CPU instruction
    // boundaries rather than executing one sub-instruction per main step.
    Machine timed;
    DisplaySubsystem::RomImage timedRom{};
    timedRom[0]=0x86;timedRom[1]=0x55;  // LDA #55
    timedRom[2]=0xb7;timedRom[3]=0x80;timedRom[4]=0x00; // STA $8000
    timedRom[5]=0x12; // NOP
    timedRom[0x3ffe]=0xc0;timedRom[0x3fff]=0x00;
    // The main-board display CPU is accessible through a card attachment.
    auto cpuBoard=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* mainCard=cpuBoard.get();
    timed.addBoard(std::move(cpuBoard));
    // Configure ROM through the same shared main board attached to 6809.
    // The display CPU is exposed directly by Machine for scheduler tests.
    timed.loadDisplayRom(timedRom);
    timed.reset();
    timed.advanceTimeNanoseconds(10000);
    if(timed.displayMemory(0x8000)!=0)return 8;
    timed.setDisplayCpuClockHz(1000000);
    timed.advanceTimeNanoseconds(2000);
    if(timed.displayCpuPc()!=0xc002)return 9;
    timed.advanceTimeNanoseconds(5000);
    if(timed.displayMemory(0x8000)!=0x55)return 10;
    mainCard->writeCpu(0xfd05,0x80);
    const auto haltedPc=timed.displayCpuPc();
    timed.advanceTimeNanoseconds(100000);
    if(timed.displayCpuPc()!=haltedPc)return 11;
    timed.reset();
    if(timed.displayCpuPc()!=0xc000)return 12;
    // Main-CPU instruction timing also advances the sub CPU.
    mainCard->setCpuClockHz(1000000);
    mainCard->writePhysical(0,0x12); // MC6809 NOP, two cycles
    timed.executeOne();
    if(timed.displayCpuPc()!=0xc002)return 13;
    return 0;
}
