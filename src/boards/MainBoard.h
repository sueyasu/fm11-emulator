#pragma once
#include "FloppyDevice.h"
#include "Usart8251.h"
#include "Ptm6840.h"
#include <cstdint>
#include <memory>
#include <limits>
#include <stdexcept>

// Main-board side of the FM-11 system bus. CPU-card address translation,
// arbitration and DMA controllers remain on their respective cards.
// The sub CPU/display subsystem will also belong to the main board.
class MainBoard {
public:
    std::shared_ptr<FloppyDevice> floppyDevice() const { return floppy_; }
    std::shared_ptr<Usart8251> usartDevice() const {return usart_;}
    std::shared_ptr<Ptm6840> ptmDevice() const {return ptm_;}
    // FD04 main-board event latch: ATENT bit 0, BREAK bit 1.
    void signalAttention() {eventStatus_|=0x01;}
    void signalBreak() {eventStatus_|=0x02;}
    void resetEvents() {eventStatus_=0;eventControl_=0;}
    BYTE eventStatus() const {return eventStatus_;}
    BYTE eventControl() const {return eventControl_;}
    // FD05: common main/sub control output and the CPU-visible HALT flag.
    // The actual sub-CPU execution/arbitration is not implemented here.
    BYTE subCpuStatus() const {return subCpuStatus_;}
    BYTE subCpuControl() const {return subCpuControl_;}
    // FD05 bit 0 selects the optional CPU-card bus-master request.
    // Arbitration/CPU suspension remains in the individual CPU cards.
    bool optionalCpuRequested() const {return (subCpuControl_&0x01)!=0;}
    void resetSubCpuControl() {subCpuStatus_=subCpuControl_=0;}

    // The PTM is clocked at 9700 Hz independently of the active CPU.
    // A shared Q32 fixed-point phase preserves sub-tick time across
    // changes between 6809, 8088 and wall-clock stepping.
    void advanceTimeNanoseconds(std::uint64_t nanoseconds) {
        advancePtmTime(nanoseconds,1000000000ULL);
    }
    void advanceCpuCycles(std::uint64_t cycles,std::uint64_t hz) {
        if(hz==0 || hz>1000000000ULL)
            throw std::invalid_argument("invalid main CPU clock frequency");
        advancePtmTime(cycles,hz);
    }
    void resetTimebase() {ptmPhase_=0;}

    // Main-board sources presented to the 8088 card's 8259 inputs.
    // Corresponds to the original Sim86 interrupt wiring. Each group may
    // eventually combine multiple physical device signals.
    enum class IrqLine : unsigned {
        Timer=0, KeyboardBreak=1, Disk8InchHdd=2,
        AttentionExtendedFirq=3, Rs232c=4,
        CoprocessorExtendedIrq=5, Disk5Inch=6, PrinterAck=7
    };
    void setIrqLine(IrqLine line,bool asserted) {
        const BYTE bit=static_cast<BYTE>(1u<<static_cast<unsigned>(line));
        if(asserted) externalIrqLines_|=bit;
        else externalIrqLines_&=static_cast<BYTE>(~bit);
    }
    BYTE irqLines() const {
        // USART is a shared device, not owned by either CPU card.
        return static_cast<BYTE>(externalIrqLines_ |
               (usart_->irqRequest()?0x10:0));
    }

    // Common system I/O aperture, decoded after CPU-card bus translation.
    // The physical PTM, USART and FDC parameter registers live here.
    bool readIo(std::uint16_t address, BYTE& value) {
        const unsigned reg=address&0xff;
        if(reg==0x05) {value=subCpuStatus_;return true;}
        if(reg==0x04) {
            value=eventStatus_;
            eventStatus_&=static_cast<BYTE>(~0x03);
            return true;
        }
        if(reg>=0x38 && reg<=0x3f) {
            value=ptm_->read(static_cast<BYTE>(reg));
            const_cast<MainBoard*>(this)->setIrqLine(IrqLine::Timer,ptm_->irqRequest());
            return true;
        }
        if(reg==0x06) {value=usart_->readData();return true;}
        if(reg==0x07) {value=usart_->status();return true;}
        if((reg>=0x19 && reg<=0x1a) || (reg>=0x1c && reg<=0x1d) ||
           (reg>=0x31 && reg<=0x32) || (reg>=0x34 && reg<=0x35)) {
            value=floppy_->registerValue(reg);
            return true;
        }
        return false;
    }
    bool writeIo(std::uint16_t address,BYTE value) {
        const unsigned reg=address&0xff;
        if(reg==0x05) {
            subCpuControl_=value;
            if(value&0x80)subCpuStatus_|=0x80;
            return true;
        }
        if(reg==0x04) {
            eventStatus_&=static_cast<BYTE>(~0x01);
            eventControl_=0x80;
            return true;
        }
        if(reg>=0x38 && reg<=0x3f) {
            ptm_->write(static_cast<BYTE>(reg),value);
            setIrqLine(IrqLine::Timer,ptm_->irqRequest());
            return true;
        }
        if(reg==0x06) {usart_->writeData(value);return true;}
        if(reg==0x07) {usart_->writeCommand(value);return true;}
        BYTE ignored=0;
        if(!readIo(address,ignored))return false;
        floppy_->writeRegister(address&0xff,value);
        return true;
    }
private:
    BYTE subCpuStatus_=0,subCpuControl_=0;
    BYTE eventStatus_=0;
    BYTE eventControl_=0;
    BYTE externalIrqLines_=0;
    void advancePtmTime(std::uint64_t amount,std::uint64_t unitsPerSecond) {
        constexpr std::uint64_t ptmHz=9700;
        const std::uint64_t whole=amount/unitsPerSecond;
        const std::uint64_t remainder=amount%unitsPerSecond;
        if(whole>std::numeric_limits<std::uint64_t>::max()/ptmHz)
            throw std::overflow_error("PTM time interval too large");
        // Q32 fractional PTM ticks; 9700 * (unitsPerSecond - 1)
        // fits in uint64_t for the supported <= 1 GHz input clocks.
        // Split integer/fraction before shifting to avoid __int128.
        constexpr unsigned fractionBits=32;
        constexpr std::uint64_t fractionUnit=std::uint64_t{1}<<fractionBits;
        const std::uint64_t scaled=remainder*ptmHz;
        const std::uint64_t integerPart=scaled/unitsPerSecond;
        const std::uint64_t fractionalPart=
            ((scaled%unitsPerSecond)<<fractionBits)/unitsPerSecond;
        const std::uint64_t combined=ptmPhase_+fractionalPart;
        const std::uint64_t carry=combined/fractionUnit;
        const std::uint64_t additional=integerPart+carry;
        const std::uint64_t base=whole*ptmHz;
        if(base>std::numeric_limits<std::uint64_t>::max()-additional)
            throw std::overflow_error("PTM time interval too large");
        ptmPhase_=combined%fractionUnit;
        ptm_->advance(base+additional);
        setIrqLine(IrqLine::Timer,ptm_->irqRequest());
    }
    std::uint64_t ptmPhase_=0;
    std::shared_ptr<FloppyDevice> floppy_=std::make_shared<FloppyDevice>();
    std::shared_ptr<Usart8251> usart_=std::make_shared<Usart8251>();
    std::shared_ptr<Ptm6840> ptm_=std::make_shared<Ptm6840>();
};
