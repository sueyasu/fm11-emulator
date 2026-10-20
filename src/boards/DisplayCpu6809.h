#pragma once
#include "DisplaySubsystem.h"
#include "Sim09.h"
#include <cstdint>
#include <algorithm>
#include <chrono>

// The display CPU is a separate MC6809 with its own address space, not
// another main CPU card. All memory cycles reach the display subsystem.
class DisplayCpu6809 final : public CSim09 {
public:
    explicit DisplayCpu6809(DisplaySubsystem& display):display_(display) {
        Set6309Enabled(false);
        display_.setNmiEnableSink([this](){nmiCycles_=0;});
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
        lastNmiCalibration_=std::chrono::steady_clock::now();
        vsyncCalibrationMs_=0;
        display_.resetInterruptClock();
    }
    // Original SubCpu::SimONE measures the 20 ms NMI interval against
    // host elapsed time, not against the nominal WM_TIMER arguments.
    // ChildView::OnTimer only latches VSync; SubCpu::SimONE consumes it.
    void timerTick(std::uint64_t /*elapsedMs*/) {
        display_.signalVsyncEvent();
    }
    bool executeOne() {
        // Reference SubCpu::SimONE: VSync/GetKeyCode, HALT, IRQ,
        // FIRQ, NMI, Fm11Ram[105] wait gate, then one instruction.
        const bool vsync=display_.consumeVsyncEvent();
        if(vsync) {
            display_.processKeyCode();
            vsyncCalibrationMs_+=16;
        }
        // Source SubCpu::SimONE checks both thresholds only on VSync.
        if(vsync && (vsyncCalibrationMs_>=2000 || totalAccesses_>1000000)) {
                const auto now=std::chrono::steady_clock::now();
                const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
                    now-lastNmiCalibration_).count();
                if(elapsed>0 && totalAccesses_)
                    nmiThreshold_=std::max<std::uint64_t>(
                        1,20*totalAccesses_/static_cast<std::uint64_t>(elapsed));
                totalAccesses_=0;
                vsyncCalibrationMs_=0;
                lastNmiCalibration_=now;
        }
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
    std::uint64_t vsyncCalibrationMs_=0;
    std::chrono::steady_clock::time_point lastNmiCalibration_=
        std::chrono::steady_clock::now();
    DisplaySubsystem& display_;
};
