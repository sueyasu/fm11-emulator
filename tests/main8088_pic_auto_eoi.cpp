#include "Main8088Bus.h"

int main()
{
    Main8088Bus bus;
    bus.reset();
    // ICW1: single 8259, ICW4 required; ICW2: vector base 20h.
    bus.writeIo(0,0x13);
    bus.writeIo(1,0x20);
    bus.writeIo(1,0x03); // ICW4: 8086 mode + automatic EOI
    bus.requestIrq(3);
    bus.requestIrq(5);
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=0x23)return 1;
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=0x25)return 2;
    bus.requestIrq(5);
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=0x25)return 3;
    bus.writeIo(0,0x0b); // OCW3: read ISR
    if(bus.readIo(0)!=0)return 4;

    // Reinitializing disables AEOI unless the next ICW4 enables it.
    bus.writeIo(0,0x13);
    bus.writeIo(1,0x20);
    bus.writeIo(1,0x01); // ICW4: normal EOI
    bus.requestIrq(3);
    bus.requestIrq(5);
    if(bus.acknowledgeInterrupt()!=0x23)return 5;
    if(bus.interruptPending())return 6;
    bus.writeIo(0,0x0b);
    if(bus.readIo(0)!=0x08)return 7;
    bus.writeIo(0,0x63); // Specific EOI IRQ3
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=0x25)return 8;
    return 0;
}
