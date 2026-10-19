#include "Main8088Bus.h"

int main()
{
    Main8088Bus bus;
    bus.reset();
    bus.writeMemory(0x2000,0x9a);
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0);
    bus.writeIo(0x10,0x20); // CH0 source
    bus.writeIo(0x12,0);
    bus.writeIo(0x12,0x30); // CH1 destination
    bus.writeIo(0x13,0);
    bus.writeIo(0x13,0); // one byte
    bus.writeIo(0x1c,0);
    bus.writeIo(0x14,0);
    bus.writeIo(0x14,0x40); // CH2 destination
    bus.writeIo(0x15,0);
    bus.writeIo(0x15,0);
    bus.writeIo(0x1b,0x46); // CH2 single mode, device to memory
    unsigned reads=0;
    bus.setDmaDevice(2,[&]()->BYTE {++reads;return 0x5c;},{});
    bus.writeIo(0x18,0x01); // memory-to-memory enabled
    bus.writeIo(0x19,0x06); // only CH2 requesting
    if(!bus.serviceDma() || reads!=1 || bus.readRam(0x4000)!=0x5c ||
       bus.readRam(0x3000)!=0)return 1;
    if(bus.serviceDma())return 2;
    bus.writeIo(0x19,0x04); // request CH0 (memory-to-memory)
    if(!bus.serviceDma() || bus.readRam(0x3000)!=0x9a)return 3;
    if((bus.readIo(0x18)&0x06)!=0x06)return 4; // CH1 and CH2 TC
    return 0;
}
