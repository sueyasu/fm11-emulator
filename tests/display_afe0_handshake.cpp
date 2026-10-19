#include "MainBoard.h"
#include <iostream>

int main() {
    MainBoard board;
    auto& sub=board.displaySubsystem();
    board.reset();
    // Original SubCpu.cpp: BUSY after reset; AFE0 bit 0 reads clear.
    if(sub.busyStatus()!=0x80 || (sub.readMemory(0xafe0)&1u) ||
       board.subCpuStatus()!=0x80)return 1;
    sub.writeMemory(0xafe0,0x85); // READY, other bits preserved
    if(sub.busyStatus()!=0 || sub.readMemory(0xafe0)!=0x85 ||
       board.subCpuStatus()!=0)return 2;
    sub.writeMemory(0xafe0,0x84); // BUSY
    if(sub.busyStatus()!=0x80 || sub.readMemory(0xafe0)!=0x84 ||
       board.subCpuStatus()!=0x80)return 3;
    sub.writeMemory(0xaff0,0x42); // DMA register, not READY
    if(sub.readMemory(0xaff0)!=0x42 || sub.busyStatus()!=0x80 ||
       sub.readMemory(0xafe0)!=0x84 || board.subCpuStatus()!=0x80)return 4;
    sub.writeMemory(0xafe0,0x01);
    if(sub.busyStatus()!=0 || sub.readMemory(0xafe0)!=1 ||
       board.subCpuStatus()!=0)return 5;
    board.reset();
    if(sub.busyStatus()!=0x80 || (sub.readMemory(0xafe0)&1u) ||
       board.subCpuStatus()!=0x80)return 6;
    std::cout<<"PASS: sub AFE0 BUSY/READY -> main FD05 status; separate AFF0 DMA\n";
    return 0;
}
