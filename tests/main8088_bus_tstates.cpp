#include "Intel8088BusTStates.h"
#include <iostream>

int main() {
    Intel8088BusTStates bus;
    auto s=bus.snapshot();
    if(s.elapsed || s.partial)return 1;
    // Four three-clock execution windows allow three four-T-state
    // instruction byte bus transfers. Remainders cross instructions.
    if(bus.advance(3,0,4)!=0 || bus.snapshot().partial!=3)return 2;
    if(bus.advance(3,0,4)!=1 || bus.snapshot().partial!=2)return 3;
    if(bus.advance(3,0,4)!=1 || bus.snapshot().partial!=1)return 4;
    if(bus.advance(3,0,4)!=1 || bus.snapshot().partial!=0)return 5;
    s=bus.snapshot();
    if(s.elapsed!=12 || s.prefetch!=12 || s.operand!=0)return 6;

    // One word operand occupies two independent four-clock bus
    // transfers. The remaining ten clocks grant two prefetch bytes
    // and carry a two-T-state partial phase.
    if(bus.advance(18,2,4)!=2)return 7;
    s=bus.snapshot();
    if(s.elapsed!=30 || s.operand!=8 || s.prefetch!=20 ||
       s.partial!=2)return 8;
    // An operand bus cycle interrupts that incomplete prefetch phase.
    if(bus.advance(8,1,4)!=1)return 9;
    s=bus.snapshot();
    if(s.operand!=12 || s.interrupted!=2 || s.partial!=0 ||
       s.prefetch!=24)return 10;

    // Full FIFO means no speculative transaction. Remaining clocks
    // are idle and must not be banked for a future refill.
    if(bus.advance(9,0,0)!=0 || bus.snapshot().partial!=0 ||
       bus.snapshot().idle!=9)return 11;
    if(bus.advance(3,0,4)!=0 || bus.snapshot().partial!=3)return 12;
    bus.flush(); // branch discards an incomplete fetch
    if(bus.snapshot().partial!=0 ||
       bus.snapshot().interrupted!=5)return 13;
    if(bus.advance(3,0,4)!=0 || bus.snapshot().partial!=3)return 14;

    bus.reset();
    s=bus.snapshot();
    if(s.elapsed || s.partial || s.interrupted ||
       s.operand || s.prefetch || s.idle)return 15;
    return 0;
}
