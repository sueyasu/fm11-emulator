#include "CpuSpeedMeter.h"
#include <chrono>
#include <cmath>
#include <iostream>

int main() {
    using namespace std::chrono;
    CpuSpeedMeter m;
    m.record(CpuSpeedMeter::Card::Main6809,250000);
    m.record(CpuSpeedMeter::Card::Z80,1500000);
    auto s=m.take(milliseconds(500));
    if(s.main6809.instructions!=1 || s.z80.instructions!=1)return 1;
    if(s.main6809.cycles!=250000 || s.z80.cycles!=1500000)return 2;
    if(std::abs(s.mhz(CpuSpeedMeter::Card::Main6809)-0.5)>1e-10)return 3;
    if(std::abs(s.mhz(CpuSpeedMeter::Card::Z80)-3.0)>1e-10)return 4;
    // 0.125 s on 6809 + 0.375 s on Z80 = 0.5 s of guest work.
    if(std::abs(s.virtualSeconds(2000000,4000000)-0.5)>1e-10)return 5;
    if(std::abs(s.progressRatio(2000000,4000000)-1.0)>1e-10)return 6;
    s=m.take(seconds(1));
    if(s.main6809.cycles || s.z80.cycles ||
       s.main6809.instructions || s.z80.instructions)return 7;
    m.record(CpuSpeedMeter::Card::Z80,4000000);
    m.reset(); // pause/resume must discard old counters
    s=m.take(seconds(1));
    if(s.z80.cycles || s.z80.instructions)return 8;
    m.record(CpuSpeedMeter::Card::Z80,4000000);
    s=m.take(seconds(2));
    if(std::abs(s.mhz(CpuSpeedMeter::Card::Z80)-2.0)>1e-10)return 9;
    if(std::abs(s.progressRatio(2000000,4000000)-0.5)>1e-10)return 10;
    std::cout<<"PASS: independent CPU T-state counters and wall-time rates\n";
    return 0;
}
