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
        // Windows SubCpu::SimONE handles pending timer/VSync first,
        // then GetKeyCode, independently of the periodic CPU NMI.
        if(display_.consumeVsyncEvent())display_.processKeyCode();
        if(display_.haltRequested()) {
            // Original SubCpu::SimONE performs a dummy memory access
            // during SUB HALT, advancing CpuClock and TotClock.
            GetMemByte(regs.r.w.PC);
            return false;
        }
        // Reference SubCpu::SimONE: interrupt priority IRQ, FIRQ, NMI.
        const bool interruptRequest=display_.subIrqPending() ||
            display_.subFirqPending() ||
            (!display_.nmiMask() && nmiCycles_>=nmiThreshold_);
        // The original clock threshold is 60000 at reset, recalibrated
        // dynamically by the Windows event loop. Count memory accesses here.
        if(display_.subIrqPending())SimIRQ();
        if(display_.subFirqPending())SimFIRQ();
        if(!display_.nmiMask() && nmiCycles_>=nmiThreshold_) {
            // Original timed keyboard/vsync event calls GetKeyCode before
            // dispatching the periodic NMI. Without this, the initial
            // AFE0-generated key and queued host input remain dormant
            // until firmware happens to read AFED.
            SimNMI();
            nmiCycles_=0;
        }
        // Original Fm11Ram[0x105] suspends the sub CPU until an
        // IRQ, FIRQ or NMI request is observed, even without SYNC/CWAI.
        if(display_.subWaitForInterrupt()) {
            const bool wake=interruptRequest;
            if(!wake) {
                GetMemByte(regs.r.w.PC);
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
