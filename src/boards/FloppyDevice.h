#pragma once
#include "portable_types.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

// System-side floppy media and sector storage, independent of the CPU/DMAC.
// The CPU boards retain only their FDC register and DMA-adapter behavior.
class FloppyDevice {
public:
    void load2d(const std::vector<BYTE>& raw) {
        if(raw.size()!=40u*2*16*256)
            throw std::invalid_argument("FM-11 2D raw image must be 327680 bytes");
        raw2d_=raw;
        d88_.clear();
        offsets_.fill(0);
        dirty2d_=false;
    }
    void load2hd(const std::vector<BYTE>& raw) {
        if(raw.size()!=77u*2*8*1024)
            throw std::invalid_argument("FM-11 2HD raw image must be 1261568 bytes");
        raw2hd_=raw;
        dirty2hd_=false;
    }
    void loadD88(const std::vector<BYTE>& d88)
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
    load2d(raw);
    d88_=d88;
    offsets_=offsets;
}


    bool sectorOffset(unsigned channel,unsigned track,unsigned side,unsigned sector,
                      unsigned drive,std::size_t& offset) const {
        if(drive!=0)return false;
        if(channel==0) {
            if(raw2d_.empty() || track>=40 || sector<1 || sector>16)return false;
            offset=((track*2+(side&1))*16+(sector-1))*256;
        } else if(channel==1) {
            if(raw2hd_.empty() || track>=77 || sector<1 || sector>8)return false;
            offset=((track*2+(side&1))*8+(sector-1))*1024;
        } else return false;
        return true;
    }
    std::size_t sectorLength(unsigned channel) const {return channel==0?256:1024;}
    BYTE read(unsigned channel,std::size_t offset) const {
        return (channel==0?raw2d_:raw2hd_).at(offset);
    }
    void write(unsigned channel,std::size_t offset,BYTE value) {
        auto& image=channel==0?raw2d_:raw2hd_;
        image.at(offset)=value;
        if(channel==0) {
            dirty2d_=true;
            if(!d88_.empty())d88_.at(offsets_.at(offset/256)+(offset%256))=value;
        } else dirty2hd_=true;
    }
    void writeSector(unsigned channel,std::size_t offset,const BYTE* bytes) {
        for(std::size_t i=0;i<sectorLength(channel);++i)write(channel,offset+i,bytes[i]);
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
                transferBuffer_[i]=read(channel,transferOffset_+i);
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
            writeSector(transferChannel_,transferOffset_,transferBuffer_.data());
            transferWriting_=false;
            status_=0;
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
    const std::vector<BYTE>& raw2d() const {return raw2d_;}
    const std::vector<BYTE>& raw2hd() const {return raw2hd_;}
    std::vector<BYTE> image2d() const {return d88_.empty()?raw2d_:d88_;}
    std::vector<BYTE> image2hd() const {return raw2hd_;}
    bool dirty2d() const {return dirty2d_;}
    bool dirty2hd() const {return dirty2hd_;}
    void clearDirty2d() {dirty2d_=false;}
    void clearDirty2hd() {dirty2hd_=false;}
    bool has2d() const {return !raw2d_.empty();}
    bool has2hd() const {return !raw2hd_.empty();}
private:
    std::vector<BYTE> raw2d_,raw2hd_,d88_;
    std::array<BYTE,256> registers_{};
    std::array<std::size_t,40*2*16> offsets_{};
    bool dirty2d_=false,dirty2hd_=false;
    std::array<BYTE,1024> transferBuffer_{};
    std::size_t transferOffset_=0,transferPosition_=256,transferLength_=256;
    unsigned transferChannel_=0;
    bool transferWriting_=false;
    BYTE status_=0;
};
