#pragma once
#include "portable_types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <utility>

// Main-board display/sub-CPU memory. I/O and display scanout are deliberately
// not decoded yet: the reference map still labels those regions tentative.
class DisplaySubsystem {
public:
    static constexpr std::size_t kVramSize=0x8000;
    static constexpr std::size_t kPhysicalVramSize=0x30000; // 192 KiB
    static constexpr std::size_t kVramWindows=kPhysicalVramSize/kVramSize;
    static constexpr std::size_t kRamSize=0x2000;
    static constexpr std::size_t kRomSize=0x5000;
    using RomImage=std::array<BYTE,kRomSize>;
    static constexpr std::size_t kAnkRomSize=0x1000;
    static constexpr std::size_t kKanjiRomSize=0x20000;
    using AnkRomImage=std::array<BYTE,kAnkRomSize>;
    using KanjiRomImage=std::array<BYTE,kKanjiRomSize>;
    struct IoWriteTrace {std::uint16_t address; BYTE value; std::uint16_t pc=0; bool fromCpu=false;};
    using IoWriteObserver=std::function<void(IoWriteTrace)>;
    struct VideoSnapshot {
        std::array<BYTE,kVramSize> vram;
        BYTE flags;
        std::uint64_t revision;
    };

    void reset() {
        haltRequested_=false;
        busy_=true;
        if(videoFlags_!=0)markVideoChanged();
        videoFlags_=0;
        vsyncCount_=0;
        vsyncMode_=0;
        vsyncStatus_=0x40;
        subIoRam_.fill(0);
        subFirqLatch_=keyboardStatusLatch_=keyboardDataLatch_=0;
        keyHead_=keyTail_=0;
        nmiMask_=true;
        dmaChannel0_=0;
        gvuEnabled_=false;
        gvuPage_=gvuPlane_=gvuOffset_=gvdPage_=0;
        gvuControl_=gvuAddressHigh_=0;
        displayAddress_=0;displayAddressHigh_=0;crtcIndex_=0;
        crtcRegisters_.fill(0);
        // ROM contents and display memory survive a CPU reset. Hardware
        // power-on clearing is a separate future operation.
    }
    void requestHalt() {haltRequested_=true;}
    void releaseHalt() {haltRequested_=false;}
    bool haltRequested() const {return haltRequested_;}

    void loadRom(const RomImage& image) {rom_=image;}
    void loadAnkRom(const AnkRomImage& image) {ankRom_=image;markVideoChanged();}
    void loadKanjiRom(const KanjiRomImage& image) {kanjiRom_=image;markVideoChanged();}
    BYTE ankGlyphRow(BYTE character,unsigned row) const {
        return ankRom_.at(std::size_t(character)*16+row);
    }
    BYTE kanjiRomByte(std::size_t offset) const {return kanjiRom_.at(offset);}
    BYTE textByte(std::size_t offset) const {return ram_.at(offset&0x0fffu);}
    // Diagnostic observer for sub-CPU writes to the undecoded A000-BFFF
    // aperture and known sub-I/O latches. Observation is non-invasive:
    // attaching an observer never changes readback or register side effects.
    void setIoWriteObserver(IoWriteObserver observer) {ioWriteObserver_=std::move(observer);}
    void clearIoWriteObserver() {ioWriteObserver_={};}
    void beginCpuInstruction(std::uint16_t pc) {tracePc_=pc;traceCpu_=true; if(vsyncCount_>0)--vsyncCount_;}
    void endCpuInstruction() {traceCpu_=false;}
    // Hardware-produced latches; CPU writes to these addresses do not set them.
    void signalSubFirq(BYTE status) {subFirqLatch_=status;}
    void setKeyboardLatches(BYTE status,BYTE data) {
        keyboardStatusLatch_=status;keyboardDataLatch_=data;
    }
    // Source SubCpu.cpp SetKeyCode/GetKeyCode: interrupt-mode key queue.
    void enqueueKeyCode(std::uint16_t code) {
        const auto next=(keyTail_+1)%keyQueue_.size();
        if(next!=keyHead_) {keyQueue_[keyTail_]=code;keyTail_=next;}
    }
    void processKeyCode() const {
        if(keyHead_==keyTail_ || (subFirqLatch_&1))return;
        const auto code=keyQueue_[keyHead_];
        keyboardStatusLatch_=static_cast<BYTE>(code>>8);
        keyboardDataLatch_=static_cast<BYTE>(code);
        if(!(code&0x0200))subFirqLatch_|=1;
        keyHead_=(keyHead_+1)%keyQueue_.size();
    }
    bool nmiMask() const {return nmiMask_;}
    bool subFirqPending() const {return subFirqLatch_!=0;}
    BYTE busyStatus() const {return busy_?0x80:0;}
    BYTE readSubControl() const {return static_cast<BYTE>((subIoRam_[0]&0xfeu)|(busy_?0u:1u));}
    std::size_t graphicsAccessPage() const {return gvuPage_;}
    std::size_t graphicsDisplayPage() const {return gvdPage_;}
    std::uint16_t graphicsOffset() const {return gvuOffset_;}
    std::size_t graphicsReadPlane() const {return gvuPlane_;}
    std::uint16_t graphicsDisplayAddress() const {return displayAddress_;}
    BYTE crtcRegister(unsigned index) const {return crtcRegisters_.at(index);}
    // AFE6 video-control flags. Individual bits are not yet mapped to
    // display modes until their FM-11 semantics have been verified.
    BYTE videoFlags() const {return videoFlags_;}
    // Reference SubCpu.cpp GetIoByte($AFE6): reading this register
    // updates bits 7 and 2, and forces bit 5 high. This is NOT an
    // independently sampled real-time vertical blank waveform.
    BYTE readVsyncStatus() {
        if(vsyncCount_>0) {
            vsyncCount_=0;
            vsyncStatus_&=static_cast<BYTE>(~0x84u);
        } else if(vsyncMode_!=0) {
            vsyncStatus_|=0x84;
        } else {
            vsyncStatus_^=0x84;
        }
        vsyncStatus_|=0x20;
        return vsyncStatus_;
    }
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
    void writeShared(std::size_t offset,BYTE value) {
        if(ram_.at(0x1f80+offset)!=value) {
            ram_[0x1f80+offset]=value;
            ++sharedWriteRevision_;
        }
    }
    std::uint64_t sharedWriteRevision() const {return sharedWriteRevision_;}
    BYTE readMemory(std::uint16_t address) const {
        if(address<0x8000) {
            if(!gvuEnabled_)return vram_[vramWindow_*kVramSize+address];
            return vram_[gvuPlane_*0x10000+graphicsAddress(address)];
        }
        if(address<0xa000)return ram_[address-0x8000];
        // Reference SubCpu.cpp: these I/O latches clear on read.
        if(address==0xafe4)return std::exchange(subFirqLatch_,BYTE{0});
        if(address==0xafec)return std::exchange(keyboardStatusLatch_,BYTE{0});
        if(address==0xafed) {
            const BYTE result=std::exchange(keyboardDataLatch_,BYTE{0});
            processKeyCode();
            return result;
        }
        if(address==0xafe0)return readSubControl();
        if(address>=0xafe1 && address<=0xafe3)return subIoRam_[address-0xafe0];
        if(address==0xafe8)return gvuAddressHigh_;
        if(address==0xafe9)return static_cast<BYTE>(gvuOffset_);
        if(address==0xafea)return displayAddressHigh_;
        if(address==0xafeb)return static_cast<BYTE>(displayAddress_);
        if(address==0xafee)return crtcIndex_;
        if(address==0xafef)return crtcIndex_<crtcRegisters_.size()?crtcRegisters_[crtcIndex_]:0xff;
        // Original SubCpu.cpp GetIoByte($AFE7): reading this register
        // asserts the main CPU's ATTENTION event (FD04 bit 0).
        // Keep the side effect on read, not write.
        if(address==0xafe7) {
            if(attentionSink_)attentionSink_();
            return 0;
        }
        if(address==0xafe6)return const_cast<DisplaySubsystem*>(this)->readVsyncStatus();
        if(address==0xaff0)return dmaChannel0_; // original write target is DMA channel 0
        if(address>=0xb000)return rom_[address-0xb000];
        return 0xff; // unmapped space, including unimplemented sub I/O
    }
    void writeMemory(std::uint16_t address,BYTE value) {
        if(address<0x8000) {
            if(gvuEnabled_) {
                const auto offset=graphicsAddress(address);
                for(unsigned plane=0;plane<3;++plane) {
                    const unsigned writeMask=1u<<plane;
                    const unsigned preserveMask=1u<<(plane+3);
                    BYTE next=vram_[plane*0x10000+offset];
                    if(gvuControl_&writeMask)next=value;
                    else if(!(gvuControl_&preserveMask))next=0;
                    if(next!=vram_[plane*0x10000+offset]) {
                        vram_[plane*0x10000+offset]=next;
                        markVideoChanged();
                    }
                }
            } else {
                const auto offset=vramWindow_*kVramSize+address;
                if(vram_[offset]!=value) {
                    vram_[offset]=value;
                    markVideoChanged();
                }
            }
        }
        else if(address<0xa000)ram_[address-0x8000]=value;
        else if(address>=0xafe0 && address<=0xafe3) {
            subIoRam_[address-0xafe0]=value;
            if(address==0xafe0) {
                if(nmiMask_) {
                    enqueueKeyCode(0x0050);
                    nmiMask_=false;
                }
                busy_=(value&1u)==0;
            }
            if(address==0xafe1) {
                gvuControl_=value;
                gvuPlane_=std::min<unsigned>(value>>6,2);
                gvuEnabled_=true;
            } else if(address==0xafe2) {
                const unsigned page=(value>>3)&3u;
                if(gvdPage_!=page) {
                    gvdPage_=page;
                    markVideoChanged();
                }
            } else if(address==0xafe3) {
                gvuPage_=(value>>3)&3;
            }
        }
        else if(address==0xafe8)gvuAddressHigh_=value;
        else if(address==0xafe9)gvuOffset_=static_cast<std::uint16_t>(
            ((unsigned(gvuAddressHigh_)<<8)|value)&0x7ffe);
        else if(address==0xafea)displayAddressHigh_=value;
        else if(address==0xafeb) {
            const auto next=static_cast<std::uint16_t>(
                ((unsigned(displayAddressHigh_)<<8)|value)&0x7fff);
            if(displayAddress_!=next) {displayAddress_=next;markVideoChanged();}
        }
        else if(address==0xafee)crtcIndex_=value&0x1f;
        else if(address==0xafef) {
            if(crtcIndex_<crtcRegisters_.size() &&
               crtcRegisters_[crtcIndex_]!=value) {
                crtcRegisters_[crtcIndex_]=value;
                if(crtcIndex_==13)vsyncCount_=100;
                markVideoChanged();
            }
        }
        else if(address==0xafe6) {
            vsyncStatus_=value;
            if(videoFlags_!=value) {
                videoFlags_=value;
                markVideoChanged();
            }
        }
        else if(address==0xaff0)dmaChannel0_=value; // original maps to DMA channel 0
        // ROM and unmapped space are read-only. Trace even writes to
        // unimplemented I/O locations; their hardware decode is unknown.
        if(address>=0xa000 && address<0xc000 && ioWriteObserver_)
            ioWriteObserver_({address,value,tracePc_,traceCpu_});
    }
    // Physical VRAM; captureVideo() returns the currently mapped 32 KiB.
    const std::array<BYTE,kPhysicalVramSize>& vram() const {return vram_;}

private:
    std::size_t graphicsAddress(std::uint16_t address) const {
        return (gvuPage_*0x4000+((gvuOffset_+address)&0x7fff))&0xffff;
    }
    void markVideoChanged() {++videoRevision_;}
    std::uint64_t videoRevision_=0;
    std::uint64_t sharedWriteRevision_=0;
    IoWriteObserver ioWriteObserver_;
    std::uint16_t tracePc_=0;
    bool traceCpu_=false;
    std::function<void()> attentionSink_;
    bool haltRequested_=false;
    bool busy_=true;
    BYTE dmaChannel0_=0;
    bool nmiMask_=true;
    std::array<std::uint16_t,256> keyQueue_{};
    mutable std::size_t keyHead_=0,keyTail_=0;
    mutable BYTE subFirqLatch_=0,keyboardStatusLatch_=0,keyboardDataLatch_=0;
    BYTE videoFlags_=0;
    unsigned vsyncCount_=0;
    BYTE vsyncMode_=0,vsyncStatus_=0x40;
    std::array<BYTE,4> subIoRam_{};
    std::size_t vramWindow_=0;
    bool gvuEnabled_=false;
    BYTE gvuControl_=0,gvuAddressHigh_=0;
    unsigned gvuPage_=0,gvuPlane_=0,gvdPage_=0;
    std::uint16_t gvuOffset_=0;
    BYTE displayAddressHigh_=0,crtcIndex_=0;
    std::uint16_t displayAddress_=0;
    std::array<BYTE,17> crtcRegisters_{};
    std::array<BYTE,kPhysicalVramSize> vram_{};
    std::array<BYTE,kRamSize> ram_{};
    RomImage rom_{};
    AnkRomImage ankRom_{};
    KanjiRomImage kanjiRom_{};
};
