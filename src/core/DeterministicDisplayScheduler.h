#pragma once
#include "MainBoard.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>

// Deterministic sub-CPU scheduler for single-threaded machine tests and
// legacy headless tools only. Never called while the real-time sub-CPU
// worker is active. No CPU-cycle-to-display-clock conversion remains in
// the parallel SDL execution path.
class DeterministicDisplayScheduler {
public:
    explicit DeterministicDisplayScheduler(MainBoard& board):board_(board) {}
    void reset() {
        credit_=0;
        cycleFraction_=0;
        nanosecondFraction_=0;
        inputHz_=0;
    }
    void setClockHz(std::uint64_t hz) {
        if(clockHz_==hz)return;
        clockHz_=hz;
        reset();
    }
    std::uint64_t clockHz() const {return clockHz_;}
    void advanceTime(std::uint64_t ns)
    {
        if(!clockHz_)return;
        auto& sub=board_.displayCpu();
        if(board_.displaySubsystem().haltRequested()) {
            credit_=0;
            nanosecondFraction_=0;
            return;
        }
        constexpr std::uint64_t second=1000000000ULL;
        const auto seconds=ns/second;
        const auto remainder=ns%second;
        if(seconds>static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max())/clockHz_)
            throw std::overflow_error("display CPU time interval too large");
        const auto fractional=remainder*clockHz_+nanosecondFraction_;
        const auto cycles=seconds*clockHz_+fractional/second;
        if(cycles>static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()-credit_))
            throw std::overflow_error("display CPU cycle budget too large");
        nanosecondFraction_=fractional%second;
        credit_+=static_cast<std::int64_t>(cycles);
        while(credit_>0) {
            if(!sub.executeOne()) {
                credit_=0;
                break;
            }
            const auto used=sub.LastInstructionCycles();
            credit_-=static_cast<std::int64_t>(used?used:1);
        }
    }

    void advanceCpuCycles(std::uint64_t cycles,std::uint64_t cpuHz)
    {
        if(!clockHz_ || !cycles || !cpuHz)return;
        constexpr std::uint64_t second=1000000000ULL;
        // The remainder is represented as nanosecond fractions in units of
        // 1/cpuHz. On a CPU-card clock switch, preserve the elapsed fraction
        // instead of discarding it (which otherwise slows the display CPU).
        if(inputHz_!=cpuHz) {
            if(inputHz_!=0)
                cycleFraction_=(cycleFraction_*cpuHz)/inputHz_;
            inputHz_=cpuHz;
        }
        const auto whole=cycles/cpuHz;
        const auto rem=cycles%cpuHz;
        if(whole>std::numeric_limits<std::uint64_t>::max()/second)
            throw std::overflow_error("CPU time interval too large");
        const auto numerator=rem*second+cycleFraction_;
        const auto ns=whole*second+numerator/cpuHz;
        cycleFraction_=numerator%cpuHz;
        advanceTime(ns);
    }


private:
    MainBoard& board_;
    std::uint64_t clockHz_=0;
    std::int64_t credit_=0;
    std::uint64_t cycleFraction_=0,inputHz_=0,nanosecondFraction_=0;
};
