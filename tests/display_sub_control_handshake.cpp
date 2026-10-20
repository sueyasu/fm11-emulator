#include "MainBoard.h"
int main() {
    MainBoard main;
    auto& sub=main.displaySubsystem();
    // FD04 writes arm the main command latch (Fm11Ram[004]).
    if(!main.writeIo(0xfd04,0x80))return 1;
    if(sub.readMemory(0xaff3)!=0x80)return 2;
    // Sub AFE7 read sets ATTENTION at main FD04 and clears command.
    if(sub.readMemory(0xafe7)!=0)return 3;
    if(sub.readMemory(0xaff3)!=0)return 4;
    if((main.eventStatus()&1)==0)return 5;
    BYTE events=0;
    if(!main.readIo(0xfd04,events) || (events&1)==0)return 6;
    if((main.eventStatus()&1)!=0)return 7;
    // FD05 CANCEL sets the IRQ latch; sub acknowledges via AFE5.
    if(!main.writeIo(0xfd05,0x40))return 8;
    if(!sub.subIrqPending())return 9;
    if(sub.readMemory(0xafe5)!=0 || sub.subIrqPending())return 10;
    // AFE0 busy/ready and Fm11Ram[105] suspension match SubCpu.
    sub.writeMemory(0xafe0,0);
    if((sub.readMemory(0xafe0)&1)!=0)return 11;
    sub.writeMemory(0xafe0,1);
    if((sub.readMemory(0xafe0)&1)==0)return 12;
    if(!sub.subWaitForInterrupt())return 13;
    return 0;
}
