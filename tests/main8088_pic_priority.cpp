#include "Main8088Bus.h"

int main()
{
    Main8088Bus bus;
    bus.reset();
    bus.requestIrq(3);
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=11)return 1;
    // Same and lower priorities must wait while IRQ3 is in service.
    bus.requestIrq(3);
    bus.requestIrq(5);
    if(bus.interruptPending())return 2;
    // IRQ1 may preempt IRQ3.
    bus.requestIrq(1);
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=9)return 3;
    // IRQ2 is blocked by IRQ1 until an EOI.
    bus.requestIrq(2);
    if(bus.interruptPending())return 4;
    bus.writeIo(0x00,0x61); // Specific EOI for IRQ1
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=10)return 5;
    bus.writeIo(0x00,0x62); // Specific EOI for IRQ2
    if(bus.interruptPending())return 6; // IRQ3 still in service
    bus.writeIo(0x00,0x63); // Specific EOI for IRQ3
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=11)return 7;
    bus.writeIo(0x00,0x63);
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=13)return 8;
    bus.writeIo(0x00,0x65);
    if(bus.interruptPending())return 9;
    return 0;
}
