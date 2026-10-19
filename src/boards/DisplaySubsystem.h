#pragma once
#include "portable_types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <functional>

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
        busy_=0x80;
        subIoRam_.fill(0);
        // ROM contents and display memory survive a CPU reset. Hardware
        // power-on clearing is a separate future operation.
    }
    void requestHalt() {haltRequested_=true;}
    bool haltRequested() const {return haltRequested_;}

    void loadRom(const RomImage& image) {rom_=image;}
    BYTE busyStatus() const {return busy_;}
    // Handshake output to the main board. The physical sub-I/O trigger
    // remains unassigned until its register decode is confirmed.
    void setAttentionSink(std::function<void()> sink) {attentionSink_=std::move(sink);}
    void notifyMainAttention() {if(attentionSink_)attentionSink_();}
    // The top 128 bytes of sub RAM also appear at main $FC80-$FCFF.
    BYTE readShared(std::size_t offset) const {return ram_[0x1f80+offset];}
    void writeShared(std::size_t offset,BYTE value) {ram_[0x1f80+offset]=value;}
    BYTE readMemory(std::uint16_t address) const {
        if(address<0x8000)return vram_[address];
        if(address<0xa000)return ram_[address-0x8000];
        if(address>=0xafe0 && address<=0xafe3)return subIoRam_[address-0xafe0];
        if(address==0xaff0)return busy_;
        if(address>=0xc000)return rom_[address-0xc000];
        return 0xff; // unmapped space, including unimplemented sub I/O
    }
    void writeMemory(std::uint16_t address,BYTE value) {
        if(address<0x8000)vram_[address]=value;
        else if(address<0xa000)ram_[address-0x8000]=value;
        else if(address>=0xafe0 && address<=0xafe3)subIoRam_[address-0xafe0]=value;
        else if(address==0xaff0)busy_=value;
        // ROM and unmapped space are read-only.
    }
    const std::array<BYTE,kVramSize>& vram() const {return vram_;}

private:
    std::function<void()> attentionSink_;
    bool haltRequested_=false;
    BYTE busy_=0x80;
    std::array<BYTE,4> subIoRam_{};
    std::array<BYTE,kVramSize> vram_{};
    std::array<BYTE,kRamSize> ram_{};
    RomImage rom_{};
};
