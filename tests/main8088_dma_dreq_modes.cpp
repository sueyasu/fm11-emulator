#include "Main8088Bus.h"

int main()
{
    Main8088Bus bus;
    bus.reset();
    unsigned n=0;
    bus.setDmaDevice(0,[&]()->BYTE {return BYTE(++n);},{});
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0);
    bus.writeIo(0x10,0x20);
    bus.writeIo(0x11,2); // three bytes
    bus.writeIo(0x11,0);
    bus.writeIo(0x1b,0x04); // demand, I/O -> memory
    bus.setDmaRequest(0,true);
    if(!bus.serviceDma() || n!=1 || bus.readRam(0x2000)!=1)return 1;
    bus.setDmaRequest(0,false);
    if(bus.serviceDma() || n!=1 || bus.dmaChannel(0).count!=1)return 2;
    bus.setDmaRequest(0,true);
    if(!bus.serviceDma() || n!=2)return 3;
    bus.setDmaRequest(0,false);
    if(bus.serviceDma())return 4;
    bus.setDmaRequest(0,true);
    if(!bus.serviceDma() || n!=3 || bus.dmaChannel(0).count!=0xffff)return 5;
    bus.setDmaRequest(0,false);
    if(bus.serviceDma())return 6;
    // Reinitialize CH0 for Block mode.
    bus.writeIo(0x1a,0); // unmask
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0);
    bus.writeIo(0x10,0x30);
    bus.writeIo(0x11,2);
    bus.writeIo(0x11,0);
    bus.writeIo(0x1b,0x84); // block, I/O -> memory
    bus.setDmaRequest(0,true);
    if(!bus.serviceDma() || n!=4)return 7;
    bus.setDmaRequest(0,false);
    if(!bus.serviceDma() || !bus.serviceDma() || n!=6 ||
       bus.readRam(0x3002)!=6)return 8;
    if(bus.serviceDma())return 9;
    // Single mode stops on DREQ removal; it does not latch a block.
    bus.writeIo(0x1a,0);
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0);
    bus.writeIo(0x10,0x40);
    bus.writeIo(0x11,1);
    bus.writeIo(0x11,0);
    bus.writeIo(0x1b,0x44); // single, I/O -> memory
    bus.setDmaRequest(0,true);
    if(!bus.serviceDma() || n!=7)return 10;
    bus.setDmaRequest(0,false);
    if(bus.serviceDma() || n!=7)return 11;
    return 0;
}
