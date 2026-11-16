#pragma once
#include "portable_types.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

// System-side floppy media and sector storage, independent of the CPU/DMAC.
// The CPU boards retain only their FDC register and DMA-adapter behavior.
class FloppyDevice {
public:
    // The controller drive-select bits address independent inserted media.
    void setMountedDrive(unsigned drive) {
        if(drive>3)throw std::invalid_argument("FDC drive must be 0..3");
        mountedDrive_=drive;
    }
    unsigned mountedDrive() const {return mountedDrive_;}
    bool present(unsigned drive) const {return drive<4 && !media2hd_[drive].raw.empty();}
    bool has2d(unsigned drive=0) const {return drive<4 && !media2d_[drive].raw.empty();}
    bool has2hd(unsigned drive=0) const {return drive<4 && !media2hd_[drive].raw.empty();}
    bool hasAny2d() const {
        for(const auto& disk:media2d_)if(!disk.raw.empty())return true;
        return false;
    }
    bool hasAny2hd() const {
        for(const auto& disk:media2hd_)if(!disk.raw.empty())return true;
        return false;
    }
    void load2d(const std::vector<BYTE>& raw,unsigned drive=0) {
        if(drive>3)throw std::invalid_argument("2D drive must be 0..3");
        if(raw.size()!=40u*2*16*256)
            throw std::invalid_argument("FM-11 2D raw image must be 327680 bytes");
        auto& disk=media2d_[drive];
        disk.raw=raw;
        disk.d88.clear();
        disk.offsets.fill(0);
        disk.dirty=false;
    }
    void load2hd(const std::vector<BYTE>& raw,unsigned drive=0) {
        if(drive>3)throw std::invalid_argument("2HD drive must be 0..3");
        if(raw.size()!=77u*2*8*1024)
            throw std::invalid_argument("FM-11 2HD raw image must be 1261568 bytes");
        auto& disk=media2hd_[drive];
        disk.raw=raw;
        disk.dirty=false;
    }
    void loadD88(const std::vector<BYTE>& d88,unsigned drive=0)
{
    // D88: 0x2B0-byte disk header; 164 little-endian track offsets.
    // Sector headers are 16 bytes and carry their own C/H/R/N ID.
    constexpr std::size_t headerSize = 0x2b0;
    constexpr std::size_t rawSize = 40u * 2 * 16 * 256;
    if (d88.size() < headerSize)
        throw std::invalid_argument("D88 header is truncated");
    auto le16 = [&](std::size_t offset) -> unsigned {
        return unsigned(d88[offset]) | (unsigned(d88[offset+1]) << 8);
    };
    auto le32 = [&](std::size_t offset) -> std::uint32_t {
        return std::uint32_t(d88[offset]) |
               (std::uint32_t(d88[offset+1]) << 8) |
               (std::uint32_t(d88[offset+2]) << 16) |
               (std::uint32_t(d88[offset+3]) << 24);
    };
    if (le32(0x1c) != d88.size())
        throw std::invalid_argument("D88 disk size field does not match file");
    if (d88[0x1b] != 0x00)
        throw std::invalid_argument("D88 is not a 2D disk image");
    std::vector<BYTE> raw(rawSize);
    std::array<bool,40*2*16> seen{};
    std::array<std::size_t,40*2*16> offsets{};
    for (unsigned trackIndex=0; trackIndex<80; ++trackIndex) {
        const auto start=le32(0x20+trackIndex*4);
        if (!start)
            continue;
        if (start < headerSize || start > d88.size()-16)
            throw std::invalid_argument("D88 track offset out of range");
        std::size_t cursor=start;
        const auto sectors=le16(cursor+4);
        if (sectors==0 || sectors>256)
            throw std::invalid_argument("D88 sector count is invalid");
        for (unsigned i=0;i<sectors;++i) {
            if (cursor > d88.size()-16)
                throw std::invalid_argument("D88 sector header is truncated");
            const unsigned c=d88[cursor], side=d88[cursor+1];
            const unsigned sector=d88[cursor+2], n=d88[cursor+3];
            const unsigned count=le16(cursor+4);
            const unsigned length=le16(cursor+14);
            if (count!=sectors || c!=trackIndex/2 ||
                side!=trackIndex%2 || sector<1 || sector>16 ||
                n!=1 || length!=256 || d88[cursor+8]!=0)
                throw std::invalid_argument("D88 sector layout is not FM-11 2D");
            cursor+=16;
            if (length>d88.size()-cursor)
                throw std::invalid_argument("D88 sector data is truncated");
            const auto idx=(c*2+side)*16+(sector-1);
            if (seen[idx])
                throw std::invalid_argument("D88 has duplicate sector IDs");
            seen[idx]=true;
            offsets[idx]=cursor;
            std::copy_n(d88.begin()+cursor,length,raw.begin()+idx*256);
            cursor+=length;
        }
    }
    for (bool present:seen)
        if (!present)
            throw std::invalid_argument("D88 is missing an FM-11 2D sector");
    load2d(raw,drive);
    media2d_[drive].d88=d88;
    media2d_[drive].offsets=offsets;
}


    bool sectorOffset(unsigned channel,unsigned track,unsigned side,unsigned sector,
                      unsigned drive,std::size_t& offset) const {
        if(drive>=4)return false;
        if(channel==0) {
            if(!has2d(drive) || track>=40 || sector<1 || sector>16)return false;
            offset=((track*2+(side&1))*16+(sector-1))*256;
        } else if(channel==1) {
            if(!has2hd(drive) || track>=77 || sector<1 || sector>8)return false;
            offset=((track*2+(side&1))*8+(sector-1))*1024;
        } else return false;
        return true;
    }
    std::size_t sectorLength(unsigned channel) const {return channel==0?256:1024;}
    BYTE read(unsigned channel,std::size_t offset,unsigned drive=0) const {
        return medium(channel,drive).raw.at(offset);
    }
    void write(unsigned channel,std::size_t offset,BYTE value,unsigned drive=0) {
        auto& disk=medium(channel,drive);
        disk.raw.at(offset)=value;
        disk.dirty=true;
        if(channel==0 && !disk.d88.empty())
            disk.d88.at(disk.offsets.at(offset/256)+(offset%256))=value;
    }
    // Callback persists one complete sector to the selected host image.
    // Both CPU boards, including their PIO and DMA paths, use it.
    using SectorWriter=std::function<bool(unsigned,unsigned,std::size_t,const BYTE*,std::size_t)>;
    void setSectorWriter(SectorWriter writer) {sectorWriter_=std::move(writer);}
    bool writeSector(unsigned channel,std::size_t offset,const BYTE* bytes,unsigned drive=0) {
        const auto length=sectorLength(channel);
        if(sectorWriter_ && !sectorWriter_(channel,drive,offset,bytes,length)) {
            status_=0x10;
            return false;
        }
        for(std::size_t i=0;i<length;++i)write(channel,offset+i,bytes[i],drive);
        return true;
    }
    std::size_t hostImageOffset(unsigned channel,std::size_t rawOffset,unsigned drive=0) const {
        const auto& disk=medium(channel,drive);
        if(channel==0 && !disk.d88.empty())
            return disk.offsets.at(rawOffset/256)+(rawOffset%256);
        return rawOffset;
    }
    // FDC control/data registers are system-side, not CPU-card local.
    // Command and data ports are dispatched separately by DMA adapters.
    BYTE registerValue(unsigned reg) const {return registers_.at(reg);}
    void writeRegister(unsigned reg,BYTE value) {registers_.at(reg)=value;}
    void resetRegisters() {registers_.fill(0);}
    enum class Command : BYTE { Other, ReadSector, WriteSector };
    static Command decodeCommand(BYTE value) {
        switch(value&0xf0) {
        case 0x80: return Command::ReadSector;
        case 0xa0: return Command::WriteSector;
        default: return Command::Other;
        }
    }
    // Command decoding is independent of the processor and its DMA chip.
    // Reset/seek commands still need their own controller implementation.
    // Shared FDC transfer FIFO. CPU-specific DMA controllers only move bytes
    // through this interface; the sector buffer belongs to the system device.
    bool beginSector(unsigned channel,unsigned track,unsigned side,unsigned sector,
                     unsigned drive,bool writing) {
        transferChannel_=channel;
        transferDrive_=drive;
        transferLength_=sectorLength(channel);
        transferPosition_=transferLength_;
        transferWriting_=false;
        status_=0;
        if(!sectorOffset(channel,track,side,sector,drive,transferOffset_)) {
            status_=0x10;
            return false;
        }
        transferPosition_=0;
        transferWriting_=writing;
        if(!writing)
            for(std::size_t i=0;i<transferLength_;++i)
                transferBuffer_[i]=read(channel,transferOffset_+i,drive);
        status_=0x02;
        return true;
    }
    BYTE transferRead() {
        if(transferWriting_ || transferPosition_>=transferLength_)return 0xff;
        const BYTE result=transferBuffer_[transferPosition_++];
        if(transferPosition_==transferLength_)status_=0;
        return result;
    }
    void transferWrite(BYTE value) {
        if(!transferWriting_ || transferPosition_>=transferLength_)return;
        transferBuffer_[transferPosition_++]=value;
        if(transferPosition_==transferLength_) {
            const bool written=writeSector(transferChannel_,transferOffset_,transferBuffer_.data(),transferDrive_);
            transferWriting_=false;
            status_=written?0:0x10;
        }
    }
    void finishTransfer() {
        transferPosition_=transferLength_;
        transferWriting_=false;
        status_=0;
    }
    void transferError() {status_=0x10;}
    BYTE transferStatus() const {return status_;}
    std::size_t transferPosition() const {return transferPosition_;}
    std::size_t transferLength() const {return transferLength_;}
    unsigned transferChannel() const {return transferChannel_;}
    const std::vector<BYTE>& raw2d(unsigned drive=0) const {return media2d_.at(drive).raw;}
    const std::vector<BYTE>& raw2hd(unsigned drive=0) const {return media2hd_.at(drive).raw;}
    std::vector<BYTE> image2d(unsigned drive=0) const {
        const auto& disk=media2d_.at(drive);
        return disk.d88.empty()?disk.raw:disk.d88;
    }
    std::vector<BYTE> image2hd(unsigned drive=0) const {return media2hd_.at(drive).raw;}
    bool dirty2d(unsigned drive=0) const {return media2d_.at(drive).dirty;}
    bool dirty2hd(unsigned drive=0) const {return media2hd_.at(drive).dirty;}
    void clearDirty2d(unsigned drive=0) {media2d_.at(drive).dirty=false;}
    void clearDirty2hd(unsigned drive=0) {media2hd_.at(drive).dirty=false;}
private:
    struct Disk {
        std::vector<BYTE> raw,d88;
        std::array<std::size_t,40*2*16> offsets{};
        bool dirty=false;
    };
    Disk& medium(unsigned channel,unsigned drive) {
        return channel==0?media2d_.at(drive):media2hd_.at(drive);
    }
    const Disk& medium(unsigned channel,unsigned drive) const {
        return channel==0?media2d_.at(drive):media2hd_.at(drive);
    }
    SectorWriter sectorWriter_;
    unsigned mountedDrive_=0;
    std::array<Disk,4> media2d_{},media2hd_{};
    std::array<BYTE,256> registers_{};
    std::array<std::size_t,40*2*16> offsets_{};
    bool dirty2d_=false,dirty2hd_=false;
    std::array<BYTE,1024> transferBuffer_{};
    std::size_t transferOffset_=0,transferPosition_=256,transferLength_=256;
    unsigned transferChannel_=0,transferDrive_=0;
    bool transferWriting_=false;
    BYTE status_=0;
};
