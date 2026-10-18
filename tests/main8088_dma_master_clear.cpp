#include "Main8088Bus.h"

int main()
{
    Main8088Bus bus;
    bus.reset();
    unsigned transfers=0;
    bus.setDmaDevice(0,[&]()->BYTE {++transfers;return 0xa5;},{});
    bus.writeIo(0x1c,0); // low-byte-first flip-flop
    bus.writeIo(0x10,0x00);
    bus.writeIo(0x10,0x20); // address 2000h
    bus.writeIo(0x11,0x00);
    bus.writeIo(0x11,0x00); // one byte
    bus.writeIo(0x1b,0x04); // channel 0, device -> memory
    bus.writeIo(0x19,0x04); // software request CH0
    if(!bus.serviceDma() || transfers!=1 || bus.readRam(0x2000)!=0xa5)return 1;
    if((bus.readIo(0x18)&1)!=1)return 2; // terminal-count bit

    bus.writeIo(0x1d,0x00); // 8237 master clear
    if(bus.dmaMask()!=0x0f || bus.readIo(0x18)!=0)return 3;
    if(bus.dmaChannel(0).address!=0 || bus.dmaChannel(0).count!=0)return 4;
    bus.writeIo(0x19,0x04); // CH0 request while still masked
    if(bus.serviceDma() || transfers!=1)return 5;
    // Master clear also resets address, count and mode. Program a new
    // transfer before unmasking the channel again.
    bus.writeIo(0x10,0x01);
    bus.writeIo(0x10,0x20); // address 2001h
    bus.writeIo(0x11,0x00);
    bus.writeIo(0x11,0x00); // one byte
    bus.writeIo(0x1b,0x04); // channel 0, device -> memory
    bus.writeIo(0x1e,0); // clear mask register (unmask all)
    if(bus.dmaMask()!=0 || !bus.serviceDma() || transfers!=2 ||
       bus.readRam(0x2001)!=0xa5)return 6;
    return 0;
}
