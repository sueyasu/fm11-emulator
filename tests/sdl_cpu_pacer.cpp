#include "SdlCpuPacer.h"
#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    using namespace std::chrono;
    SdlCpuPacer p;
    if(p.ready())return 1;
    p.addElapsed(nanoseconds(1000)); // 1 us
    if(!p.ready() || p.creditPs()!=1000000)return 2;
    p.consume(4,4000000); // one Z80 NOP: 1 us
    if(p.ready() || p.creditPs()!=0)return 3;

    p.addElapsed(nanoseconds(2000));
    p.consume(4,2000000); // one 6809 operation: 2 us
    if(p.creditPs()!=0)return 4;

    p.addElapsed(nanoseconds(3000));
    p.consume(4,4000000); // Z80: 1 us
    p.consume(4,2000000); // 6809: 2 us (FD05 handoff)
    if(p.creditPs()!=0)return 5;

    // An instruction may slightly overrun the current credit; retain that
    // negative debt instead of executing extra instructions.
    p.addElapsed(nanoseconds(500));
    p.consume(4,4000000);
    if(p.creditPs()!=-500000 || p.ready())return 6;
    p.addElapsed(nanoseconds(500));
    if(p.ready() || p.creditPs()!=0)return 7;

    // Rendering pauses should not accumulate unbounded catch-up work.
    p.addElapsed(seconds(5));
    if(p.creditPs()!=SdlCpuPacer::maxLagPs)return 8;
    p.addElapsed(milliseconds(5));
    if(p.creditPs()!=SdlCpuPacer::maxLagPs)return 9;

    // Atomic Z80 repeat instructions still cost their real emulated
    // cycles in Machine, but host-side debt stays bounded.
    p.consume(20000000,4000000);
    if(p.creditPs()!=0)return 10;
    p.consume(20000000,4000000);
    if(p.creditPs()!=-SdlCpuPacer::maxLagPs)return 11;
    p.reset();
    if(p.creditPs()!=0 || p.ready())return 12;

    // Non-integral picoseconds per clock must be safely handled.
    p.addElapsed(nanoseconds(1000));
    p.consume(1,3333333);
    if(p.creditPs()<=0 || p.creditPs()>=1000000)return 13;
    // Speed is a guest-time multiplier; 20 ms of wall lag at 10x
    // corresponds to 200 ms of guest budget, not just 20 ms.
    SdlCpuPacer fast(10);
    fast.addElapsed(milliseconds(10));
    if(fast.creditPs()!=SdlCpuPacer::maxLagPs*5)return 14;
    fast.consume(400000,4000000); // 0.1 guest second
    if(fast.creditPs()!=0)return 15;
    fast.addElapsed(milliseconds(50)); // bounded to 20 ms of host lag
    if(fast.creditPs()!=SdlCpuPacer::maxLagPs*10)return 16;
    fast.consume(800000,4000000);
    if(fast.creditPs()!=0)return 17;
    fast.addElapsed(milliseconds(5)); // a VSYNC-like delay
    if(fast.creditPs()!=SdlCpuPacer::maxLagPs*10/4)return 18;
    fast.consume(1000000,4000000); // 250 ms debt: clipped to 200 ms
    if(fast.creditPs()!=-SdlCpuPacer::maxLagPs*10*3/4)return 19;

    // "unlimited" must never block the guest or modify emulated clocks.
    SdlCpuPacer unlimited(0);
    if(!unlimited.ready())return 20;
    unlimited.addElapsed(seconds(4));
    unlimited.consume(4000000,4000000);
    if(!unlimited.ready() || unlimited.creditPs()!=0)return 21;
    if(SdlCpuPacer(1).maxGuestLagPs()!=SdlCpuPacer::maxLagPs)return 22;
    try {
        SdlCpuPacer invalid(101);
        return 23;
    } catch(const std::invalid_argument&) {
    }
    std::cout<<"PASS: SDL wall-time pacing, clock switching, debt and catch-up limits\n";
    return 0;
}
