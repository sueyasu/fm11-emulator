#include "Main8088Bus.h"

int main()
{
    Main8088Bus bus;
    bus.reset();
    bus.writeMemory(0x2100,0x31);
    bus.writeMemory(0x2101,0x32);
    bus.writeMemory(0x2102,0x33);
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0x00);bus.writeIo(0x10,0x21); // CH0 src 2100h
    bus.writeIo(0x11,0x00);bus.writeIo(0x11,0x00);
    bus.writeIo(0x12,0x00);bus.writeIo(0x12,0x31); // CH1 dst 3100h
    bus.writeIo(0x13,0x02);bus.writeIo(0x13,0x00); // 3 bytes
    bus.writeIo(0x1b,0x81); // CH1 block mode
    bus.writeIo(0x18,0x01); // memory-to-memory
    bus.setDmaRequest(0,true);
    if(!bus.serviceDma() || bus.readRam(0x3100)!=0x31)return 1;
    bus.setDmaRequest(0,false);
    if(!bus.serviceDma() || !bus.serviceDma())return 2;
    if(bus.readRam(0x3101)!=0x32 || bus.readRam(0x3102)!=0x33 ||
       bus.dmaChannel(1).count!=0xffff || (bus.dmaMask()&3)!=3)return 3;
    if(bus.serviceDma())return 4;
    if((bus.readIo(0x18)&0x02)==0)return 5;
    return 0;
}
