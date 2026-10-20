#pragma once
#include "portable_types.h"
#include <array>
#include <cstdint>
#include <limits>

// MainCpu::Fm11PTM port. The board scheduler supplies PTMCLOCK/2
// (= 9,700 Hz) units. The reference stores six independent counters:
// three 16-bit counters or six halves for dual-8-bit operation.
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
        controls_.fill(0);
        counts_.fill(0);
        remaining_.fill(0);
        periods_.fill(0);
        active_.fill(false);
        registers_.fill(0);
        latchHigh.fill(0);
        counterLowLatch.fill(0);
        status=0;
        irqAcknowledged=false;
    }
    bool irqRequest() const {
        return ((status&0x80)!=0) && !irqAcknowledged;
    }
    void refreshIrq() {
        BYTE pending=static_cast<BYTE>(status&0x07);
        bool irq=false;
        for(unsigned i=0;i<3;++i)
            if((pending&(1u<<i)) && (controls_[i]&0x40))irq=true;
        if(irq)status|=0x80;
        else status&=0x7f;
    }
    // One tick is 1/9700 second, per PTMCLOCK/2 in MainCpu.cpp.
    void advance(std::uint64_t ticks) {
        if(ticks==0)return;
        // Keep the separate halves of a dual-eight-bit channel.
        for(unsigned slot=0;slot<6;++slot) {
            if(!active_[slot])continue;
            const unsigned channel=slot%3;
            std::uint64_t left=ticks;
            if(remaining_[slot]>static_cast<std::int64_t>(left)) {
                remaining_[slot]-=static_cast<std::int64_t>(left);
                continue;
            }
            // The original interval routine expires when PtmClock < 0,
            // rather than when it becomes exactly zero.
            if(remaining_[slot]==static_cast<std::int64_t>(left)) {
                remaining_[slot]=0;
                continue;
            }
            left-=static_cast<std::uint64_t>(remaining_[slot]+1);
            status|=static_cast<BYTE>(1u<<channel);
            irqAcknowledged=false;
            const bool continuous=(controls_[channel]&0x28)==0;
            if(continuous && periods_[slot]>0) {
                const auto period=static_cast<std::uint64_t>(periods_[slot]);
                remaining_[slot]=static_cast<std::int64_t>(
                    period-1-(left%period));
            } else {
                active_[slot]=false;
                remaining_[slot]=0;
            }
            updatePublicChannel(channel);
        }
        refreshIrq();
    }
    BYTE read(BYTE port) {
        if(port==0x38)return 0;
        if(port==0x39) {
            refreshIrq();
            const BYTE result=status;
            // Reference FD39 read acknowledges FY03/FD03 but does not
            // clear the individual three pending status bits.
            irqAcknowledged=true;
            return result;
        }
        if(port<0x3a || port>0x3f)return 0xff;
        const unsigned channel=(port-0x3a)/2;
        status&=static_cast<BYTE>(~(1u<<channel));
        refreshIrq();
        const unsigned slot=(controls_[channel]&0x04)
            ?channel+((port&1)?3:0):channel;
        const std::uint64_t initial=periods_[slot];
        const std::uint64_t count=counts_[slot];
        const std::uint64_t remaining=remaining_[slot]>0
            ?static_cast<std::uint64_t>(remaining_[slot]):0;
        const std::uint64_t current=initial?count*remaining/initial:0;
        if((controls_[channel]&0x04)!=0)
            return static_cast<BYTE>(current);
        if((port&1)==0) {
            registers_[port-0x38]=static_cast<BYTE>(current>>8);
            registers_[port+1-0x38]=static_cast<BYTE>(current);
        }
        return registers_[port-0x38];
    }
    void write(BYTE port,BYTE value) {
        if(port==0x38 || port==0x39) {
            // Reference: the selection bit belongs to channel 3, not 2.
            const unsigned channel=port==0x39?1:((controls_[2]&1)?0:2);
            controls_[channel]=value;
            channels[channel].control=value;
            if(channel==0 && (value&1)) {
                reset();
                controls_[0]=1;
                channels[0].control=1;
                return;
            }
            status&=static_cast<BYTE>(~(1u<<channel));
            refreshIrq();
            const BYTE mode=value&0x38;
            if(mode==0x10 || mode==0x30) {
                start(channel);
                if(value&0x04)start(channel+3);
            }
            return;
        }
        if(port<0x3a || port>0x3f)return;
        registers_[port-0x38]=value;
        const unsigned channel=(port-0x3a)/2;
        const bool dual=(controls_[channel]&0x04)!=0;
        if(!dual && !(port&1)) {
            latchHigh[channel]=value;
            return;
        }
        const unsigned slot=channel+(dual && (port&1)?3:0);
        std::uint64_t count=dual?value:
            (std::uint64_t(registers_[port-1-0x38])<<8)|value;
        if(count==0)count=dual?0x100:0x10000;
        if(channel==2 && (controls_[2]&1))count*=8;
        counts_[slot]=count;
        channels[channel].latch=static_cast<WORD>(count);
        // MainCpu::Fm11PTM: PTMCLOCK/2 or PTMCLOCK*100.
        // PTMCLOCK/2 input is the unit supplied by the scheduler.
        const auto divisor=(controls_[slot%3]&0x02)?200u:1u;
        periods_[slot]=std::max<std::uint64_t>(1,(count+divisor-1)/divisor);
        remaining_[slot]=static_cast<std::int64_t>(periods_[slot]);
        const bool shouldStart=dual
            ?((controls_[channel]&0x18)==0 || active_[slot])
            :((controls_[channel]&0x18)==0 || active_[slot]);
        if(shouldStart)start(slot);
        updatePublicChannel(channel);
    }
    std::uint64_t nextEventNanoseconds() const {
        std::uint64_t soonest=UINT64_MAX;
        for(unsigned slot=0;slot<6;++slot)
            if(active_[slot]) {
                const auto ticks=remaining_[slot]<0?1u:
                    static_cast<std::uint64_t>(remaining_[slot])+1;
                if(ticks<soonest)soonest=ticks;
            }
        if(soonest==UINT64_MAX)return soonest;
        return (soonest*1000000000ULL+9699)/9700;
    }
    std::array<Channel,3> channels{};
    std::array<BYTE,3> latchHigh{};
    std::array<BYTE,3> counterLowLatch{};
    BYTE status=0;
    bool irqAcknowledged=false;
private:
    void start(unsigned slot) {
        if(!periods_[slot])return;
        active_[slot]=true;
        if(remaining_[slot]<=0)
            remaining_[slot]=static_cast<std::int64_t>(periods_[slot]);
        updatePublicChannel(slot%3);
    }
    void updatePublicChannel(unsigned index) {
        auto& ch=channels[index];
        ch.running=active_[index]||active_[index+3];
        ch.counter=static_cast<std::uint32_t>(
            remaining_[index]>0?remaining_[index]:0);
    }
    std::array<WORD,6> controls_{};
    std::array<std::uint64_t,6> counts_{};
    std::array<std::int64_t,6> remaining_{};
    std::array<std::uint64_t,6> periods_{};
    std::array<bool,6> active_{};
    std::array<BYTE,8> registers_{};
};
