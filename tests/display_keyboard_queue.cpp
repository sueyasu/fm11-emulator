#include "DisplaySubsystem.h"
#include "Machine.h"
#include "Main6809Board.h"
#include <memory>
#include <iostream>
int main() {
    DisplaySubsystem d;
    d.reset();
    if(!d.nmiMask())return 1;
    d.writeMemory(0xafe0,0x01);
    if(d.nmiMask() || d.busyStatus()!=0)return 2;
    d.processKeyCode();
    if(!d.subFirqPending() || d.readMemory(0xafec)!=0 ||
       d.readMemory(0xafed)!=0x50)return 3;
    if(d.readMemory(0xafe4)!=1)return 4;
    d.enqueueKeyCode(0x0041);
    d.processKeyCode();
    if(d.readMemory(0xafed)!=0x41 || d.readMemory(0xafe4)!=1)return 5;
    d.reset();
    if(!d.nmiMask() || d.subFirqPending() ||
       d.readMemory(0xafec)!=0 || d.readMemory(0xafed)!=0)return 6;
    // A queued SDL character must wake the sub keyboard without a
    // speculative AFED read; the firmware sees AFE4 bit 0.
    d.enqueueKeyCode(0x0042);
    if((d.readMemory(0xafe4)&1)==0 || d.readMemory(0xafed)!=0x42)return 7;
    // In main-keyboard mode, FD00/FD01 carry two-byte key codes;
    // reading FD01 acknowledges IRQ and presents the next code.
    Machine machine;
    auto board=std::make_unique<Main6809Board>();
    auto* main=board.get();
    machine.addBoard(std::move(board));
    machine.reset();
    main->writeCpu(0xfd02,0x80);
    machine.enqueueDisplayKeyCode(0x0041);
    machine.enqueueDisplayKeyCode(0x0042);
    main->syncPtmIrq();
    if((main->readCpu(0xfd03)&0x40)==0 || main->readCpu(0xfd01)!=0x41)return 8;
    if((main->readCpu(0xfd03)&0x40)==0 || main->readCpu(0xfd01)!=0x42)return 9;
    if((main->readCpu(0xfd03)&0x40)!=0)return 10;
    std::cout<<"PASS: AFE0 initial keyboard event, AFED queue, FIRQ latch\n";
    return 0;
}
