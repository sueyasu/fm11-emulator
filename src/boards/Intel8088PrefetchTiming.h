#pragma once
#include "portable_types.h"
#include "Intel8088BusTStates.h"
#include <algorithm>
#include <array>
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
        // Optional experimental demand-fetch penalty (four T states per
        // missed byte); never included in passive/FIFO-only mode.
        std::uint64_t demandStallCycles=0;
    };

    void reset() {
        state_={};
        validNext_=false;
        nextCs_=nextIp_=0;
        instructionDataBytes_=0;
        instructionDemandBytes_=0;
        head_=0;
        queue_.fill(0);
        busTStates_.reset();
    }
    void setEnabled(bool value) {
        enabled_=value;
        byteQueueEnabled_=false;
        demandStallEnabled_=false;
        busPhaseEnabled_=false;
        reset();
    }
    bool enabled() const { return enabled_; }
    // An explicit opt-in separate from the historical shadow counters.
    // This mode retains actual prefetched instruction bytes.
    void setByteQueueEnabled(bool value) {
        byteQueueEnabled_=value;
        enabled_=value;
        demandStallEnabled_=false;
        reset();
    }
    // Experimental opt-in: a missed byte in the FIFO is one 8088
    // four-clock demand bus cycle. This is a deliberately conservative
    // additive approximation, NOT bus-phase-accurate EU/BIU overlap.
    void setDemandStallTimingEnabled(bool value) {
        enabled_=value;
        byteQueueEnabled_=value;
        demandStallEnabled_=value;
        busPhaseEnabled_=false;
        reset();
    }
    // Separate opt-in: use a four-T-state byte-bus phase accumulator
    // instead of the old per-instruction floor(cycles/4) refill budget.
    // Instruction execution clocks and semantics are unchanged.
    void setBusPhaseTimingEnabled(bool value) {
        enabled_=value;
        byteQueueEnabled_=value;
        demandStallEnabled_=false;
        busPhaseEnabled_=value;
        reset();
    }
    bool busPhaseTimingEnabled() const {return enabled_ && busPhaseEnabled_;}
    Intel8088BusTStates::Snapshot busPhaseSnapshot() const {
        return busTStates_.snapshot();
    }
    bool demandStallTimingEnabled() const {
        return enabled_ && byteQueueEnabled_ && demandStallEnabled_;
    }
    bool byteQueueEnabled() const { return enabled_ && byteQueueEnabled_; }
    Snapshot snapshot() const { return state_; }

    // Instrument only explicit CPU data transactions, never fetch8().
    // The 8088 bus is eight bits wide, so every observed byte transfer
    // reserves at least one four-clock bus cycle. External WAIT/HOLD
    // timing and independent DMA bus ownership are not yet observable.
    void beginInstruction() {
        instructionDataBytes_=0;
        instructionDemandBytes_=0;
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

    // Called for every instruction byte in passive shadow mode.
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
    template<class ReadByte>
    BYTE fetch(WORD cs, WORD ip, ReadByte&& read) {
        if(!byteQueueEnabled()) {
            consume(cs,ip);
            return read(cs,ip);
        }
        if(validNext_ && (nextCs_!=cs || nextIp_!=ip))flush();
        validNext_=true;
        nextCs_=cs;
        nextIp_=WORD(ip+1);
        ++state_.instructionBytes;
        if(state_.depth) {
            const BYTE value=queue_[head_];
            head_=(head_+1u)%4u;
            --state_.depth;
            ++state_.bufferedBytes;
            return value; // Do not reread modified guest memory.
        }
        ++state_.demandBytes;
        if(demandStallEnabled_)++instructionDemandBytes_;
        return read(cs,ip);
    }
    template<class ReadByte>
    void finish(WORD cs, WORD ip, std::uint64_t instructionCycles,
                ReadByte&& read) {
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
        const unsigned grant=busPhaseEnabled_
            ? busTStates_.advance(instructionCycles,instructionDataBytes_,free)
            : static_cast<unsigned>(
                std::min<std::uint64_t>(free,available/4));
        if(byteQueueEnabled_) {
            // A control transfer leaves the old queue invalid, and
            // prefetch restarts at the actual target CS:IP. No speculative
            // memory reads occur in passive-shadow mode.
            if(!validNext_) {
                validNext_=true;
                nextCs_=cs;
                nextIp_=ip;
            }
            for(unsigned i=0;i<grant;++i) {
                const WORD offset=WORD(nextIp_+state_.depth);
                queue_[(head_+state_.depth)%4u]=read(nextCs_,offset);
                ++state_.depth;
            }
        } else state_.depth+=grant;
        state_.refillBytes+=grant;
    }
    // Called *after* finish() by the CPU timing finalizer. Refill
    // grants are derived from the reference instruction clocks, never
    // from these additional demand-stall clocks, so no second refill
    // can be charged for the same demand fetch.
    unsigned consumeDemandStallCycles() {
        if(!demandStallTimingEnabled())return 0;
        const unsigned cycles=instructionDemandBytes_*4u;
        state_.demandStallCycles+=cycles;
        instructionDemandBytes_=0;
        return cycles;
    }
    void flush() {
        if(!enabled_)return;
        state_.depth=0;
        head_=0;
        validNext_=false;
        if(busPhaseEnabled_)busTStates_.flush();
        ++state_.flushes;
    }
private:
    bool enabled_=false;
    bool byteQueueEnabled_=false;
    bool demandStallEnabled_=false;
    bool busPhaseEnabled_=false;
    Intel8088BusTStates busTStates_{};
    bool validNext_=false;
    std::array<BYTE,4> queue_{};
    unsigned head_=0;
    WORD nextCs_=0;
    WORD nextIp_=0;
    unsigned instructionDataBytes_=0;
    unsigned instructionDemandBytes_=0;
    Snapshot state_{};
};
