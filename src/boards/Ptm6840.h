#pragma once
#include "portable_types.h"
#include <array>
#include <cstdint>
#include <limits>
#include <algorithm>

// Port of CMainCpu::Fm11PTM(mode 0/1/2) and Fm11PTMset.
// The scheduler supplies PTMCLOCK/2 ticks (9700 Hz). The reference
// maintains six internal slots: three channels and three 8-bit halves.
// All counter state and register side effects are kept together here.
class Ptm6840 {
public:
    struct Channel {
        BYTE control=0;
        WORD latch=0;
        std::uint32_t counter=0;
        bool running=false;
    };
    void reset() {
        reg_.fill(0);
        mode_.fill(0);
        count_.fill(0);
        init_.fill(0);
        clock_.fill(0);
        enabled_.fill(false);
        channels.fill(Channel{});
        latchHigh.fill(0);
        counterLowLatch.fill(0);
        status=0;
        irqAcknowledged=false;
        referenceClockHz_=2000000;
    }
    bool irqRequest() const {
        return (status & 0x80) != 0 && !irqAcknowledged;
    }
    void refreshIrq() {
        // Original Fm11PTM computes bit 7 from outstanding enabled sources.
        const BYTE pending=status & 7;
        bool asserted=false;
        for(unsigned no=0;no<3;++no)
            if((pending&(1u<<no)) && (mode_[no]&0x40))
                asserted=true;
        if(asserted)status|=0x80;
        else status&=0x7f;
    }
    // SysClock is counted in main-CPU memory accesses, like Fm11PTM.
    void advanceReferenceAccesses(std::uint64_t accesses,std::uint64_t clockHz) {
        referenceClockHz_=clockHz;
        advance(accesses);
    }
    void advance(std::uint64_t ticks) {
        if(ticks==0)return;
        // Fm11PTM interval mode: active slots are decremented by SysClock,
        // expire on strictly negative remaining time, and are then
        // reloaded only for continuous mode (control & 0x28 == 0).
        for(unsigned slot=0;slot<6;++slot) {
            if(!enabled_[slot])continue;
            const auto t=static_cast<std::int64_t>(
                std::min<std::uint64_t>(ticks,
                    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())));
            clock_[slot]-=t;
            if(clock_[slot]<0) {
                const unsigned channel=slot%3;
                status|=static_cast<BYTE>(1u<<channel);
                irqAcknowledged=false;
                if((mode_[channel]&0xc0)!=0)status|=0x80;
                if((mode_[channel]&0x28)==0) {
                    const auto period=periodFor(slot);
                    init_[slot]=period;
                    if(period>0) {
                        // Multiple elapsed periods are accounted for by
                        // keeping the negative residual from this interval.
                        const auto deficit=-clock_[slot]-1;
                        clock_[slot]=period-1-(deficit%period);
                    } else {
                        clock_[slot]=0;
                        enabled_[slot]=false;
                    }
                } else {
                    enabled_[slot]=false;
                    clock_[slot]=0;
                }
            }
        }
        syncChannels();
        refreshIrq();
    }
    BYTE read(BYTE port) {
        switch(port) {
        case 0x38:return 0;
        case 0x39: {
            // FD39 acknowledges the main IRQ latch without clearing
            // the three per-channel timeout flags.
            const BYTE result=status;
            irqAcknowledged=true;
            return result;
        }
        default:break;
        }
        if(port<0x3a || port>0x3f)return 0xff;
        unsigned no=(port-0x3a)/2;
        status&=static_cast<BYTE>(~(1u<<no));
        // Preserve the reference's unusual (status & 3) test:
        // channel 3 alone does not keep bit 7 set on counter read.
        if((status&3)==0)status&=0x7f;
        if((mode_[no]&0x04)==0) {
            if((port&1)==0) {
                const std::uint32_t value=current(no);
                reg_[port-0x38]=static_cast<BYTE>(value>>8);
                reg_[port+1-0x38]=static_cast<BYTE>(value);
            }
        } else {
            no+=(port&1)*3;
            reg_[port-0x38]=static_cast<BYTE>(current(no));
        }
        return reg_[port-0x38];
    }
    void write(BYTE port,BYTE value) {
        if(port==0x38 || port==0x39) {
            const unsigned no=port==0x39?1:((mode_[2]&1)?0:2);
            mode_[no]=value;
            channels[no].control=value;
            if(no==0 && (value&1)) {
                // Reference channel-1 reset does not discard the entire
                // emulated machine, only the PTM registers/counters/status.
                reg_.fill(0);
                mode_.fill(0);
                count_.fill(0);
                init_.fill(0);
                clock_.fill(0);
                enabled_.fill(false);
                mode_[0]=1;
                channels.fill(Channel{});
                channels[0].control=1;
                status=0;
            } else {
                status&=static_cast<BYTE>(~(1u<<no));
                status&=0x7f;
                refreshIrq();
                switch(value&0x38) {
                case 0x10: case 0x30:
                    enable(no);
                    if(value&0x04)enable(no+3);
                    break;
                default:break; // latch and comparison modes do not start
                }
            }
            syncChannels();
            return;
        }
        if(port<0x3a || port>0x3f)return;
        reg_[port-0x38]=value;
        const unsigned no=(port-0x3a)/2;
        const bool dual=(mode_[no]&0x04)!=0;
        if(!dual && !(port&1)) {
            latchHigh[no]=value;
            return;
        }
        const unsigned slot=no+(dual ? ((port&1)*3):0);
        std::uint32_t count=dual?value:
            ((std::uint32_t(reg_[port-1-0x38])<<8)|value);
        if(count==0)count=dual?0x100u:0x10000u;
        if(no==2 && (mode_[no]&1))count*=8;
        count_[slot]=count;
        // One scheduler tick is 1/(PTMCLOCK/2); the alternative
        // PTMCLOCK*100 source advances 200 times as quickly.
        init_[slot]=periodFor(slot);
        clock_[slot]=init_[slot];
        if((mode_[no]&0x18)==0 || enabled_[slot])enable(slot);
        channels[no].latch=static_cast<WORD>(count);
        syncChannels();
    }
    std::uint64_t nextEventNanoseconds() const {
        std::uint64_t nearest=UINT64_MAX;
        for(unsigned i=0;i<6;++i)
            if(enabled_[i]) {
                const auto ticks=clock_[i]<0 ? 1ULL:
                    static_cast<std::uint64_t>(clock_[i])+1ULL;
                nearest=std::min<std::uint64_t>(nearest,ticks);
            }
        if(nearest==UINT64_MAX)return nearest;
        const auto hz=referenceClockHz_?referenceClockHz_:2000000ULL;
        const auto seconds=nearest/hz;
        const auto remainder=nearest%hz;
        if(seconds>UINT64_MAX/1000000000ULL)return UINT64_MAX;
        return seconds*1000000000ULL+
            (remainder*1000000000ULL+hz-1)/hz;
    }
    std::array<Channel,3> channels{};
    std::array<BYTE,3> latchHigh{};
    std::array<BYTE,3> counterLowLatch{};
    BYTE status=0;
    bool irqAcknowledged=false;
private:
    std::int64_t periodFor(unsigned slot) const {
        // Fm11PTM: (PtmCtr * CpuClock) / selected PTM input Hz.
        constexpr std::uint64_t ptmHz=9700;
        const std::uint64_t divisor=(mode_[slot%3]&0x02)
            ?ptmHz*200:ptmHz;
        const auto count=count_[slot];
        const auto clock=referenceClockHz_;
        if(!clock || !count)return 0;
        // Reorder arithmetic into quotient/remainder to avoid __int128
        // and prevent multiplication overflow for normal CPU frequencies.
        const auto whole=clock/divisor;
        const auto rem=clock%divisor;
        const auto value=static_cast<std::uint64_t>(count)*whole+
            (static_cast<std::uint64_t>(count)*rem)/divisor;
        return static_cast<std::int64_t>(std::min<std::uint64_t>(
            value,static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())));
    }
    void enable(unsigned slot) {
        if(init_[slot]>0)enabled_[slot]=true;
    }
    std::uint32_t current(unsigned slot) const {
        if(init_[slot]<=0 || clock_[slot]<=0)return 0;
        return static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(count_[slot])*
            static_cast<std::uint64_t>(clock_[slot])/
            static_cast<std::uint64_t>(init_[slot]));
    }
    void syncChannels() {
        for(unsigned n=0;n<3;++n) {
            channels[n].running=enabled_[n]||enabled_[n+3];
            channels[n].counter=clock_[n]>0?
                static_cast<std::uint32_t>(clock_[n]):0;
            channels[n].control=static_cast<BYTE>(mode_[n]);
        }
    }
    std::uint64_t referenceClockHz_=2000000;
    std::array<BYTE,8> reg_{};
    std::array<BYTE,3> mode_{};
    std::array<std::uint32_t,6> count_{};
    std::array<std::int64_t,6> init_{};
    std::array<std::int64_t,6> clock_{};
    std::array<bool,6> enabled_{};
};
