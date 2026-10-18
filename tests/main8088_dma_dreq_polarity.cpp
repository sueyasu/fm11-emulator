#include "Main8088Bus.h"
int main()
{
    Main8088Bus bus;
    bus.reset();
    unsigned transfers=0;
    bus.setDmaDevice(0,[&]()->BYTE {return BYTE(++transfers);},{});
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0);bus.writeIo(0x10,0x20);
    bus.writeIo(0x11,3);bus.writeIo(0x11,0);
    bus.writeIo(0x1b,0x04);
    bus.writeIo(0x18,0x40); // active-low DREQ
    if(bus.readIo(0x18)!=0 || bus.serviceDma())return 1;
    bus.setDmaRequest(0,true); // high: inactive
    if(bus.readIo(0x18)!=0 || bus.serviceDma())return 2;
    bus.setDmaRequest(0,false); // low: active
    if((bus.readIo(0x18)&0xf0)!=0x10 || !bus.serviceDma() ||
       transfers!=1 || bus.readRam(0x2000)!=1)return 3;
    bus.setDmaRequest(0,true);
    if(bus.serviceDma() || (bus.readIo(0x18)&0xf0))return 4;
    bus.writeIo(0x18,0); // active-high DREQ
    if((bus.readIo(0x18)&0xf0)!=0x10 || !bus.serviceDma() ||
       transfers!=2)return 5;
    bus.setDmaRequest(0,false);
    if(bus.serviceDma())return 6;
    bus.writeIo(0x18,0x40);
    bus.writeIo(0x19,0x04); // software request bypasses polarity
    if((bus.readIo(0x18)&0xf0)!=0x10 || !bus.serviceDma() ||
       transfers!=3)return 7;
    return 0;
}
