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
    // Main $FC80-$FCFF and sub $9F80-$9FFF address the same RAM.
    mainCard->writeCpu(0xfc80,0x61);
    mainCard->writeCpu(0xfcff,0x72);
    if(timed.displayMemory(0x9f80)!=0x61 ||
       timed.displayMemory(0x9fff)!=0x72)return 14;
    // Execute sub-CPU writes through the ROM instruction path, too.
    DisplaySubsystem::RomImage mailboxRom{};
    // LDA #A5; STA $9F81
    mailboxRom[0]=0x86;mailboxRom[1]=0xa5;
    mailboxRom[2]=0xb7;mailboxRom[3]=0x9f;mailboxRom[4]=0x81;
    mailboxRom[0x3ffe]=0xc0;mailboxRom[0x3fff]=0x00;
    timed.loadDisplayRom(mailboxRom);
    timed.reset();
    timed.advanceTimeNanoseconds(7000);
    if(mainCard->readCpu(0xfc81)!=0xa5)return 15;
    // System reset does not erase RAM, and adjacent main-board RAM
    // remains independent from the shared aperture.
    mainCard->writeCpu(0xfc7f,0x33);
    mainCard->writeCpu(0xfd05,0x80);
    mainCard->reset();
    if(mainCard->readCpu(0xfc80)!=0x61)return 16;
    if(mainCard->readCpu(0xfc7f)!=0)return 17;
    // Sub-system I/O: AFE0-AFE3 scratch and AFF0 busy latch.
    // Adjacent undocumented registers must remain unmapped.
    auto& io=board.displaySubsystem();
    io.writeMemory(0xafe0,0x12);
    io.writeMemory(0xafe3,0x34);
    if(io.readMemory(0xafe0)!=0x12 ||
       io.readMemory(0xafe3)!=0x34)return 18;
    if(io.readMemory(0xafe4)!=0xff ||
       io.readMemory(0xafef)!=0xff ||
       io.readMemory(0xaff1)!=0xff)return 19;
    io.writeMemory(0xaff0,0x00);
    if(io.busyStatus()!=0 || io.readMemory(0xaff0)!=0)return 20;
    // BUSY control is accessible from real sub-CPU instructions.
    DisplaySubsystem::RomImage busyRom{};
    busyRom[0]=0x86;busyRom[1]=0x00; // LDA #$00
    busyRom[2]=0xb7;busyRom[3]=0xaf;busyRom[4]=0xf0; // STA $AFF0
    busyRom[0x3ffe]=0xc0;busyRom[0x3fff]=0x00;
    io.loadRom(busyRom);
    board.reset();
    if(io.busyStatus()!=0x80 || io.readMemory(0xafe0)!=0)return 21;
    if(!board.displayCpu().executeOne() ||
       !board.displayCpu().executeOne() ||
       io.busyStatus()!=0x00)return 22;
    io.writeMemory(0xaff0,0);
    if(io.busyStatus()!=0)return 23;
    // Display-side ATTENTION reaches the common FD04 event latch.
    // FD04 read acknowledges the request; a second read is clear.
    board.displaySubsystem().notifyMainAttention();
    BYTE event=0;
    if(!board.readIo(0xfd04,event) || (event&0x01)==0)return 24;
    if(!board.readIo(0xfd04,event) || (event&0x01)!=0)return 25;
    // BUSY can be sampled by the main board without assuming yet
    // whether or how it shares FD05's HALT/status bit.
    board.displaySubsystem().writeMemory(0xaff0,0x37);
    if(board.displayBusyStatus()!=0x37)return 26;
    board.displaySubsystem().notifyMainAttention();
    board.reset();
    if(board.displayBusyStatus()!=0x80)return 27;
    if(!board.readIo(0xfd04,event) || (event&0x01)!=0)return 28;
    return 0;
}
