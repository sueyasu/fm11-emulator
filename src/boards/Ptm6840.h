#pragma once
#include "portable_types.h"
#include <array>
#include <cstdint>

// MC6840 state belongs to the main board, not the optional main CPU card.
// The 6809 adapter retains register decoding and clock stepping for now.
class Ptm6840 {
public:
    struct Channel {
        BYTE control=0;
        WORD latch=0;
        std::uint32_t counter=0;
        bool running=false;
    };
    void reset() {
        channels.fill(Channel{});
        status=0;
        irqAcknowledged=false;
    }
    std::array<Channel,3> channels{};
    BYTE status=0;
    bool irqAcknowledged=false;
};
