#pragma once
#include "portable_types.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>

// Translation of MainCpu.cpp Fm11PTMset() and Fm11PTM().
// SysClock/PtmFast/PtmClock are expressed in main CPU memory accesses,
// not in a synthetic fixed-frequency PTM tick.
class Ptm6840 {
public:
    struct Channel {
        BYTE control=0;
        WORD latch=0;
        std::uint32_t counter=0;
        bool running=false;
    };
    std::array<Channel,3> channels{};
    std::array<BYTE,3> latchHigh{};
    std::array<BYTE,3> counterLowLatch{};
    BYTE status=0;
    bool irqAcknowledged=false;

    void reset() {
        registers_.fill(0);
        ptmReg_.fill(0);
        ptmCtr_.fill(0);
        ptmClock_.fill(0);
        ptmInit_.fill(0);
        sysClock_=ptmFast_=0;
        cpuClock_=2000000;
        status=0;
        irqAcknowledged=false;
        irqLatch_=false;
        channels.fill(Channel{});
        latchHigh.fill(0);
        counterLowLatch.fill(0);
    }
    // FY($03) bit 0 is separate from FD($39) status bit 7.
    bool irqRequest() const {return irqLatch_;}
    void refreshIrq() {syncChannels();}

    // MainCpu::GetMemByte/SetMemByte increment SysClock; MainCpu::SimONE
    // tests PtmFast != 0 && PtmFast < SysClock at the instruction boundary.
    void advanceReferenceAccesses(std::uint64_t accesses,std::uint64_t cpuClock) {
        if(cpuClock)cpuClock_=cpuClock;
        sysClock_+=accesses;
    }
    // Original MainCpu::SimONE checks PTM immediately before dispatching
    // the IRQ and executing the next main CPU instruction.
    void checkInstructionBoundary() {
        if(ptmFast_!=0 &&
           (ptmFast_<0 || static_cast<std::uint64_t>(ptmFast_)<sysClock_))
            interval();
    }
    // Compatibility for the other board-timebase users. They do not use
    // the reference SysClock path but can still drive the counters.
    void advance(std::uint64_t ticks) {advanceReferenceAccesses(ticks,0);}

    BYTE read(BYTE port) {
        if(port==0x38)return 0;
        if(port==0x39) {
            interval(); // Fm11PTM(2) before sampling the status register
            const BYTE result=status;
            irqAcknowledged=true;
            irqLatch_=false; // FY03/FD03 acknowledgement
            return result;
        }
        if(port<0x3a || port>0x3f)return 0xff;
        unsigned no=(port-0x3a)/2;
        status=static_cast<BYTE>(status&~(1u<<no));
        if((status&3)==0)status&=0x7f;
        if((ptmReg_[no]&0x04)==0) {
            if((port&1)==0) {
                interval();
                const auto t=counterValue(no);
                registers_[port-0x38]=static_cast<BYTE>(t>>8);
                registers_[port+1-0x38]=static_cast<BYTE>(t);
            }
        } else {
            interval();
            no+=(port&1)*3;
            registers_[port-0x38]=static_cast<BYTE>(counterValue(no));
        }
        syncChannels();
        return registers_[port-0x38];
    }
    void write(BYTE port,BYTE value) {
        if(port==0x38 || port==0x39) {
            const unsigned no=port==0x39?1:((ptmReg_[2]&1)?0:2);
            ptmReg_[no]=(ptmReg_[no]&0x100)|value;
            if(no==0 && (value&1)) {
                ptmReg_[0]=1;ptmReg_[1]=ptmReg_[2]=0;
                ptmClock_.fill(0);
                ptmInit_.fill(0);
                // Reference reset clears the counters and FD39, not SysClock/PtmFast.
                status=0;
                // Source PTM reset leaves FY03 unchanged until a PTM
                // acknowledgement/interval or a later Fm11IRQ update.
            } else {
                status=static_cast<BYTE>((status&~(1u<<no))&0x7f);
                bool irq=false;
                for(unsigned n=0;n<3;++n)
                    irq|=(status&(1u<<n)) && (ptmReg_[n]&0x40);
                if(irq)status|=0x80;
                if((status&7)==0)irqLatch_=false;
                switch(ptmReg_[no]&0x38) {
                case 0x10: case 0x30:
                    start(no);
                    if(ptmReg_[no]&0x04)start(no+3);
                    break;
                default:break;
                }
            }
            syncChannels();
            return;
        }
        if(port<0x3a || port>0x3f)return;
        registers_[port-0x38]=value;
        const unsigned no=(port-0x3a)/2;
        if((ptmReg_[no]&0x04)==0) {
            if((port&1)==0) {
                latchHigh[no]=value;
                return;
            }
            std::uint32_t count=(std::uint32_t(registers_[port-1-0x38])<<8)|value;
            if(count==0)count=0x10000;
            if(no==2 && (ptmReg_[no]&1))count*=8;
            ptmCtr_[no]=static_cast<WORD>(count);
            ptmClock_[no]=ptmInit_[no]=periodFromCount(no,count);
            if((ptmReg_[no]&0x18)==0 || (ptmReg_[no]&0x100))
                start(no);
        } else {
            const unsigned slot=no+(port&1)*3;
            std::uint32_t count=value?value:0x100;
            if(no==2 && (ptmReg_[no]&1))count*=8;
            ptmCtr_[slot]=static_cast<WORD>(count);
            ptmClock_[slot]=ptmInit_[slot]=periodFromCount(slot,count);
            if((ptmReg_[no]&0x18)==0 || (ptmReg_[slot]&0x100))
                start(slot);
        }
        syncChannels();
    }
    std::uint64_t nextEventNanoseconds() const {
        if(!ptmFast_)return UINT64_MAX;
        const std::uint64_t remaining=(ptmFast_>0 &&
            static_cast<std::uint64_t>(ptmFast_)>=sysClock_)
            ?static_cast<std::uint64_t>(ptmFast_)-sysClock_+1:1;
        const auto clock=cpuClock_?cpuClock_:2000000ULL;
        const auto seconds=remaining/clock;
        const auto frac=remaining%clock;
        if(seconds>UINT64_MAX/1000000000ULL)return UINT64_MAX;
        return seconds*1000000000ULL+
            (frac*1000000000ULL+clock-1)/clock;
    }
private:
    static constexpr std::uint64_t ptmHz=9700;
    std::array<BYTE,8> registers_{};
    std::array<unsigned,6> ptmReg_{};
    std::array<std::uint32_t,6> ptmCtr_{};
    std::array<std::int64_t,6> ptmClock_{};
    std::array<std::int64_t,6> ptmInit_{};
    std::uint64_t sysClock_=0;
    std::int64_t ptmFast_=0;
    std::uint64_t cpuClock_=2000000;
    bool irqLatch_=false;
    std::int64_t periodFromCount(unsigned no,std::uint64_t count) const {
        const auto divisor=(ptmReg_[no]&0x02)?ptmHz*200:ptmHz;
        return static_cast<std::int64_t>(count*cpuClock_/divisor);
    }
    std::int64_t period(unsigned no) const {
        return periodFromCount(no,ptmCtr_[no]);
    }
    void start(unsigned no) {
        ptmReg_[no]|=0x100;
        if(ptmFast_==0) {
            ptmFast_=ptmClock_[no];
            sysClock_=0;
        } else {
            ptmClock_[no]+=static_cast<std::int64_t>(sysClock_);
            if(ptmFast_>ptmClock_[no])ptmFast_=ptmClock_[no];
        }
    }
    std::uint32_t counterValue(unsigned no) const {
        if(!ptmInit_[no])return 0;
        return static_cast<std::uint32_t>(
            static_cast<std::int64_t>(ptmCtr_[no])*ptmClock_[no]/ptmInit_[no]);
    }
    void interval() {
        ptmFast_=0;
        irqLatch_=false; // Fm11PTM(2) first clears FY($03) bit 0
        // Preserve the source's loop bounds and (no & 3) status routing.
        for(unsigned no=1;no<6;++no) {
            if((ptmReg_[no]&0x100)==0)continue;
            ptmClock_[no]-=static_cast<std::int64_t>(sysClock_);
            if(ptmClock_[no]<0) {
                status|=static_cast<BYTE>(1u<<(no&3));
                irqAcknowledged=false;
                if(ptmReg_[no&3]&0xc0) {
                    status|=0x80;
                    irqLatch_=true;
                }
                if((ptmReg_[no&3]&0x28)==0) {
                    ptmInit_[no]=period(no);
                    ptmClock_[no]+=ptmInit_[no];
                    if(ptmFast_==0 || ptmFast_>ptmClock_[no])
                        ptmFast_=ptmClock_[no];
                } else {
                    ptmReg_[no]&=0xff;
                    ptmClock_[no]=0;
                }
            } else if(ptmFast_==0 || ptmFast_>ptmClock_[no])
                ptmFast_=ptmClock_[no];
        }
        sysClock_=0;
        syncChannels();
    }
    void syncChannels() {
        for(unsigned n=0;n<3;++n) {
            channels[n].control=static_cast<BYTE>(ptmReg_[n]);
            channels[n].latch=static_cast<WORD>(ptmCtr_[n]);
            channels[n].counter=ptmClock_[n]>0?
                static_cast<std::uint32_t>(ptmClock_[n]):0;
            channels[n].running=(ptmReg_[n]&0x100)!=0 ||
                (ptmReg_[n+3]&0x100)!=0;
        }
    }
};
