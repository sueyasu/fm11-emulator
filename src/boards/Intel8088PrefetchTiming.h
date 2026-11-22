#pragma once
#include "portable_types.h"
#include <algorithm>
#include <cstdint>

// 8088 BIU *timing shadow*, not an instruction-byte cache.
//
// The 8088 has a four-byte instruction prefetch queue on its byte-wide
// bus. This tracker is deliberately passive: it never speculatively reads
// guest memory and never changes the clocks reported by the CPU. Future
// stages can use its counters when a BIU/EU bus scheduler is available.
// In particular, depth is an approximation, not a claim that the bus
// completed those reads in parallel with the execution unit.
class Intel8088PrefetchTiming {
public:
    struct Snapshot {
        unsigned depth=0;
        std::uint64_t instructionBytes=0;
        std::uint64_t demandBytes=0;
        std::uint64_t bufferedBytes=0;
        std::uint64_t refillBytes=0;
        std::uint64_t flushes=0;
        std::uint64_t memoryReadBytes=0;
        std::uint64_t memoryWriteBytes=0;
        std::uint64_t ioReadBytes=0;
        std::uint64_t ioWriteBytes=0;
        std::uint64_t operandBusCycles=0;
    };

    void reset() {
        state_={};
        validNext_=false;
        nextCs_=nextIp_=0;
        instructionDataBytes_=0;
    }
    void setEnabled(bool value) {
        enabled_=value;
        reset();
    }
    bool enabled() const { return enabled_; }
    Snapshot snapshot() const { return state_; }

    // Instrument only explicit CPU data transactions, never fetch8().
    // The 8088 bus is eight bits wide, so every observed byte transfer
    // reserves at least one four-clock bus cycle. External WAIT/HOLD
    // timing and independent DMA bus ownership are not yet observable.
    void beginInstruction() {
        instructionDataBytes_=0;
    }
    void dataRead(bool io=false) {
        if(!enabled_)return;
        ++instructionDataBytes_;
        if(io)++state_.ioReadBytes;
        else ++state_.memoryReadBytes;
    }
    void dataWrite(bool io=false) {
        if(!enabled_)return;
        ++instructionDataBytes_;
        if(io)++state_.ioWriteBytes;
        else ++state_.memoryWriteBytes;
    }

    // Called for every instruction byte, including prefix, ModR/M,
    // displacement and immediate bytes. The CPU's real fetch still reads
    // memory; this is only a record of how it would consume BIU slots.
    void consume(WORD cs, WORD ip) {
        if(!enabled_)return;
        if(validNext_ && (nextCs_!=cs || nextIp_!=ip))flush();
        validNext_=true;
        nextCs_=cs;
        nextIp_=WORD(ip+1);
        ++state_.instructionBytes;
        if(state_.depth) {
            --state_.depth;
            ++state_.bufferedBytes;
        } else ++state_.demandBytes;
    }
    void finish(WORD cs, WORD ip, std::uint64_t instructionCycles) {
        if(!enabled_)return;
        if(validNext_ && (cs!=nextCs_ || ip!=nextIp_))flush();
        // One observed operand byte consumes at least four bus clocks.
        // Subtract these bus reservations before granting *provisional*
        // prefetch slots. This is a per-instruction budget, not an
        // accurate bus phase scheduler or a true prefetch FIFO.
        const std::uint64_t occupied=std::uint64_t(instructionDataBytes_)*4;
        state_.operandBusCycles+=occupied;
        const std::uint64_t available=instructionCycles>occupied
            ? instructionCycles-occupied:0;
        const unsigned free=4-state_.depth;
        const unsigned grant=static_cast<unsigned>(
            std::min<std::uint64_t>(free,available/4));
        state_.depth+=grant;
        state_.refillBytes+=grant;
    }
    void flush() {
        if(!enabled_)return;
        state_.depth=0;
        validNext_=false;
        ++state_.flushes;
    }
private:
    bool enabled_=false;
    bool validNext_=false;
    WORD nextCs_=0;
    WORD nextIp_=0;
    unsigned instructionDataBytes_=0;
    Snapshot state_{};
};
