#include "DisplaySubsystem.h"
#include <iostream>

int main() {
    DisplaySubsystem sub;
    sub.reset();
    if(sub.readMemory(0xafe4)!=0 || sub.readMemory(0xafec)!=0 ||
       sub.readMemory(0xafed)!=0)return 1;

    // Reference GetIoByte clears each latch after returning its old value.
    sub.signalSubFirq(0x40);
    sub.setKeyboardLatches(0x23,0x61);
    if(sub.readMemory(0xafe4)!=0x40 || sub.readMemory(0xafe4)!=0)return 2;
    if(sub.readMemory(0xafec)!=0x23 || sub.readMemory(0xafec)!=0)return 3;
    if(sub.readMemory(0xafed)!=0x61 || sub.readMemory(0xafed)!=0)return 4;

    // Reference SetIoByte ignores writes to AFEC/AFED and (absent any
    // external event) AFE4; the emulator must not fabricate key events.
    sub.writeMemory(0xafec,0xff);
    sub.writeMemory(0xafed,0xff);
    sub.writeMemory(0xafe4,0xff);
    if(sub.readMemory(0xafec)!=0 || sub.readMemory(0xafed)!=0 ||
       sub.readMemory(0xafe4)!=0)return 5;

    sub.signalSubFirq(0x11);
    sub.setKeyboardLatches(0x22,0x33);
    sub.reset();
    if(sub.readMemory(0xafe4)!=0 || sub.readMemory(0xafec)!=0 ||
       sub.readMemory(0xafed)!=0)return 6;
    std::cout<<"PASS: SubCpu read-clear latches AFE4 AFEC AFED\n";
    return 0;
}
