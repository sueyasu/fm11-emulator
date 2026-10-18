#include "Main8088Bus.h"

int main()
{
    Main8088Bus bus;
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea; // x86 reset fetch at physical FFFF0
    rom[0xff1]=0x00;
    bus.loadBootRom(rom);
    if(bus.readMemory(0xffff0)!=0xea)return 1;
    if(bus.readMemory(0xffff1)!=0)return 2;
    bus.writeMemory(0xffff0,0x12);
    if(bus.readMemory(0xffff0)!=0xea || bus.readRam(0xffff0)!=0x12)return 3;
    bus.writeMemory(0x100020,0x77);
    if(bus.readMemory(0x20)!=0x77)return 4;
    bus.writeIo(0xfc50,0xa5);
    if(bus.readIo(0xfc50)!=0xa5)return 5;
    WORD lastPort=0; BYTE lastValue=0;
    bus.setSystemIo([](WORD port){return port==0xfd18 ? BYTE(0x42):BYTE(0);},
                    [&](WORD port,BYTE value){lastPort=port;lastValue=value;});
    if(bus.readIo(0xfd18)!=0x42)return 6;
    bus.writeIo(0xfd18,0x80);
    if(lastPort!=0xfd18 || lastValue!=0x80)return 7;
    if(bus.readIo(0xfd18)!=0x42)return 8;
    bus.reset();
    if(bus.readIo(0xfc50)!=0)return 9;
    if(bus.readMemory(0xffff0)!=0xea)return 10;
    // ICW1..ICW4 programming, mask, IRR/ISR and EOI.
    bus.writeIo(0x00,0x13); // edge triggered, single 8259, ICW4 required
    bus.writeIo(0x01,0x28); // vectors 28h..2Fh
    bus.writeIo(0x01,0x01); // 8086 mode
    bus.writeIo(0x01,0xfe); // only IRQ0 unmasked
    bus.requestIrq(1);
    if(bus.interruptPending() || bus.readIo(0x01)!=0xfe)return 11;
    bus.requestIrq(0);
    if(!bus.interruptPending() || bus.readIo(0x00)!=0x03)return 12;
    if(bus.acknowledgeInterrupt()!=0x28 || bus.interruptPending())return 13;
    bus.writeIo(0x00,0x0b); // OCW3: read ISR
    if(bus.readIo(0x00)!=0x01)return 14;
    bus.writeIo(0x00,0x20); // non-specific EOI
    if(bus.readIo(0x00)!=0)return 15;
    bus.writeIo(0x01,0xfd); // only IRQ1 unmasked
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=0x29)return 16;
    bus.writeIo(0x00,0x61); // specific EOI IRQ1
    if(bus.readIo(0x00)!=0)return 17;
    bus.reset();
    if(bus.readIo(0x01)!=0 || bus.interruptPending())return 18;
    // Sim86-compatible 8237 DMA programming on ports 10h..23h.
    bus.writeIo(0x1c,0);
    bus.writeIo(0x14,0x78);
    bus.writeIo(0x14,0x56);
    bus.writeIo(0x15,0x34);
    bus.writeIo(0x15,0x12);
    bus.writeIo(0x22,0x0a);
    bus.writeIo(0x1b,0x4a); // channel 2, single mode, I/O -> memory
    if(bus.dmaChannel(2).address!=0x5678 ||
       bus.dmaChannel(2).count!=0x1234 ||
       bus.dmaChannel(2).baseCount!=0x1234 ||
       bus.dmaChannel(2).bank!=0x0a ||
       bus.dmaChannel(2).mode!=0x4a)return 19;
    bus.writeIo(0x1c,0);
    if(bus.readIo(0x14)!=0x78 || bus.readIo(0x14)!=0x56 ||
       bus.readIo(0x15)!=0x34 || bus.readIo(0x15)!=0x12)return 20;
    if(bus.readIo(0x22)!=0x0a)return 21;
    bus.writeIo(0x1f,0x0f);
    bus.writeIo(0x1a,0x02); // unmask channel 2
    if(bus.dmaMask()!=0x0b)return 22;
    bus.writeIo(0x1a,0x06); // mask channel 2
    if(bus.dmaMask()!=0x0f)return 23;
    bus.writeIo(0x1e,0);
    if(bus.dmaMask()!=0)return 24;
    bus.writeIo(0x19,0x06); // set request ch2
    if(bus.readIo(0x18)!=0x40)return 25;
    bus.writeIo(0x19,0x02); // clear request ch2
    if(bus.readIo(0x18)!=0)return 26;
    bus.writeIo(0x1d,0); // master clear
    if(bus.dmaChannel(2).address!=0 || bus.dmaChannel(2).bank!=0 ||
       bus.dmaMask()!=0)return 27;
    return 0;
}
