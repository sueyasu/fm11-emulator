#include "Main8088Bus.h"

int main()
{
    Main8088Bus bus;
    bus.reset();
    unsigned cascadeReads=0;
    unsigned normalReads=0;
    bus.setDmaDevice(0,[&]()->BYTE {++cascadeReads;return 0xaa;},{});
    bus.setDmaDevice(1,[&]()->BYTE {++normalReads;return 0x5a;},{});
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0x00);
    bus.writeIo(0x10,0x20); // CH0 address 2000h
    bus.writeIo(0x11,0x00);
    bus.writeIo(0x11,0x00); // CH0 count 0
    bus.writeIo(0x1b,0xc4); // CH0 cascade, nominal device-to-memory
    bus.writeIo(0x19,0x04); // CH0 request
    if(bus.serviceDma() || cascadeReads!=0 ||
       bus.dmaChannel(0).address!=0x2000 ||
       bus.dmaChannel(0).count!=0 ||
       bus.readRam(0x2000)!=0)return 1;
    if((bus.readIo(0x18)&0x11)!=0x10)return 2; // request still pending, no TC

    bus.writeIo(0x1c,0);
    bus.writeIo(0x12,0x00);
    bus.writeIo(0x12,0x30); // CH1 address 3000h
    bus.writeIo(0x13,0x00);
    bus.writeIo(0x13,0x00); // CH1 count 0
    bus.writeIo(0x1b,0x45); // CH1 single, device-to-memory
    bus.writeIo(0x19,0x05); // CH1 request
    if(!bus.serviceDma() || normalReads!=1 || cascadeReads!=0 ||
       bus.readRam(0x3000)!=0x5a)return 3;
    if((bus.readIo(0x18)&0x13)!=0x12)return 4; // CH1 TC, CH0 request
    if(bus.serviceDma() || cascadeReads!=0)return 5;
    // Reprogram CH0 to single transfer without dropping its request.
    bus.writeIo(0x1b,0x44);
    if(!bus.serviceDma() || cascadeReads!=1 ||
       bus.readRam(0x2000)!=0xaa)return 6;
    return 0;
}
