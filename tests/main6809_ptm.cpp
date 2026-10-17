#include "Main6809Board.h"
#include <cstdint>

namespace {
void startFirst(Main6809Board& b, BYTE control, WORD count)
{
    b.writeCpu(0xfd39, 0x01); // FD38 controls channel 1
    b.writeCpu(0xfd38, control);
    b.writeCpu(0xfd3a, static_cast<BYTE>(count >> 8));
    b.writeCpu(0xfd3b, static_cast<BYTE>(count));
}

bool statusAckPreservesExpiry()
{
    Main6809Board b;
    b.writeCpu(0xfd02, 0x10);
    startFirst(b, 0x50, 4);
    b.advancePtm(4);
    if (!b.irqPending() || b.readCpu(0xfd39) != 0x81) return false;
    if (b.irqPending() || b.readCpu(0xfd39) != 0x81) return false;
    (void)b.readCpu(0xfd3a); // acknowledge channel-1 flag
    if (b.readCpu(0xfd39) != 0) return false;
    b.advancePtm(4);
    return b.irqPending() && b.readCpu(0xfd39) == 0x81;
}

bool periodAndLargeTicks()
{
    Main6809Board b;
    startFirst(b, 0x50, 7);
    b.advancePtm(3);
    if (b.readCpu(0xfd3a) != 0 || b.readCpu(0xfd3b) != 4) return false;
    b.advancePtm(7000000);
    return b.readCpu(0xfd3a) == 0 && b.readCpu(0xfd3b) == 4;
}

bool oneShot()
{
    Main6809Board b;
    b.writeCpu(0xfd02, 0x10);
    startFirst(b, 0x70, 3); // single-shot start
    b.advancePtm(3);
    if (!b.irqPending()) return false;
    b.readCpu(0xfd39);
    b.readCpu(0xfd3a);
    b.advancePtm(30);
    return !b.irqPending() && b.readCpu(0xfd39) == 0;
}

bool independentChannels()
{
    Main6809Board b;
    startFirst(b, 0x50, 4);
    b.writeCpu(0xfd39, 0x50); // channel 2 control
    b.writeCpu(0xfd3c, 0);
    b.writeCpu(0xfd3d, 6);
    b.advancePtm(3);
    if (b.readCpu(0xfd3b) != 1 || b.readCpu(0xfd3d) != 3) return false;
    b.advancePtm(1);
    if (b.readCpu(0xfd3b) != 4 || b.readCpu(0xfd3d) != 2) return false;
    b.reset();
    return b.readCpu(0xfd39) == 0 && !b.irqPending();
}
} // namespace

int main()
{
    if (!statusAckPreservesExpiry()) return 1;
    if (!periodAndLargeTicks()) return 2;
    if (!oneShot()) return 3;
    if (!independentChannels()) return 4;
    return 0;
}
