#pragma once
#include "FloppyDevice.h"
#include <cstdint>
#include <memory>

// Main-board side of the FM-11 system bus. CPU-card address translation,
// arbitration and DMA controllers remain on their respective cards.
// The sub CPU/display subsystem will also belong to the main board.
class MainBoard {
public:
    std::shared_ptr<FloppyDevice> floppyDevice() const { return floppy_; }

    // Common system I/O aperture, decoded after the CPU card has translated
    // its own bus cycles. Only shared FDC parameter registers are claimed so
    // far; other registers remain with their existing owners.
    bool readIo(std::uint16_t address, BYTE& value) const {
        const unsigned reg=address&0xff;
        if((reg>=0x19 && reg<=0x1a) || (reg>=0x1c && reg<=0x1d) ||
           (reg>=0x31 && reg<=0x32) || (reg>=0x34 && reg<=0x35)) {
            value=floppy_->registerValue(reg);
            return true;
        }
        return false;
    }
    bool writeIo(std::uint16_t address,BYTE value) {
        BYTE ignored=0;
        if(!readIo(address,ignored))return false;
        floppy_->writeRegister(address&0xff,value);
        return true;
    }
private:
    std::shared_ptr<FloppyDevice> floppy_=std::make_shared<FloppyDevice>();
};
