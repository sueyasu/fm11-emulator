#pragma once
#include "portable_types.h"
#include <cstdint>
#include <deque>
#include <limits>

// Intel 8251A shared on the FM-11 system board (FD06 data, FD07 control).
// The default byte-complete PTY transport is retained for compatibility.
// When setBaudRate() supplies a nonzero line rate, advanceNanoseconds()
// drives distinct Tx holding/shift and Rx holding/shift registers.
class Usart8251 {
public:
    enum class ControlPhase { Mode, Sync1, Sync2, Command };
    void reset() {
        input_.clear(); output_.clear();
        mode_=command_=sync1_=sync2_=errors_=0;
        phase_=ControlPhase::Mode;
        txIrqEnabled_=true;
        cts_=dsr_=dcd_=true;
        syncDetected_=false;
        txHoldingFull_=txShiftBusy_=rxHoldingFull_=rxShiftBusy_=false;
        txTimeLeft_=rxTimeLeft_=0;
        // The physical clock comes from external DIP circuitry and is not
        // reset when the USART itself receives a RESET signal.
    }
    void setTxIrqEnabled(bool enabled) {txIrqEnabled_=enabled;}
    void setModemInputs(bool cts,bool dsr,bool dcd=true) {
        cts_=cts;dsr_=dsr;dcd_=dcd;
    }
    // Zero selects the original instantaneous host transport.
    // This is a *line* baud rate after the 8251 mode's x1/x16/x64 division.
    void setBaudRate(unsigned baud) {
        baud_=baud;
        txHoldingFull_=txShiftBusy_=rxHoldingFull_=rxShiftBusy_=false;
        txTimeLeft_=rxTimeLeft_=0;
        input_.clear();
        output_.clear();
    }
    unsigned baudRate() const {return baud_;}
    // The serial frontend provides completed host characters. In clocked
    // mode this is the pre-receiver wire queue, not the read-data register.
    void receive(BYTE value) {input_.push_back(value);}
    bool takeOutput(BYTE& value) {
        if(output_.empty())return false;
        value=output_.front();output_.pop_front();return true;
    }
    BYTE status() const {
        BYTE result=static_cast<BYTE>(errors_ | (dsr_?0x80:0));
        if(txReady())result|=0x01;
        if(rxReady())result|=0x02;
        if(transmitterEmpty())result|=0x04;
        if(syncDetected_)result|=0x40;
        return result;
    }
    BYTE readData() {
        if(!rxReady())return 0;
        if(baud_) {
            rxHoldingFull_=false;
            return rxHolding_;
        }
        const BYTE value=input_.front();
        input_.pop_front();
        return value;
    }
    void writeData(BYTE value) {
        if(!txReady())return;
        if(!baud_) {
            output_.push_back(value);
            return;
        }
        // First write goes directly into the free shift register. One more
        // byte can be staged in the CPU-facing data buffer.
        if(!txShiftBusy_)startTransmit(value);
        else {txHolding_=value;txHoldingFull_=true;}
    }
    void writeCommand(BYTE value) {
        switch(phase_) {
        case ControlPhase::Mode:
            mode_=value;
            phase_=(mode_&0x03)?ControlPhase::Command:ControlPhase::Sync1;
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
            if(value&0x40) {
                mode_=command_=errors_=0;
                phase_=ControlPhase::Mode;
                syncDetected_=false;
                txHoldingFull_=txShiftBusy_=rxHoldingFull_=rxShiftBusy_=false;
                txTimeLeft_=rxTimeLeft_=0;
                return;
            }
            if(value&0x10)errors_=0;
            if(value&0x80)syncDetected_=false;
            command_=static_cast<BYTE>(value&~0x10u);
            return;
        }
    }
    bool irqRequest() const {
        return (txIrqEnabled_ && txReady()) || rxReady();
    }
    bool txReady() const {
        return phase_==ControlPhase::Command && (command_&0x01) && cts_ &&
               !(command_&0x08) && (!baud_ || !txHoldingFull_);
    }
    bool rxReady() const {
        return phase_==ControlPhase::Command && (command_&0x04) &&
               (baud_ ? rxHoldingFull_ : !input_.empty());
    }
    // Advance only when an external baud clock is configured. The default
    // frontend has no timing source yet and retains its immediate behavior.
    void advanceNanoseconds(std::uint64_t nanoseconds) {
        if(!baud_ || phase_!=ControlPhase::Command || !nanoseconds)return;
        advanceTransmit(nanoseconds);
        advanceReceive(nanoseconds);
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
    std::uint64_t frameNanoseconds() const {
        // Use half-bits so that async 1.5 stop bits are representable.
        // Bits 2-3: 5..8 data, bit 4: parity, bits 6-7: stop count.
        const unsigned dataBits=5+((mode_>>2)&3);
        const unsigned parity=(mode_&0x10)?1:0;
        const bool async=(mode_&0x03)!=0;
        unsigned halfBits=2*(dataBits+parity+(async?1:0));
        if(async) {
            switch((mode_>>6)&3) {
            case 1:halfBits+=2;break;
            case 2:halfBits+=3;break;
            case 3:halfBits+=4;break;
            default:break; // invalid stop setting: no invented stop bits
            }
        }
        // Round up: a frame cannot finish earlier than its final bit.
        return (static_cast<std::uint64_t>(halfBits)*500000000ULL+baud_-1)/baud_;
    }
    void startTransmit(BYTE value) {
        txShift_=value;
        txShiftBusy_=true;
        txTimeLeft_=frameNanoseconds();
    }
    void advanceTransmit(std::uint64_t elapsed) {
        while(txShiftBusy_ && elapsed>=txTimeLeft_) {
            elapsed-=txTimeLeft_;
            output_.push_back(txShift_);
            txShiftBusy_=false;
            txTimeLeft_=0;
            if(txHoldingFull_) {
                const BYTE value=txHolding_;
                txHoldingFull_=false;
                startTransmit(value);
            }
        }
        if(txShiftBusy_)txTimeLeft_-=elapsed;
    }
    void advanceReceive(std::uint64_t elapsed) {
        if(!(command_&0x04))return;
        while(true) {
            if(!rxShiftBusy_) {
                if(input_.empty())return;
                rxShift_=input_.front();
                input_.pop_front();
                rxShiftBusy_=true;
                rxTimeLeft_=frameNanoseconds();
            }
            if(elapsed<rxTimeLeft_) {
                rxTimeLeft_-=elapsed;
                return;
            }
            elapsed-=rxTimeLeft_;
            rxShiftBusy_=false;
            rxTimeLeft_=0;
            if(rxHoldingFull_)errors_|=0x10; // receive overrun, newest wins
            rxHolding_=rxShift_;
            rxHoldingFull_=true;
            if(!elapsed)return;
        }
    }
    bool transmitterEmpty() const {
        return baud_ ? (!txShiftBusy_ && !txHoldingFull_) : output_.empty();
    }
    std::deque<BYTE> input_,output_;
    BYTE mode_=0,command_=0,sync1_=0,sync2_=0,errors_=0;
    BYTE txHolding_=0,txShift_=0,rxHolding_=0,rxShift_=0;
    ControlPhase phase_=ControlPhase::Mode;
    bool txIrqEnabled_=true,cts_=true,dsr_=true,dcd_=true;
    bool syncDetected_=false;
    bool txHoldingFull_=false,txShiftBusy_=false,rxHoldingFull_=false,rxShiftBusy_=false;
    unsigned baud_=0;
    std::uint64_t txTimeLeft_=0,rxTimeLeft_=0;
};
