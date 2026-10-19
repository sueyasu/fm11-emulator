#include "Main6809Board.h"
#include <vector>

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
    b.reset();
    b.loadFloppy2d(std::vector<BYTE>(40 * 2 * 16 * 256, 0x5a));
    b.writeCpu(0xfd19, 0);    // track
    b.writeCpu(0xfd1a, 1);    // sector
    b.writeCpu(0xfd1c, 0);    // side
    b.writeCpu(0xfd1d, 0);    // drive
    b.writeCpu(0xfd40, 0x20); // DMA address
    b.writeCpu(0xfd41, 0);
    b.writeCpu(0xfd42, 1);    // 256 bytes
    b.writeCpu(0xfd43, 0);
    b.writeCpu(0xfd50, 0x04); // CH0 DMA enable
    b.writeCpu(0xfd02, 0xff);
    b.writeCpu(0xfd55, 0);    // all DMA IRQs disabled
    b.writeCpu(0xfd18, 0x80); // actual FDD-to-memory DMA completion
    if ((b.readCpu(0xfd55) & 0x80) == 0 || b.irqPending()) return false;
    b.writeCpu(0xfd55, 0x02); // CH1 only: CH0 completion stays masked
    if (b.irqPending()) return false;
    b.writeCpu(0xfd55, 0x01); // enable IRQ for completed CH0
    if (b.readCpu(0xfd03) != 0x10) return false;
    b.writeCpu(0xfd55, 0);    // mask without acknowledging DEND
    if (b.irqPending() || (b.readCpu(0xfd55) & 0x80) == 0) return false;
    b.writeCpu(0xfd50, 0x04); // acknowledge CH0 completion
    return (b.readCpu(0xfd55) & 0x80) == 0 && !b.irqPending();
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
