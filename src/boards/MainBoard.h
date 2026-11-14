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
#include <atomic>
#include <chrono>
#include <mutex>

// Main-board side of the FM-11 system bus. CPU-card address translation,
// arbitration and DMA controllers remain on their respective cards.
// The sub CPU/display subsystem will also belong to the main board.
class MainBoard {
public:
    MainBoard() {display_.setAttentionSink([this](){signalAttention();});}
    MainBoard(const MainBoard&)=delete;
    MainBoard& operator=(const MainBoard&)=delete;
    // BUSY is available separately until FD05 status bit semantics are proven.
    BYTE displayBusyStatus() const {
        auto lock=lockDisplay();
        return display_.busyStatus();
    }
    BYTE displayControl() const {
        auto lock=lockDisplay();
        return displayControl_;
    }
    BYTE displayPalette(unsigned index) const {
        auto lock=lockDisplay();
        return displayPalette_[index&7];
    }
    std::shared_ptr<FloppyDevice> floppyDevice() const { return floppy_; }
    std::shared_ptr<Usart8251> usartDevice() const {return usart_;}
    std::shared_ptr<Ptm6840> ptmDevice() const {return ptm_;}
    // Shared display bus: lock each CPU instruction/mailbox transaction
    // and capture/render operation, never the entire main CPU loop.
    std::unique_lock<std::recursive_mutex> lockDisplay() const {
        return std::unique_lock<std::recursive_mutex>(displayMutex_);
    }
    BYTE readDisplayShared(std::size_t offset) {
        auto lock=lockDisplay();
        return display_.readShared(offset);
    }
    void writeDisplayShared(std::size_t offset,BYTE value) {
        auto lock=lockDisplay();
        display_.writeShared(offset,value);
    }
    DisplaySubsystem& displaySubsystem() {return display_;}
    const DisplaySubsystem& displaySubsystem() const {return display_;}
    DisplayCpu6809& displayCpu() {return displayCpu_;}
    const DisplayCpu6809& displayCpu() const {return displayCpu_;}
    // System reset is distinct from resetting an individual CPU card.
    // Disk media remain mounted; only any in-flight transfer is aborted.
    void reset() {
        resetEvents();
        resetSubCpuControl();
        {auto lock=lockDisplay();displayCpu_.resetCpu();}
        resetTimebase();
        externalIrqLines_=0;
        usart_->reset();
        {
            std::lock_guard<std::mutex> lock(ptmMutex_);
            ptm_->reset();
            ptmIrqLevel_.store(false,std::memory_order_release);
            realtimePtmLast_=std::chrono::steady_clock::now();
        }
        floppy_->finishTransfer();
    }
    // FD04 main-board event latch: ATENT bit 0, BREAK bit 1.
    void signalAttention() {eventStatus_.fetch_or(0x01,std::memory_order_release);}
    void signalBreak() {eventStatus_.fetch_or(0x02,std::memory_order_release);}
    void resetEvents() {eventStatus_.store(0);eventControl_=0;}
    BYTE eventStatus() const {return eventStatus_.load(std::memory_order_acquire);}
    BYTE eventControl() const {return eventControl_;}
    // FD05 bit 7 is the source emulator's latched $D05 BUSY status.
    // HALT release alone must not report READY.
    BYTE subCpuStatus() const {
        auto lock=lockDisplay();
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
        auto lock=lockDisplay();
        display_.reset();
    }

    // Runtime PTM mode is opt-in. All methods touching PTM state serialize
    // with the dedicated timer worker. Other bus state remains single-owner
    // until the later CPU-thread commits.
    void startRealtimePtm() {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        realtimePtmLast_=std::chrono::steady_clock::now();
        realtimePtm_.store(true,std::memory_order_release);
    }
    void stopRealtimePtm() {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        realtimePtm_.store(false,std::memory_order_release);
    }
    bool realtimePtmEnabled() const {
        return realtimePtm_.load(std::memory_order_acquire);
    }
    void rebaseRealtimePtm() {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        realtimePtmLast_=std::chrono::steady_clock::now();
    }
    void pollRealtimePtm() {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        advanceRealtimePtmLocked();
    }
    BYTE readPtmRegister(BYTE reg) {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        advanceRealtimePtmLocked();
        const BYTE value=ptm_->read(reg);
        updatePtmIrqLocked();
        return value;
    }
    void writePtmRegister(BYTE reg,BYTE value) {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        advanceRealtimePtmLocked();
        ptm_->write(reg,value);
        updatePtmIrqLocked();
    }
    // CPU-card/legacy tests use this to advance the shared timer without
    // bypassing the lock or leaving the published IRQ level stale.
    // Real-time mode is exclusively advanced by the timer worker.
    void advancePtmTicks(std::uint64_t ticks) {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        if(realtimePtmEnabled())return;
        ptm_->advance(ticks);
        updatePtmIrqLocked();
    }
    bool ptmIrqLevel() const {
        return ptmIrqLevel_.load(std::memory_order_acquire);
    }
    std::uint64_t nextPtmEventNanoseconds() {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        advanceRealtimePtmLocked();
        return ptm_->nextEventNanoseconds();
    }
    // Legacy timebase remains available for headless tests and standalone
    // emulation. A running real-time timer MUST NOT advance twice.
    void advanceTimeNanoseconds(std::uint64_t nanoseconds) {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        if(realtimePtmEnabled())return;
        constexpr std::uint64_t second=1000000000ULL, ptmHz=9700;
        const auto whole=nanoseconds/second;
        if(whole>std::numeric_limits<std::uint64_t>::max()/ptmHz)
            throw std::overflow_error("PTM time interval too large");
        advancePtmFraction(whole*ptmHz,(nanoseconds%second)*ptmHz);
    }
    void advanceCpuCycles(std::uint64_t cycles,std::uint64_t hz) {
        std::lock_guard<std::mutex> lock(ptmMutex_);
        if(realtimePtmEnabled())return;
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
    void resetTimebase() {std::lock_guard<std::mutex> lock(ptmMutex_);ptmPhase_=0;}

    // Main-board sources presented to the 8088 card's 8259 inputs.
    // Corresponds to the original Sim86 interrupt wiring. Each group may
    // eventually combine multiple physical device signals.
    enum class IrqLine : unsigned {
        Timer=0, KeyboardBreak=1, Disk8InchHdd=2,
        AttentionExtendedFirq=3, Rs232c=4,
        CoprocessorExtendedIrq=5, Disk5Inch=6, PrinterAck=7
    };
    void setIrqLine(IrqLine line,bool asserted) {
        if(line==IrqLine::Timer) {
            ptmIrqLevel_.store(asserted,std::memory_order_release);
            return;
        }
        const BYTE bit=static_cast<BYTE>(1u<<static_cast<unsigned>(line));
        if(asserted) externalIrqLines_|=bit;
        else externalIrqLines_&=static_cast<BYTE>(~bit);
    }
    BYTE irqLines() const {
        // USART is a shared device, not owned by either CPU card.
        return static_cast<BYTE>(externalIrqLines_ |
               (ptmIrqLevel()?0x01:0) |
               (usart_->irqRequest()?0x10:0));
    }

    // Common system I/O aperture, decoded after CPU-card bus translation.
    // The physical PTM, USART and FDC parameter registers live here.
    bool readIo(std::uint16_t address, BYTE& value) {
        const unsigned reg=address&0xff;
        if(reg==0x05) {value=subCpuStatus();return true;}
        if(reg==0x04) {
            value=eventStatus_.exchange(0,std::memory_order_acq_rel);
            return true;
        }
        if(reg>=0x38 && reg<=0x3f) {
            value=readPtmRegister(static_cast<BYTE>(reg));
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
            auto lock=lockDisplay();
            displayControl_=value;
            return true;
        }
        if(reg>=0x98 && reg<=0x9f) {
            auto lock=lockDisplay();
            displayPalette_[reg&7]=value&0x0f;
            return true;
        }
        if(reg==0x05) {
            auto lock=lockDisplay();
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
            eventStatus_.fetch_and(static_cast<BYTE>(~0x01),std::memory_order_acq_rel);
            eventControl_=0x80;
            return true;
        }
        if(reg>=0x38 && reg<=0x3f) {
            writePtmRegister(static_cast<BYTE>(reg),value);
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
    mutable std::recursive_mutex displayMutex_;
    DisplaySubsystem display_;
    DisplayCpu6809 displayCpu_{display_};
    BYTE subCpuControl_=0;
    std::function<void(BYTE)> fd05WriteObserver_;
    BYTE displayControl_=0;
    std::array<BYTE,8> displayPalette_{{0,1,2,3,4,5,6,7}};
    std::atomic<BYTE> eventStatus_{0};
    BYTE eventControl_=0;
    BYTE externalIrqLines_=0;
    mutable std::mutex ptmMutex_;
    std::atomic<bool> realtimePtm_{false};
    std::atomic<bool> ptmIrqLevel_{false};
    std::chrono::steady_clock::time_point realtimePtmLast_{};
    void updatePtmIrqLocked() {
        // Never mutate the 6809 CPU adapter from the PTM thread.
        ptmIrqLevel_.store(ptm_->irqRequest(),std::memory_order_release);
    }
    void advanceRealtimePtmLocked() {
        if(!realtimePtmEnabled())return;
        const auto now=std::chrono::steady_clock::now();
        const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(
            now-realtimePtmLast_).count();
        if(ns<=0)return;
        realtimePtmLast_=now;
        constexpr std::uint64_t second=1000000000ULL,ptmHz=9700;
        const auto elapsed=static_cast<std::uint64_t>(ns);
        advancePtmFraction((elapsed/second)*ptmHz,(elapsed%second)*ptmHz);
    }
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
        updatePtmIrqLocked();
    }
    std::uint64_t ptmPhase_=0;
    std::shared_ptr<FloppyDevice> floppy_=std::make_shared<FloppyDevice>();
    std::shared_ptr<Usart8251> usart_=std::make_shared<Usart8251>();
    std::shared_ptr<Ptm6840> ptm_=std::make_shared<Ptm6840>();
};
