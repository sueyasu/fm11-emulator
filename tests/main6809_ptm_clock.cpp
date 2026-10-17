#include "Main6809Board.h"

#include <stdexcept>

namespace {
void setup(Main6809Board& b)
{
    b.writeCpu(0xfd02, 0x10);
    b.writeCpu(0xfd39, 0x01);
    b.writeCpu(0xfd38, 0x50);
    b.writeCpu(0xfd3a, 0);
    b.writeCpu(0xfd3b, 4);
}

bool fractionalCpuClock()
{
    Main6809Board b;
    setup(b);
    b.setCpuClockHz(19400); // exactly two CPU cycles per PTM input tick
    b.advanceCpuCycles(7);
    if (b.irqPending()) return false;
    b.advanceCpuCycles(1);
    if (!b.irqPending()) return false;
    if (b.readCpu(0xfd39) != 0x81) return false;
    b.advanceCpuCycles(7);
    if (b.irqPending()) return false;
    b.advanceCpuCycles(1);
    return b.irqPending();
}

bool chunkIndependence()
{
    Main6809Board a, b;
    setup(a);
    setup(b);
    a.setCpuClockHz(2000000);
    b.setCpuClockHz(2000000);
    a.advanceCpuCycles(824); // 3 PTM ticks and a fractional remainder
    for (int i = 0; i < 824; ++i)
        b.advanceCpuCycles(1);
    if (a.readCpu(0xfd3a) != b.readCpu(0xfd3a)) return false;
    if (a.readCpu(0xfd3b) != b.readCpu(0xfd3b)) return false;
    a.advanceCpuCycles(1);
    b.advanceCpuCycles(1);
    return a.irqPending() == b.irqPending();
}

bool noSilentClockAssumption()
{
    Main6809Board b;
    bool throws = false;
    try { b.advanceCpuCycles(1); }
    catch (const std::logic_error&) { throws = true; }
    if (!throws) return false;
    b.setCpuClockHz(9700);
    setup(b);
    b.advanceCpuCycles(3);
    if (b.irqPending()) return false;
    b.advanceCpuCycles(1);
    if (!b.irqPending()) return false;
    b.reset();
    b.advanceCpuCycles(4); // clock rate remains configured; phase reset
    return !b.irqPending();
}
} // namespace

int main()
{
    if (!fractionalCpuClock()) return 1;
    if (!chunkIndependence()) return 2;
    if (!noSilentClockAssumption()) return 3;
    return 0;
}
