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
    // Limit *host* catch-up to 20 ms. At 10x speed, that is 200 ms
    // of virtual CPU time, so a 16.7 ms VSYNC wait is not truncated.
    static constexpr std::int64_t maxLagPs=20000000000LL; // 20 ms
    static constexpr unsigned maxSpeed=100;
    explicit SdlCpuPacer(unsigned speed=1):speed_(speed) {
        if(speed>maxSpeed)throw std::invalid_argument("speed must be 0..100");
    }
    unsigned speed() const {return speed_;}
    bool unlimited() const {return speed_==0;}
    std::int64_t maxGuestLagPs() const {
        return maxLagPs*static_cast<std::int64_t>(speed_);
    }
    void reset() {creditPs_=0;}
    void addElapsed(std::chrono::nanoseconds elapsed) {
        if(unlimited())return;
        const auto ns=std::clamp<std::int64_t>(
            elapsed.count(),0,maxLagPs/1000);
        const auto limit=maxGuestLagPs();
        creditPs_=std::min(limit,creditPs_+ns*1000*speed_);
    }
    bool ready() const {return unlimited() || creditPs_>0;}
    void consume(std::uint64_t cycles,std::uint64_t hz) {
        if(unlimited())return;
        if(!hz || hz>1000000000ULL)
            throw std::invalid_argument("invalid CPU clock for SDL pacing");
        constexpr std::uint64_t secondPs=1000000000000ULL;
        const auto limit=static_cast<std::uint64_t>(maxGuestLagPs());
        // Split to prevent overflow when hz approaches 1 GHz.
        const auto whole=cycles/hz,rem=cycles%hz;
        const auto wholePs=whole?limit:0ULL;
        const auto remPs=rem*(secondPs/hz)+(rem*(secondPs%hz))/hz;
        const auto used=std::min<std::uint64_t>(limit,wholePs+remPs);
        // Cap debt so a huge atomic block operation cannot stall SDL for
        // hundreds of milliseconds; full instruction time still reaches
        // the peripheral scheduler via Machine::executeOne().
        creditPs_=std::max(-maxGuestLagPs(),
                           creditPs_-static_cast<std::int64_t>(used));
    }
    std::int64_t creditPs() const {return creditPs_;}
private:
    unsigned speed_=1;
    std::int64_t creditPs_=0;
};
