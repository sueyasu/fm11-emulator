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
        latchHigh.fill(0);
        counterLowLatch.fill(0);
    }
    bool irqRequest() const {
        bool active=false;
        for(unsigned i=0;i<channels.size();++i)
            active |= (status&(1u<<i))!=0 && (channels[i].control&0x40)!=0;
        return active && !irqAcknowledged;
    }
    void refreshIrq() {
        bool active=false;
        for(unsigned i=0;i<channels.size();++i)
            active |= (status&(1u<<i))!=0 && (channels[i].control&0x40)!=0;
        if(active)status|=0x80;
        else status&=0x7f;
    }
    void advance(std::uint64_t ticks) {
        for(unsigned i=0;i<channels.size();++i) {
            auto& ch=channels[i];
            if(!ch.running || ticks==0)continue;
            if(ticks<ch.counter) {
                ch.counter-=static_cast<std::uint32_t>(ticks);
                continue;
            }
            const auto remaining=ticks-ch.counter;
            status|=static_cast<BYTE>(1u<<i);
            irqAcknowledged=false;
            const bool continuous=(ch.control&0x28)==0;
            if(continuous) {
                const std::uint64_t period=ch.latch==0?0x10000u:ch.latch;
                ch.counter=static_cast<std::uint32_t>(period-(remaining%period));
            } else {
                ch.counter=0;
                ch.running=false;
            }
        }
        refreshIrq();
    }
    BYTE read(BYTE port) {
        if(port==0x38)return 0;
        if(port==0x39) {
            const BYTE result=status;
            irqAcknowledged=true;
            refreshIrq();
            return result;
        }
        const unsigned index=(port-0x3a)/2;
        auto& ch=channels[index];
        status&=static_cast<BYTE>(~(1u<<index));
        refreshIrq();
        const WORD value=static_cast<WORD>(ch.counter);
        if((port&1)==0)counterLowLatch[index]=static_cast<BYTE>(value);
        return (port&1)?counterLowLatch[index]:static_cast<BYTE>(value>>8);
    }
    void write(BYTE port,BYTE value) {
        if(port==0x38 || port==0x39) {
            const unsigned index=port==0x39?1:((channels[1].control&1)?0:2);
            channels[index].control=value;
            if(index==0 && (value&1)) {
                reset();
                channels[0].control=1;
            } else {
                status&=static_cast<BYTE>(~(1u<<index));
                const BYTE mode=value&0x38;
                if(mode==0x10 || mode==0x30) {
                    auto& ch=channels[index];
                    ch.counter=ch.latch==0?0x10000u:ch.latch;
                    ch.running=true;
                }
            }
            refreshIrq();
            return;
        }
        const unsigned index=(port-0x3a)/2;
        if((port&1)==0)latchHigh[index]=value;
        else {
            auto& ch=channels[index];
            ch.latch=static_cast<WORD>((static_cast<WORD>(latchHigh[index])<<8)|value);
            ch.counter=ch.latch==0?0x10000u:ch.latch;
            ch.running=true;
        }
    }
    std::uint64_t nextEventNanoseconds() const {
        std::uint64_t soonest=UINT64_MAX;
        for(const auto& ch:channels)
            if(ch.running && ch.counter && ch.counter<soonest)soonest=ch.counter;
        if(soonest==UINT64_MAX)return soonest;
        return (soonest*1000000000ULL+9699)/9700;
    }
    std::array<Channel,3> channels{};
    std::array<BYTE,3> latchHigh{};
    std::array<BYTE,3> counterLowLatch{};
    BYTE status=0;
    bool irqAcknowledged=false;
};
