#pragma once
#include "DisplaySubsystem.h"
#include "Sim09.h"
#include <cstdint>

// The display CPU is a separate MC6809 with its own address space, not
// another main CPU card. All memory cycles reach the display subsystem.
class DisplayCpu6809 final : public CSim09 {
public:
    explicit DisplayCpu6809(DisplaySubsystem& display):display_(display) {
        Set6309Enabled(false);
    }
    BYTE GetMemByte(WORD address) override {
        return display_.readMemory(address);
    }
    void SetMemByte(WORD address,BYTE value) override {
        display_.writeMemory(address,value);
    }
    void resetCpu() {
        SimRESET();
        nmiCycles_=0;
        display_.resetInterruptClock();
    }
    bool executeOne() {
        // Windows SubCpu::SimONE handles pending timer/VSync first,
        // then GetKeyCode, independently of the periodic CPU NMI.
        if(display_.consumeVsyncEvent())display_.processKeyCode();
        if(display_.haltRequested())return false;
        // Reference SubCpu::SimONE: interrupt priority IRQ, FIRQ, NMI.
        // The original clock threshold is 60000 at reset, recalibrated
        // dynamically by the Windows event loop. Use CPU cycles here.
        // Reference SubCpu::SimONE: after AFE0 announces READY, the
        // display CPU stays idle until IRQ, FIRQ or NMI is serviced.
        if(display_.readyWait()) {
            const bool interruptPresent=
                display_.subIrqPending() || display_.subFirqPending();
            const bool periodicNmi=
                !display_.nmiMask() &&
                nmiCycles_>=display_.nmiClockCycles();
            if(!interruptPresent && !periodicNmi)return false;
            display_.clearReadyWait();
        }
        if(display_.subIrqPending())SimIRQ();
        if(display_.subFirqPending())SimFIRQ();
        if(!display_.nmiMask() && nmiCycles_>=display_.nmiClockCycles()) {
            // Original timed keyboard/vsync event calls GetKeyCode before
            // dispatching the periodic NMI. Without this, the initial
            // AFE0-generated key and queued host input remain dormant
            // until firmware happens to read AFED.
            SimNMI();
            nmiCycles_=0;
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
        nmiCycles_+=LastInstructionCycles();
        return true;
    }
    void advanceIdleCycles(std::uint64_t cycles) {nmiCycles_+=cycles;}
    std::uint16_t pc() const {return regs.r.w.PC;}
private:
    std::uint64_t nmiCycles_=0;
    DisplaySubsystem& display_;
};
