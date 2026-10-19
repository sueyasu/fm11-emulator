#pragma once
#include "FloppyDevice.h"
#include "Usart8251.h"
#include "Ptm6840.h"
#include <cstdint>
#include <memory>

// Main-board side of the FM-11 system bus. CPU-card address translation,
// arbitration and DMA controllers remain on their respective cards.
// The sub CPU/display subsystem will also belong to the main board.
class MainBoard {
public:
    std::shared_ptr<FloppyDevice> floppyDevice() const { return floppy_; }
    std::shared_ptr<Usart8251> usartDevice() const {return usart_;}
    std::shared_ptr<Ptm6840> ptmDevice() const {return ptm_;}

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
    std::shared_ptr<FloppyDevice> floppy_=std::make_shared<FloppyDevice>();
    std::shared_ptr<Usart8251> usart_=std::make_shared<Usart8251>();
    std::shared_ptr<Ptm6840> ptm_=std::make_shared<Ptm6840>();
};
