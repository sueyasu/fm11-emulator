#pragma once
#include "portable_types.h"
#include <deque>

// Intel 8251A shared on the FM-11 system board (FD06 data, FD07 control).
// Byte-complete host transport: serial bit-clock pacing and external DIP
// clock generation are deliberately separate from this register-level model.
class Usart8251 {
public:
    enum class ControlPhase { Mode, Sync1, Sync2, Command };
    void reset() {
        input_.clear(); output_.clear();
        mode_=command_=sync1_=sync2_=0;
        phase_=ControlPhase::Mode;
        errors_=0;
        txIrqEnabled_=true;
        cts_=dsr_=true;
        dcd_=true;
        syncDetected_=false;
    }
    void setTxIrqEnabled(bool enabled) {txIrqEnabled_=enabled;}
    void setModemInputs(bool cts,bool dsr,bool dcd=true) {
        cts_=cts;dsr_=dsr;dcd_=dcd;
    }
    // Host bytes are buffered upstream of the 8251 receive-data register.
    // The PTY currently submits whole bursts at once, not byte-clock events.
    // Do not mistake host burst size for a physical receiver overrun.
    void receive(BYTE value) {input_.push_back(value);}
    bool takeOutput(BYTE& value) {
        if(output_.empty())return false;
        value=output_.front();output_.pop_front();return true;
    }
    BYTE status() const {
        BYTE result=static_cast<BYTE>(errors_ | (dsr_?0x80:0));
        if(txReady())result|=0x01; // TxRDY
        if(rxReady())result|=0x02; // RxRDY
        if(output_.empty())result|=0x04; // TxEMPTY: host transport approximation
        if(syncDetected_)result|=0x40;
        return result;
    }
    BYTE readData() {
        if(!rxReady())return 0;
        const BYTE value=input_.front();
        input_.pop_front();
        return value;
    }
    void writeData(BYTE value) {
        if(!txReady())return;
        output_.push_back(value);
    }
    void writeCommand(BYTE value) {
        switch(phase_) {
        case ControlPhase::Mode:
            mode_=value;
            if((mode_&0x03)!=0)phase_=ControlPhase::Command;
            else phase_=ControlPhase::Sync1;
            return;
        case ControlPhase::Sync1:
            sync1_=value;
            phase_=(mode_&0x80)?ControlPhase::Command:ControlPhase::Sync2;
            return;
        case ControlPhase::Sync2:
            sync2_=value;
            phase_=ControlPhase::Command;
            return;
        case ControlPhase::Command:
            if(value&0x40) { // Internal Reset: next control write is a mode word
                mode_=command_=0;
                phase_=ControlPhase::Mode;
                errors_=0;
                syncDetected_=false;
                return;
            }
            if(value&0x10)errors_=0; // Error Reset clears PE/OE/FE
            if(value&0x80)syncDetected_=false; // Enter Hunt Mode
            command_=static_cast<BYTE>(value&~0x10u); // ER is a pulse
            return;
        }
    }
    bool irqRequest() const {
        return (txIrqEnabled_ && txReady()) || rxReady();
    }
    bool txReady() const {
        return phase_==ControlPhase::Command && (command_&0x01) && cts_ &&
               !(command_&0x08);
    }
    bool rxReady() const {
        return phase_==ControlPhase::Command && (command_&0x04) &&
               !input_.empty();
    }
    ControlPhase phase() const {return phase_;}
    BYTE mode() const {return mode_;}
    BYTE command() const {return command_;}
    BYTE sync1() const {return sync1_;}
    BYTE sync2() const {return sync2_;}
    bool dcd() const {return dcd_;}
    void signalReceiveError(bool parity,bool overrun,bool framing) {
        if(parity)errors_|=0x08;
        if(overrun)errors_|=0x10;
        if(framing)errors_|=0x20;
    }
    void setSyncDetected(bool detected) {syncDetected_=detected;}
private:
    std::deque<BYTE> input_,output_;
    BYTE mode_=0,command_=0,sync1_=0,sync2_=0,errors_=0;
    ControlPhase phase_=ControlPhase::Mode;
    bool txIrqEnabled_=true,cts_=true,dsr_=true,dcd_=true;
    bool syncDetected_=false;
};
