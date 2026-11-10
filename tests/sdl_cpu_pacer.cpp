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
    std::cout<<"PASS: SDL wall-time pacing, clock switching, debt and catch-up limits\n";
    return 0;
}
