#pragma once
#include "portable_types.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
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
        disk.d88.clear();
        disk.offsets.fill(0);
        disk.dirty=false;
    }
    void loadD88(const std::vector<BYTE>& d88,unsigned drive=0) {
        loadD88Medium(d88,drive,false);
    }
    void loadD882hd(const std::vector<BYTE>& d88,unsigned drive=0) {
        loadD88Medium(d88,drive,true);
    }
    void loadD88Medium(const std::vector<BYTE>& d88,unsigned drive,bool is2hd) {
        if(drive>3)throw std::invalid_argument("D88 drive must be 0..3");
        // D88 header: 0x2B0 bytes, 164 little-endian track offsets.
        constexpr std::size_t headerSize=0x2b0;
        const unsigned tracks=is2hd?77:40;
        const unsigned sectorsPerTrack=is2hd?8:16;
        const unsigned sectorSize=is2hd?1024:256;
        const unsigned sectorN=is2hd?3:1;
        const BYTE diskType=is2hd?0x20:0x00;
        if(d88.size()<headerSize)
            throw std::invalid_argument("D88 header is truncated");
        auto le16=[&](std::size_t offset)->unsigned {
            return unsigned(d88[offset])|(unsigned(d88[offset+1])<<8);
        };
        auto le32=[&](std::size_t offset)->std::uint32_t {
            return std::uint32_t(d88[offset])|
                   (std::uint32_t(d88[offset+1])<<8)|
                   (std::uint32_t(d88[offset+2])<<16)|
                   (std::uint32_t(d88[offset+3])<<24);
        };
        if(le32(0x1c)!=d88.size())
            throw std::invalid_argument("D88 disk size field does not match file");
        if(d88[0x1b]!=diskType)
            throw std::invalid_argument(is2hd?"D88 is not a 2HD disk image":
                                             "D88 is not a 2D disk image");
        std::vector<BYTE> raw(std::size_t(tracks)*2*sectorsPerTrack*sectorSize);
        std::vector<bool> seen(std::size_t(tracks)*2*sectorsPerTrack);
        std::vector<std::size_t> offsets(seen.size());
        for(unsigned trackIndex=0;trackIndex<tracks*2;++trackIndex) {
            const auto trackStart=le32(0x20+trackIndex*4);
            if(!trackStart)continue;
            if(trackStart<headerSize || trackStart>d88.size()-16)
                throw std::invalid_argument("D88 track offset out of range");
            std::size_t cursor=trackStart;
            const auto sectors=le16(cursor+4);
            if(sectors==0 || sectors>256)
                throw std::invalid_argument("D88 sector count is invalid");
            for(unsigned i=0;i<sectors;++i) {
                if(cursor>d88.size()-16)
                    throw std::invalid_argument("D88 sector header is truncated");
                const unsigned c=d88[cursor],side=d88[cursor+1];
                const unsigned sector=d88[cursor+2],n=d88[cursor+3];
                const unsigned count=le16(cursor+4),length=le16(cursor+14);
                if(count!=sectors || c!=trackIndex/2 ||
                   side!=trackIndex%2 || sector<1 || sector>sectorsPerTrack ||
                   n!=sectorN || length!=sectorSize || d88[cursor+8]!=0)
                    throw std::invalid_argument(is2hd?
                        "D88 sector layout is not FM-11 2HD":
                        "D88 sector layout is not FM-11 2D");
                cursor+=16;
                if(length>d88.size()-cursor)
                    throw std::invalid_argument("D88 sector data is truncated");
                const auto idx=(std::size_t(c)*2+side)*sectorsPerTrack+(sector-1);
                if(seen[idx])throw std::invalid_argument("D88 has duplicate sector IDs");
                seen[idx]=true;
                offsets[idx]=cursor;
                std::copy_n(d88.begin()+cursor,length,raw.begin()+idx*sectorSize);
                cursor+=length;
            }
        }
        for(bool present:seen)
            if(!present)
                throw std::invalid_argument(is2hd?
                    "D88 is missing an FM-11 2HD sector":
                    "D88 is missing an FM-11 2D sector");
        if(is2hd)load2hd(raw,drive);
        else load2d(raw,drive);
        auto& disk=is2hd?media2hd_[drive]:media2d_[drive];
        disk.d88=d88;
        std::copy(offsets.begin(),offsets.end(),disk.offsets.begin());
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
        if(!disk.d88.empty())
            disk.d88.at(disk.offsets.at(offset/sectorLength(channel))+(offset%sectorLength(channel)))=value;
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
        if(!disk.d88.empty())
            return disk.offsets.at(rawOffset/sectorLength(channel))+(rawOffset%sectorLength(channel));
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
    std::vector<BYTE> image2hd(unsigned drive=0) const {
        const auto& disk=media2hd_.at(drive);
        return disk.d88.empty()?disk.raw:disk.d88;
    }
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
    std::array<BYTE,1024> transferBuffer_{};
    std::size_t transferOffset_=0,transferPosition_=256,transferLength_=256;
    unsigned transferChannel_=0,transferDrive_=0;
    bool transferWriting_=false;
    BYTE status_=0;
};
