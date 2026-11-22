#pragma once
#include "Intel8088BusEvents.h"
#include "Intel8088BusTStates.h"
#include <cstdint>
#include <vector>

// Diagnostic scheduling of *known* CPU operand bus transaction positions.
// Offsets are relative to the start of one executed instruction, in T
// states. They must be supplied from independently verified timing data.
// Fetch events are EU byte consumption and do not reserve a bus cycle
// here: bytes may already be in the 8088 prefetch queue.
//
// Each operand byte reserves a complete T1..T4 interval. Events may not
// overlap, exceed the instruction's reference cycles, or run backwards.
// An invalid/unknown timing trace is not assigned fabricated timestamps.
// No hardware is accessed and no CPU clocks are changed.
class Intel8088BusEventSchedule {
public:
    enum class Status { Unspecified, Valid, CountMismatch, OutOfRange, Overlap };
    struct Result {
        Status status=Status::Unspecified;
        unsigned operandEvents=0;
        unsigned prefetchGrants=0;
        Intel8088BusTStates::Snapshot bus{};
    };

    void reset() {
        bus_.reset();
        result_={};
    }
    Result lastResult() const {return result_;}

    Result evaluate(const std::vector<Intel8088BusEvents::Event>& events,
                    std::uint64_t instructionClocks,
                    const std::vector<unsigned>& operandStarts,
                    unsigned freeSlots=4) {
        unsigned count=0;
        for(const auto& event:events) {
            if(event.kind!=Intel8088BusEvents::Kind::Fetch)++count;
        }
        Result next{};
        next.operandEvents=count;
        if(operandStarts.size()!=count) {
            next.status=Status::CountMismatch;
            return result_=next;
        }
        std::uint64_t cursor=0;
        for(unsigned start:operandStarts) {
            if(std::uint64_t(start)+4u>instructionClocks) {
                next.status=Status::OutOfRange;
                return result_=next;
            }
            if(start<cursor) {
                next.status=Status::Overlap;
                return result_=next;
            }
            cursor=std::uint64_t(start)+4;
        }

        // All checks are complete before mutating the scheduler.
        cursor=0;
        for(unsigned start:operandStarts) {
            const unsigned grant=bus_.advance(start-cursor,0,freeSlots);
            next.prefetchGrants+=grant;
            freeSlots-=grant;
            cursor=std::uint64_t(start);
            // The byte bus is occupied for T1-T4 at this position.
            bus_.advance(4,1,freeSlots);
            cursor+=4;
        }
        next.prefetchGrants+=bus_.advance(instructionClocks-cursor,0,freeSlots);
        next.status=Status::Valid;
        next.bus=bus_.snapshot();
        return result_=next;
    }

    void flush() {bus_.flush();}
private:
    Intel8088BusTStates bus_{};
    Result result_{};
};
