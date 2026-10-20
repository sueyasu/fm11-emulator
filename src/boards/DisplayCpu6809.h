#pragma once
#include "DisplaySubsystem.h"
#include "Sim09.h"
#include <cstdint>
#include <algorithm>

// The display CPU is a separate MC6809 with its own address space, not
// another main CPU card. All memory cycles reach the display subsystem.
class DisplayCpu6809 final : public CSim09 {
public:
    explicit DisplayCpu6809(DisplaySubsystem& display):display_(display) {
        Set6309Enabled(false);
    }
    BYTE GetMemByte(WORD address) override {
        ++nmiCycles_;
        ++totalAccesses_;
        return display_.readMemory(address);
    }
    void SetMemByte(WORD address,BYTE value) override {
        ++nmiCycles_;
        ++totalAccesses_;
        display_.writeMemory(address,value);
    }
    void resetCpu() {
        SimRESET();
        nmiCycles_=0;
        nmiThreshold_=60000;
        totalAccesses_=0;
        elapsedTimerMs_=0;
        display_.resetInterruptClock();
    }
    // Original SubCpu::GetMemByte/SetMemByte increment CpuClock and TotClock.
    // Recalibrate its 20 ms NMI period against measured host time every ~2 s.
    void timerTick(std::uint64_t elapsedMs) {
        display_.signalVsyncEvent();
        elapsedTimerMs_+=elapsedMs;
        if(elapsedTimerMs_>=2000 || totalAccesses_>1000000) {
            if(elapsedTimerMs_ && totalAccesses_)
                nmiThreshold_=std::max<std::uint64_t>(1,20*totalAccesses_/elapsedTimerMs_);
            totalAccesses_=0;
            elapsedTimerMs_=0;
        }
    }
    bool executeOne() {
        // Reference SubCpu::SimONE: VSync/GetKeyCode, HALT, IRQ,
        // FIRQ, NMI, Fm11Ram[105] wait gate, then one instruction.
        if(display_.consumeVsyncEvent())display_.processKeyCode();
        display_.tickSubInstruction();
        regs.HD=0;
        if(display_.haltRequested()) {
            regs.HD|=010;
            GetMemByte(regs.r.w.PC); // original HALT dummy read
            return false;
        }
        if(display_.subIrqPending()) {
            regs.HD|=001;
            SimIRQ();
        }
        if(display_.subFirqPending()) {
            regs.HD|=002;
            SimFIRQ();
        }
        if(!display_.nmiMask() && nmiCycles_>=nmiThreshold_) {
            regs.HD|=004;
            SimNMI();
            nmiCycles_=0;
        }
        if(display_.subWaitForInterrupt()) {
            if((regs.HD&007)==0) {
                regs.HD|=010;
                GetMemByte(regs.r.w.PC); // original wait dummy read
                return false;
            }
            display_.clearSubWaitForInterrupt();
        }
        if(waitState()!=WaitState::Running)return false;
        display_.beginCpuInstruction(regs.r.w.PC);
        try {
            SimONE();
        } catch(...) {
            display_.endCpuInstruction();
            throw;
        }
        display_.endCpuInstruction();
        return true;
    }
    void advanceIdleCycles(std::uint64_t) {}
    std::uint16_t pc() const {return regs.r.w.PC;}
private:
    std::uint64_t nmiCycles_=0;
    std::uint64_t nmiThreshold_=60000;
    std::uint64_t totalAccesses_=0;
    std::uint64_t elapsedTimerMs_=0;
    DisplaySubsystem& display_;
};
