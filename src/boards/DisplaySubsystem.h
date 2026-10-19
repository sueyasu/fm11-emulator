#pragma once
#include "portable_types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <functional>
#include <algorithm>

// Main-board display/sub-CPU memory. I/O and display scanout are deliberately
// not decoded yet: the reference map still labels those regions tentative.
class DisplaySubsystem {
public:
    static constexpr std::size_t kVramSize=0x8000;
    static constexpr std::size_t kPhysicalVramSize=0x30000; // 192 KiB
    static constexpr std::size_t kVramWindows=kPhysicalVramSize/kVramSize;
    static constexpr std::size_t kRamSize=0x2000;
    static constexpr std::size_t kRomSize=0x4000;
    using RomImage=std::array<BYTE,kRomSize>;
    struct IoWriteTrace {std::uint16_t address; BYTE value;};
    using IoWriteObserver=std::function<void(IoWriteTrace)>;
    struct VideoSnapshot {
        std::array<BYTE,kVramSize> vram;
        BYTE flags;
        std::uint64_t revision;
    };

    void reset() {
        haltRequested_=false;
        busy_=0x80;
        if(videoFlags_!=0)markVideoChanged();
        videoFlags_=0;
        subIoRam_.fill(0);
        // ROM contents and display memory survive a CPU reset. Hardware
        // power-on clearing is a separate future operation.
    }
    void requestHalt() {haltRequested_=true;}
    void releaseHalt() {haltRequested_=false;}
    bool haltRequested() const {return haltRequested_;}

    void loadRom(const RomImage& image) {rom_=image;}
    // Diagnostic observer for sub-CPU writes to the undecoded A000-BFFF
    // aperture and known sub-I/O latches. Observation is non-invasive:
    // attaching an observer never changes readback or register side effects.
    void setIoWriteObserver(IoWriteObserver observer) {ioWriteObserver_=std::move(observer);}
    void clearIoWriteObserver() {ioWriteObserver_={};}
    BYTE busyStatus() const {return busy_;}
    // AFE6 video-control flags. Individual bits are not yet mapped to
    // display modes until their FM-11 semantics have been verified.
    BYTE videoFlags() const {return videoFlags_;}
    // Renderer-facing change tracking, independent of the eventual display
    // resolution and the meanings assigned to AFE6 bits.
    std::uint64_t videoRevision() const {return videoRevision_;}
    // Copy the video state at one emulation boundary. The frontend can
    // retain this value after the emulator updates VRAM again.
    VideoSnapshot captureVideo() const {
        VideoSnapshot snapshot{};
        std::copy_n(vram_.begin()+vramWindow_*kVramSize,kVramSize,snapshot.vram.begin());
        snapshot.flags=videoFlags_;
        snapshot.revision=videoRevision_;
        return snapshot;
    }
    // Diagnostic physical VRAM window selection, not an FM-11 I/O port.
    // Mapping of hardware bank control bits is not yet verified.
    std::size_t vramWindow() const {return vramWindow_;}
    void selectVramWindow(std::size_t window) {
        if(window>=kVramWindows)throw std::out_of_range("display VRAM window");
        if(vramWindow_!=window) {vramWindow_=window;markVideoChanged();}
    }
    BYTE readPhysicalVram(std::size_t offset) const {return vram_.at(offset);}
    const std::array<BYTE,kPhysicalVramSize>& physicalVram() const {return vram_;}
    // Handshake output to the main board. The physical sub-I/O trigger
    // remains unassigned until its register decode is confirmed.
    void setAttentionSink(std::function<void()> sink) {attentionSink_=std::move(sink);}
    void notifyMainAttention() {if(attentionSink_)attentionSink_();}
    // The top 128 bytes of sub RAM also appear at main $FC80-$FCFF.
    BYTE readShared(std::size_t offset) const {return ram_[0x1f80+offset];}
    void writeShared(std::size_t offset,BYTE value) {ram_[0x1f80+offset]=value;}
    BYTE readMemory(std::uint16_t address) const {
        if(address<0x8000)return vram_[vramWindow_*kVramSize+address];
        if(address<0xa000)return ram_[address-0x8000];
        if(address>=0xafe0 && address<=0xafe3)return subIoRam_[address-0xafe0];
        if(address==0xafe6)return videoFlags_;
        if(address==0xaff0)return busy_;
        if(address>=0xc000)return rom_[address-0xc000];
        return 0xff; // unmapped space, including unimplemented sub I/O
    }
    void writeMemory(std::uint16_t address,BYTE value) {
        if(address<0x8000) {
            const auto offset=vramWindow_*kVramSize+address;
            if(vram_[offset]!=value) {
                vram_[offset]=value;
                markVideoChanged();
            }
        }
        else if(address<0xa000)ram_[address-0x8000]=value;
        else if(address>=0xafe0 && address<=0xafe3)subIoRam_[address-0xafe0]=value;
        else if(address==0xafe6) {
            if(videoFlags_!=value) {
                videoFlags_=value;
                markVideoChanged();
            }
        }
        else if(address==0xaff0)busy_=value;
        // ROM and unmapped space are read-only. Trace even writes to
        // unimplemented I/O locations; their hardware decode is unknown.
        if(address>=0xa000 && address<0xc000 && ioWriteObserver_)
            ioWriteObserver_({address,value});
    }
    // Physical VRAM; captureVideo() returns the currently mapped 32 KiB.
    const std::array<BYTE,kPhysicalVramSize>& vram() const {return vram_;}

private:
    void markVideoChanged() {++videoRevision_;}
    std::uint64_t videoRevision_=0;
    IoWriteObserver ioWriteObserver_;
    std::function<void()> attentionSink_;
    bool haltRequested_=false;
    BYTE busy_=0x80;
    BYTE videoFlags_=0;
    std::array<BYTE,4> subIoRam_{};
    std::size_t vramWindow_=0;
    std::array<BYTE,kPhysicalVramSize> vram_{};
    std::array<BYTE,kRamSize> ram_{};
    RomImage rom_{};
};
