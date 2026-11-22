#pragma once
#include <algorithm>
#include <cstdint>

// 8088 byte-bus T-state accounting for a *diagnostic* BIU scheduler.
//
// One byte transfer consumes T1..T4. Execution time supplied here
// belongs to the EU; operand bus cycles are reserved first (ordering
// is a deliberate approximation until micro-operation scheduling is
// available). Consecutive idle windows may complete a partial
// prefetch bus cycle across instruction boundaries. An intervening
// operand transfer cancels this unfinished prefetch phase.
//
// No CPU or device event is delayed by this class. External WAIT,
// HOLD, refresh and instruction-level demand stalls are not modeled.
class Intel8088BusTStates {
public:
    struct Snapshot {
        std::uint64_t elapsed=0;
        std::uint64_t operand=0;
        std::uint64_t prefetch=0;
        std::uint64_t idle=0;
        std::uint64_t interrupted=0;
        unsigned partial=0;
    };

    void reset() {state_={};}

    // A branch discards queued instruction bytes and an incomplete
    // speculative bus transaction, but does not rewind elapsed time.
    void flush() {
        state_.interrupted+=state_.partial;
        state_.partial=0;
    }

    // The operand transfers consume the first part of the supplied
    // instruction budget. This is a reservation, not an additional cost
    // to be added to Intel's instruction execution total.
    unsigned advance(std::uint64_t baseClocks,
                     unsigned operandBytes,unsigned freeSlots) {
        const std::uint64_t occupied=std::uint64_t(operandBytes)*4u;
        state_.elapsed+=baseClocks;
        state_.operand+=std::min(occupied,baseClocks);
        if(operandBytes && state_.partial) {
            state_.interrupted+=state_.partial;
            state_.partial=0;
        }
        std::uint64_t idle=baseClocks>occupied?baseClocks-occupied:0;
        if(freeSlots==0) {
            state_.idle+=idle;
            state_.partial=0;
            return 0;
        }
        const std::uint64_t available=idle+state_.partial;
        const unsigned grant=static_cast<unsigned>(
            std::min<std::uint64_t>(freeSlots,available/4u));
        state_.prefetch+=std::uint64_t(grant)*4u;
        const std::uint64_t spent=std::uint64_t(grant)*4u;
        state_.partial=grant==freeSlots?0u:
            static_cast<unsigned>(available-spent);
        // Cycles during which the prefetch FIFO could not accept a
        // further transaction are idle, rather than partial T states.
        if(grant==freeSlots)state_.idle+=available-spent;
        return grant;
    }

    Snapshot snapshot() const {return state_;}
private:
    Snapshot state_{};
};
