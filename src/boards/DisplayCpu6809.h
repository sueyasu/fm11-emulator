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
    void resetCpu() {SimRESET();}
    bool executeOne() {
        if(display_.haltRequested())return false;
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
    std::uint16_t pc() const {return regs.r.w.PC;}
private:
    DisplaySubsystem& display_;
};
