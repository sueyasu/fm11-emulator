#include "DisplaySubsystem.h"
int main() {
    DisplaySubsystem sub;
    sub.reset();
    // At reset the original GVU has no enabled write planes:
    // writing clears all three physical plane bytes.
    sub.writeMemory(0x0010,0xff);
    for(unsigned plane=0;plane<3;++plane)
        if(sub.readPhysicalVram(plane*0x10000+0x10)!=0)return 1;
    sub.writeMemory(0xafe1,0x07); // write RGB planes, read plane 0
    sub.writeMemory(0x0010,0x11);
    for(unsigned plane=0;plane<3;++plane)
        if(sub.readPhysicalVram(plane*0x10000+0x10)!=0x11)return 2;
    sub.writeMemory(0xafe3,0x08); // GVUpg = 1
    sub.writeMemory(0xafe8,0x7f);
    sub.writeMemory(0xafe9,0xfe); // GVUad = 7FFE
    sub.writeMemory(0x0003,0x44); // offset wraps at 7FFF
    if(sub.readPhysicalVram(0x4001)!=0x44)return 3;
    sub.writeMemory(0xafe1,0x87); // read blue plane, write all
    sub.writeMemory(0x0003,0x55);
    if(sub.readMemory(0x0003)!=0x55)return 4;
    sub.writeMemory(0xafe2,0x18); // GVDpg = 3, independent of GVUpg
    if(sub.graphicsDisplayPage()!=3 || sub.graphicsAccessPage()!=1)return 5;
    return 0;
}
