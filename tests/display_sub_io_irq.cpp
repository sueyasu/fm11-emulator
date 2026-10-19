#include "DisplaySubsystem.h"
#include "MainBoard.h"
#include <iostream>

int main() {
    DisplaySubsystem d;
    d.reset();
    // Reference SubCpu::GetMemByte / SetMemByte maps sub RAM in A000-AFFF
    // except the explicit AFE* / AFF* device registers.
    if(d.readMemory(0xa100)!=0 || d.readMemory(0xaff1)!=0)return 1;
    d.writeMemory(0xa100,0x3c);
    d.writeMemory(0xaff1,0x59);
    if(d.readMemory(0xa100)!=0x3c || d.readMemory(0xaff1)!=0x59)return 2;
    d.requestSubIrq();
    if(!d.subIrqPending())return 3;
    if(d.readMemory(0xafe5)!=0 || d.subIrqPending())return 4;
    d.requestSubIrq();
    d.reset();
    if(d.subIrqPending() || d.readMemory(0xa100)!=0 ||
       d.readMemory(0xaff1)!=0 || d.nmiClockCycles()!=60000)return 5;
    // Main-side FD05 bit 6 is the original CANCEL interrupt source.
    // Lowering FD05 must not acknowledge the request; only AFE5 does.
    MainBoard board;
    board.reset();
    if(board.displaySubsystem().subIrqPending())return 6;
    board.writeIo(0xfd05,0x40);
    if(!board.displaySubsystem().subIrqPending())return 7;
    board.writeIo(0xfd05,0x00);
    if(!board.displaySubsystem().subIrqPending())return 8;
    if(board.displaySubsystem().readMemory(0xafe5)!=0 ||
       board.displaySubsystem().subIrqPending())return 9;
    board.writeIo(0xfd05,0x00);
    if(board.displaySubsystem().subIrqPending())return 10;
    board.writeIo(0xfd05,0x40);
    if(!board.displaySubsystem().subIrqPending())return 11;
    board.reset();
    if(board.displaySubsystem().subIrqPending())return 12;
    std::cout<<"PASS: sub RAM, AFE5 acknowledgment, FD05 CANCEL IRQ\\n";
    return 0;
}
