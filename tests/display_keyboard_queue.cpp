#include "DisplaySubsystem.h"
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
    std::cout<<"PASS: AFE0 initial keyboard event, AFED queue, FIRQ latch\n";
    return 0;
}
