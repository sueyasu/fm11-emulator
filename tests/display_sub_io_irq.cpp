#include "DisplaySubsystem.h"
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
    std::cout<<"PASS: original sub RAM aperture and AFE5 IRQ acknowledge\n";
    return 0;
}
