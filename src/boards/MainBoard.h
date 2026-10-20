#pragma once
#include "FloppyDevice.h"
#include "Usart8251.h"
#include "Ptm6840.h"
#include "DisplaySubsystem.h"
#include "DisplayCpu6809.h"
#include <cstdint>
#include <array>
#include <functional>
#include <memory>
#include <limits>
#include <stdexcept>

// Main-board side of the FM-11 system bus. CPU-card address translation,
// arbitration and DMA controllers remain on their respective cards.
// The sub CPU/display subsystem will also belong to the main board.
class MainBoard {
public:
    MainBoard() {display_.setAttentionSink([this](){signalAttention();});}
    MainBoard(const MainBoard&)=delete;
    MainBoard& operator=(const MainBoard&)=delete;
    // BUSY is available separately until FD05 status bit semantics are proven.
    BYTE displayBusyStatus() const {return display_.busyStatus();}
    BYTE displayControl() const {return displayControl_;}
    BYTE displayPalette(unsigned index) const {return displayPalette_[index&7];}
    std::shared_ptr<FloppyDevice> floppyDevice() const { return floppy_; }
    std::shared_ptr<Usart8251> usartDevice() const {return usart_;}
    std::shared_ptr<Ptm6840> ptmDevice() const {return ptm_;}
    DisplaySubsystem& displaySubsystem() {return display_;}
    const DisplaySubsystem& displaySubsystem() const {return display_;}
    DisplayCpu6809& displayCpu() {return displayCpu_;}
    const DisplayCpu6809& displayCpu() const {return displayCpu_;}
    // System reset is distinct from resetting an individual CPU card.
    // Disk media remain mounted; only any in-flight transfer is aborted.
    void reset() {
        resetEvents();
        resetSubCpuControl();
        displayCpu_.resetCpu();
        resetTimebase();
        externalIrqLines_=0;
        usart_->reset();
        ptm_->reset();
        floppy_->finishTransfer();
    }
    // FD04 main-board event latch: ATENT bit 0, BREAK bit 1.
    void signalAttention() {eventStatus_|=0x01;}
    void signalBreak() {eventStatus_|=0x02;}
    void resetEvents() {eventStatus_=0;eventControl_=0;}
    BYTE eventStatus() const {return eventStatus_;}
    BYTE eventControl() const {return eventControl_;}
    // FD05 bit 7 is the source emulator's latched $D05 BUSY status.
    // HALT release alone must not report READY.
    BYTE subCpuStatus() const {
        return display_.fd05Status();
    }
    BYTE subCpuControl() const {return subCpuControl_;}
    void setFd05WriteObserver(std::function<void(BYTE)> cb) {fd05WriteObserver_=std::move(cb);}
    // FD05 bit 0 selects the optional CPU-card bus-master request.
    // Arbitration/CPU suspension remains in the individual CPU cards.
    bool optionalCpuRequested() const {return (subCpuControl_&0x01)!=0;}
    void resetSubCpuControl() {
        subCpuControl_=0;
        displayControl_=0;
        for(unsigned i=0;i<8;++i)displayPalette_[i]=static_cast<BYTE>(i);
        display_.reset();
    }

    // One common fractional phase, in billionths of a PTM tick.
    // Nanosecond slices accumulate exactly, without Q32 rounding loss.
    void advanceTimeNanoseconds(std::uint64_t nanoseconds) {
        constexpr std::uint64_t second=1000000000ULL, ptmHz=9700;
        const auto whole=nanoseconds/second;
        if(whole>std::numeric_limits<std::uint64_t>::max()/ptmHz)
            throw std::overflow_error("PTM time interval too large");
        advancePtmFraction(whole*ptmHz,(nanoseconds%second)*ptmHz);
    }
    void advanceCpuCycles(std::uint64_t cycles,std::uint64_t hz) {
        if(hz==0 || hz>1000000000ULL)
            throw std::invalid_argument("invalid main CPU clock frequency");
        constexpr std::uint64_t ptmHz=9700, phaseUnit=1000000000ULL;
        const auto whole=cycles/hz;
        if(whole>std::numeric_limits<std::uint64_t>::max()/ptmHz)
            throw std::overflow_error("PTM CPU interval too large");
        const auto scaled=(cycles%hz)*ptmHz;
        const auto integer=scaled/hz;
        // (scaled % hz)*phaseUnit is below 1e18, within uint64_t.
        const auto fraction=((scaled%hz)*phaseUnit)/hz;
        if(whole*ptmHz>std::numeric_limits<std::uint64_t>::max()-integer)
            throw std::overflow_error("PTM CPU interval too large");
        advancePtmFraction(whole*ptmHz+integer,fraction);
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
        if(reg==0x05) {value=subCpuStatus();return true;}
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
        if(reg==0xa0) {
            displayControl_=value;
            return true;
        }
        if(reg>=0x98 && reg<=0x9f) {
            displayPalette_[reg&7]=value&0x0f;
            return true;
        }
        if(reg==0x05) {
            subCpuControl_=value;
            // Reference main-side FD05 bit 6 (CANCEL) requests the
            // display sub-CPU IRQ. The request remains pending until
            // the sub CPU acknowledges it by reading AFE5.
            if(value&0x40)display_.requestSubIrq();
            if(value&0x80)display_.requestHalt();
            else display_.releaseHalt();
            if(fd05WriteObserver_)fd05WriteObserver_(value);
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
    DisplaySubsystem display_;
    DisplayCpu6809 displayCpu_{display_};
    BYTE subCpuControl_=0;
    std::function<void(BYTE)> fd05WriteObserver_;
    BYTE displayControl_=0;
    std::array<BYTE,8> displayPalette_{{0,1,2,3,4,5,6,7}};
    BYTE eventStatus_=0;
    BYTE eventControl_=0;
    BYTE externalIrqLines_=0;
    void advancePtmFraction(std::uint64_t ticks,std::uint64_t fraction) {
        constexpr std::uint64_t phaseUnit=1000000000ULL;
        // fraction may exceed one tick for nanosecond intervals.
        const auto extra=fraction/phaseUnit;
        const auto phase=ptmPhase_+(fraction%phaseUnit);
        const auto carry=phase/phaseUnit;
        if(ticks>std::numeric_limits<std::uint64_t>::max()-extra ||
           ticks+extra>std::numeric_limits<std::uint64_t>::max()-carry)
            throw std::overflow_error("PTM time interval too large");
        ptmPhase_=phase%phaseUnit;
        ptm_->advance(ticks+extra+carry);
        setIrqLine(IrqLine::Timer,ptm_->irqRequest());
    }
    std::uint64_t ptmPhase_=0;
    std::shared_ptr<FloppyDevice> floppy_=std::make_shared<FloppyDevice>();
    std::shared_ptr<Usart8251> usart_=std::make_shared<Usart8251>();
    std::shared_ptr<Ptm6840> ptm_=std::make_shared<Ptm6840>();
};
