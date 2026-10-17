#include "Main6809Board.h"
#include <cstdio>

int main()
{
    Main6809Board b;
    b.writeCpu(0xfd02, 0x10); // PTM mask
    b.writeCpu(0xfd39, 0x01); // select channel 1 via channel 2 bit 0
    b.writeCpu(0xfd38, 0x50); // channel 1: continuous, IRQ enabled
    b.writeCpu(0xfd3a, 0x00);
    b.writeCpu(0xfd3b, 0x04); // 4 PTM ticks
    b.advancePtm(3);
    if (b.irqPending()) return 1;
    b.advancePtm(1);
    if (!b.irqPending() || b.readCpu(0xfd03) != 0x01) { std::fprintf(stderr, "PTM: IRQ not asserted on expiry\\n"); return 2; }
    if (b.readCpu(0xfd39) != 0x81) return 3;
    if (b.irqPending()) return 4;
    b.advancePtm(4);
    if (!b.irqPending()) return 5;
    b.readCpu(0xfd39);
    b.writeCpu(0xfd02, 0);
    b.advancePtm(4);
    if (b.irqPending()) return 6;
    // A second channel must advance independently of the first.
    b.writeCpu(0xfd39, 0x50); // start channel 2
    b.writeCpu(0xfd3c, 0);
    b.writeCpu(0xfd3d, 6);
    b.advancePtm(3);
    if (b.readCpu(0xfd3c) != 0 || b.readCpu(0xfd3d) != 3) return 8;
    b.reset();
    if (b.readCpu(0xfd39) != 0 || b.irqPending()) return 7;
    return 0;
}
