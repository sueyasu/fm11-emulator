#pragma once
#include "Intel8088BusEvents.h"
#include <cstdint>
#include <vector>

// Datasheet-supported MOV transfer validation, NOT microcode timestamps.
// Intel iAPX 86/88 User's Manual Table 2-21: MOV reg,mem 8+EA,
// MOV mem,reg 9+EA; each 8088 16-bit operand adds four clocks.
// Validate bus width, direction, order and the independently known
// minimum 4-T-state occupancy of each memory operand byte.
class Intel8088MovBusReference {
public:
    enum class Status { Valid, Unsupported, EventMismatch, ClockMismatch };
    struct Result {
        Status status=Status::Unsupported;
        unsigned expectedOperandBytes=0;
        unsigned expectedClocks=0;
        // A window, not a timestamp. Verified bus positions still
        // require cycle traces or a microcode-derived timing model.
        unsigned earliestTState=0;
        unsigned latestFirstTState=0;
    };

    static Result validate(std::uint8_t opcode,std::uint8_t modrm,
                           unsigned eaClocks,unsigned actualClocks,
                           const std::vector<Intel8088BusEvents::Event>& events,
                           unsigned prefixClocks=0) {
        Result r;
        if(opcode<0x88 || opcode>0x8b)return r;
        const bool memory=(modrm&0xc0)!=0xc0;
        const bool word=(opcode&1)!=0;
        const bool load=(opcode&2)!=0;
        const unsigned bytes=memory?(word?2u:1u):0u;
        r.expectedOperandBytes=bytes;
        r.expectedClocks=prefixClocks+(memory
            ? (load?8u:9u)+eaClocks+(word?4u:0u):2u);
        if(actualClocks!=r.expectedClocks) {
            r.status=Status::ClockMismatch;
            return r;
        }
        // Events begin with opcode and ModR/M fetch. Displacement
        // bytes, if any, must precede the data transaction(s).
        unsigned firstData=0;
        while(firstData<events.size() &&
              events[firstData].kind==Intel8088BusEvents::Kind::Fetch)
            ++firstData;
        if(firstData<2 || events.size()!=firstData+bytes ||
           events[0].value!=opcode ||
           events[1].value!=modrm) {
            r.status=Status::EventMismatch;
            return r;
        }
        for(unsigned i=0;i<bytes;++i) {
            const auto& e=events[firstData+i];
            const auto kind=load?Intel8088BusEvents::Kind::MemoryRead
                                :Intel8088BusEvents::Kind::MemoryWrite;
            if(e.kind!=kind ||
               e.order!=firstData+i ||
               (i && e.address!=((events[firstData].address+i)&0xfffff))) {
                r.status=Status::EventMismatch;
                return r;
            }
        }
        r.status=Status::Valid;
        r.earliestTState=0;
        r.latestFirstTState=r.expectedClocks-4*bytes;
        return r;
    }
};
