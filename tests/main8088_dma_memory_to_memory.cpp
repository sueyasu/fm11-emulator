#include "Main8088Bus.h"

static void program(Main8088Bus& bus,unsigned channel,WORD address,WORD count)
{
    bus.writeIo(0x1c,0);
    bus.writeIo(WORD(0x10+2*channel),BYTE(address));
    bus.writeIo(WORD(0x10+2*channel),BYTE(address>>8));
    bus.writeIo(WORD(0x11+2*channel),BYTE(count));
    bus.writeIo(WORD(0x11+2*channel),BYTE(count>>8));
}

int main()
{
    Main8088Bus bus;
    bus.reset();
    for(unsigned i=0;i<3;++i)bus.writeMemory(0x2000+i,BYTE(0xa1+i));
    program(bus,0,0x2000,0);
    program(bus,1,0x3000,2);
    bus.writeIo(0x18,0x01); // memory-to-memory mode
    bus.writeIo(0x19,0x04); // software request CH0
    for(int i=0;i<3;++i)if(!bus.serviceDma())return 1;
    for(unsigned i=0;i<3;++i)
        if(bus.readRam(0x3000+i)!=BYTE(0xa1+i))return 2;
    if(bus.dmaChannel(0).address!=0x2003 ||
       bus.dmaChannel(1).address!=0x3003 ||
       bus.dmaChannel(1).count!=0xffff ||
       bus.readIo(0x1d)!=0xa3)return 3;
    if((bus.readIo(0x18)&0x02)!=0x02 || bus.serviceDma())return 4;
    if((bus.readIo(0x18)&0x02)!=0)return 5;
    // Address hold repeats the same source byte into a destination range.
    program(bus,0,0x2001,0);
    program(bus,1,0x4000,1);
    bus.writeIo(0x1e,0); // unmask
    bus.writeIo(0x18,0x03); // memory-to-memory + CH0 address hold
    bus.writeIo(0x19,0x04);
    if(!bus.serviceDma() || !bus.serviceDma())return 6;
    if(bus.readRam(0x4000)!=0xa2 || bus.readRam(0x4001)!=0xa2 ||
       bus.dmaChannel(0).address!=0x2001 ||
       bus.dmaChannel(1).address!=0x4002)return 7;
    bus.writeIo(0x1d,0); // master clear resets temp register
    if(bus.readIo(0x1d)!=0)return 8;
    return 0;
}
