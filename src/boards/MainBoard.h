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

    // PTM input clock is 9700 Hz. The fractional remainder makes elapsed
    // time independent of how the scheduler divides its time slices.
    // This clock is owned by the main board, not by the active CPU card.
    void advanceTimeNanoseconds(std::uint64_t nanoseconds) {
        constexpr std::uint64_t second=1000000000ULL;
        constexpr std::uint64_t ptmHz=9700;
        const auto seconds=nanoseconds/second;
        const auto remainder=nanoseconds%second;
        if(seconds>std::numeric_limits<std::uint64_t>::max()/ptmHz)
            throw std::overflow_error("main-board PTM time interval too large");
        const std::uint64_t baseTicks=seconds*ptmHz;
        const std::uint64_t numerator=remainder*ptmHz+ptmTimeRemainder_;
        const std::uint64_t extraTicks=numerator/second;
        if(baseTicks>std::numeric_limits<std::uint64_t>::max()-extraTicks)
            throw std::overflow_error("main-board PTM time interval too large");
        ptmTimeRemainder_=numerator%second;
        ptm_->advance(baseTicks+extraTicks);
        setIrqLine(IrqLine::Timer,ptm_->irqRequest());
    }
    void resetTimebase() {ptmTimeRemainder_=0;}

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

    // Common system I/O aperture, decoded after the CPU card has translated
    // its own bus cycles. Only shared FDC parameter registers are claimed so
    // far; other registers remain with their existing owners.
    bool readIo(std::uint16_t address, BYTE& value) const {
        const unsigned reg=address&0xff;
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
        if(reg==0x06) {usart_->writeData(value);return true;}
        if(reg==0x07) {usart_->writeCommand(value);return true;}
        BYTE ignored=0;
        if(!readIo(address,ignored))return false;
        floppy_->writeRegister(address&0xff,value);
        return true;
    }
private:
    BYTE externalIrqLines_=0;
    std::uint64_t ptmTimeRemainder_=0;
    std::shared_ptr<FloppyDevice> floppy_=std::make_shared<FloppyDevice>();
    std::shared_ptr<Usart8251> usart_=std::make_shared<Usart8251>();
    std::shared_ptr<Ptm6840> ptm_=std::make_shared<Ptm6840>();
};
