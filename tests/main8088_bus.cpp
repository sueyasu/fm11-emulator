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
    return 0;
}
