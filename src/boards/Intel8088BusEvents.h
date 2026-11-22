#pragma once
#include <cstdint>
#include <vector>

// Ordered CPU-visible 8088 bus events. These are micro-operation ordering
// markers, NOT timestamps or completed external T-state transactions.
// A byte-wide memory/port transaction is recorded once; instruction
// fetches remain distinct from data reads. Recording is opt-in and
// does not cause speculative device accesses.
class Intel8088BusEvents {
public:
    enum class Kind { Fetch, MemoryRead, MemoryWrite, PortRead, PortWrite };
    struct Event {
        Kind kind;
        std::uint32_t address;
        std::uint8_t value;
        // CPU program order within the instruction, starting from zero.
        unsigned order;
    };
    void setEnabled(bool enabled) {
        enabled_=enabled;
        reset();
    }
    bool enabled() const {return enabled_;}
    void reset() {events_.clear();}
    void beginInstruction() {events_.clear();}
    void record(Kind kind,std::uint32_t address,std::uint8_t value) {
        if(!enabled_)return;
        events_.push_back(Event{kind,address,value,
                                static_cast<unsigned>(events_.size())});
    }
    const std::vector<Event>& events() const {return events_;}
private:
    bool enabled_=false;
    std::vector<Event> events_{};
};
