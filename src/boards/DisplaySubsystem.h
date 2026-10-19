#pragma once
#include "portable_types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

// Main-board display/sub-CPU memory. I/O and display scanout are deliberately
// not decoded yet: the reference map still labels those regions tentative.
class DisplaySubsystem {
public:
    static constexpr std::size_t kVramSize=0x8000;
    static constexpr std::size_t kRamSize=0x2000;
    static constexpr std::size_t kRomSize=0x4000;
    using RomImage=std::array<BYTE,kRomSize>;

    void reset() {
        haltRequested_=false;
        // ROM contents and display memory survive a CPU reset. Hardware
        // power-on clearing is a separate future operation.
    }
    void requestHalt() {haltRequested_=true;}
    bool haltRequested() const {return haltRequested_;}

    void loadRom(const RomImage& image) {rom_=image;}
    BYTE readMemory(std::uint16_t address) const {
        if(address<0x8000)return vram_[address];
        if(address<0xa000)return ram_[address-0x8000];
        if(address>=0xc000)return rom_[address-0xc000];
        return 0xff; // unmapped space, including unimplemented sub I/O
    }
    void writeMemory(std::uint16_t address,BYTE value) {
        if(address<0x8000)vram_[address]=value;
        else if(address<0xa000)ram_[address-0x8000]=value;
        // ROM and unmapped space are read-only.
    }
    const std::array<BYTE,kVramSize>& vram() const {return vram_;}

private:
    bool haltRequested_=false;
    std::array<BYTE,kVramSize> vram_{};
    std::array<BYTE,kRamSize> ram_{};
    RomImage rom_{};
};
