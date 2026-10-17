#include "Main6809Board.h"

namespace {

bool mainMasks()
{
    Main6809Board b;
    b.setMainIrqSource(0x40, true); // keyboard
    if (b.irqPending() || b.readCpu(0xfd03) != 0)
        return false;
    b.writeCpu(0xfd02, 0x80);
    if (!b.irqPending() || b.readCpu(0xfd03) != 0x40)
        return false;
    b.setMainIrqSource(0x40, false);
    return !b.irqPending() && b.readCpu(0xfd03) == 0;
}

bool serialMasks()
{
    Main6809Board b;
    b.setMainIrqSource(0x80, true);
    b.writeCpu(0xfd02, 0x01);
    if (b.irqPending()) return false;
    b.writeCpu(0xfd02, 0x02);
    return b.readCpu(0xfd03) == 0x80;
}

bool individualMasks()
{
    constexpr BYTE bits[3] = {0x08, 0x02, 0x01};
    constexpr BYTE masks[3] = {0x20, 0x01, 0x10};
    for (int i = 0; i < 3; ++i) {
        Main6809Board b;
        b.setMainIrqSource(bits[i], true);
        b.writeCpu(0xfd02, masks[i]);
        if (b.readCpu(0xfd03) != bits[i]) return false;
        b.writeCpu(0xfd02, 0);
        if (b.irqPending()) return false;
    }
    return true;
}

bool dmaUsesFd55Mask()
{
    Main6809Board b;
    b.setMainIrqSource(0x10, true);
    b.writeCpu(0xfd02, 0xff);
    if (b.irqPending()) return false;
    b.writeCpu(0xfd55, 0x01);
    if (b.readCpu(0xfd03) != 0x10) return false;
    b.writeCpu(0xfd55, 0);
    return !b.irqPending();
}

bool extendedAggregationAndAck()
{
    Main6809Board b;
    b.setExtendedIrqSource(0x01, true); // LPT1
    if (b.irqPending()) return false;
    b.writeCpu(0xfd2c, 0x01);
    if (b.readCpu(0xfd03) != 0x20 || !b.irqPending()) return false;
    if (b.readCpu(0xfd2c) != 0x01) return false;
    // FD2C read clears the visible latch; the device's pending source remains.
    if (b.readCpu(0xfd03) != 0x20) return false;
    b.setExtendedIrqSource(0x01, false);
    return b.readCpu(0xfd03) == 0 && !b.irqPending();
}

bool hddGatedByFdC2()
{
    Main6809Board b;
    b.setExtendedIrqSource(0x04, true);
    b.writeCpu(0xfd2c, 0xff);
    if (b.irqPending()) return false;
    b.writeCpu(0xfdc2, 0x80);
    if (b.readCpu(0xfd2c) != 0x04 || !b.irqPending()) return false;
    b.writeCpu(0xfdc2, 0);
    return !b.irqPending();
}

bool resetClearsIrqState()
{
    Main6809Board b;
    b.writeCpu(0xfd02, 0x80);
    b.setMainIrqSource(0x40, true);
    b.writeCpu(0xfd2c, 1);
    b.setExtendedIrqSource(1, true);
    b.reset();
    return !b.irqPending() && b.readCpu(0xfd03) == 0 &&
           b.readCpu(0xfd2c) == 0;
}
} // namespace

int main()
{
    if (!mainMasks()) return 1;
    if (!serialMasks()) return 2;
    if (!individualMasks()) return 3;
    if (!dmaUsesFd55Mask()) return 4;
    if (!extendedAggregationAndAck()) return 5;
    if (!hddGatedByFdC2()) return 6;
    if (!resetClearsIrqState()) return 7;
    return 0;
}
