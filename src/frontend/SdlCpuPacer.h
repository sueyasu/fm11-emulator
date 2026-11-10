#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>

// SDL host-time pacing. Machine::executeOne() remains the sole owner of
// emulated PTM and display-sub-CPU time; this limits only host-side execution.
// Picosecond credit preserves exact 2 MHz / 4 MHz cycle lengths across FD05
// CPU switches, without rounding each short instruction to microseconds.
class SdlCpuPacer {
public:
    static constexpr std::int64_t maxLagPs=20000000000LL; // 20 ms
    void reset() {creditPs_=0;}
    void addElapsed(std::chrono::nanoseconds elapsed) {
        const auto ns=std::clamp<std::int64_t>(
            elapsed.count(),0,maxLagPs/1000);
        creditPs_=std::min(maxLagPs,creditPs_+ns*1000);
    }
    bool ready() const {return creditPs_>0;}
    void consume(std::uint64_t cycles,std::uint64_t hz) {
        if(!hz || hz>1000000000ULL)
            throw std::invalid_argument("invalid CPU clock for SDL pacing");
        constexpr std::uint64_t secondPs=1000000000000ULL;
        // Split to prevent overflow when hz approaches 1 GHz.
        const auto whole=cycles/hz,rem=cycles%hz;
        const auto wholePs=whole?static_cast<std::uint64_t>(maxLagPs):0ULL;
        const auto remPs=rem*(secondPs/hz)+(rem*(secondPs%hz))/hz;
        const auto used=std::min<std::uint64_t>(
            static_cast<std::uint64_t>(maxLagPs),wholePs+remPs);
        // Cap debt so a huge atomic block operation cannot stall SDL for
        // hundreds of milliseconds; full instruction time still reaches
        // the peripheral scheduler via Machine::executeOne().
        creditPs_=std::max(-maxLagPs,creditPs_-static_cast<std::int64_t>(used));
    }
    std::int64_t creditPs() const {return creditPs_;}
private:
    std::int64_t creditPs_=0;
};
