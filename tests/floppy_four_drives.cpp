#include "FloppyDevice.h"
#include <array>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <stdexcept>


static std::vector<BYTE> make2hdD88() {
    // Put sectors in reverse order to verify CHRN translation, not file order.
    std::vector<BYTE> bytes(0x2b0,0);
    bytes[0x1b]=0x20; // 2HD
    const auto le16=[&](std::size_t pos,unsigned v) {
        bytes[pos]=BYTE(v);bytes[pos+1]=BYTE(v>>8);
    };
    const auto le32=[&](std::size_t pos,std::uint32_t v) {
        for(unsigned n=0;n<4;++n)bytes[pos+n]=BYTE(v>>(8*n));
    };
    for(unsigned track=0;track<154;++track) {
        const auto start=bytes.size();
        le32(0x20+track*4,std::uint32_t(start));
        for(unsigned sector=8;sector>=1;--sector) {
            const auto hdr=bytes.size();
            bytes.resize(hdr+16,0);
            bytes[hdr]=BYTE(track/2);
            bytes[hdr+1]=BYTE(track%2);
            bytes[hdr+2]=BYTE(sector);
            bytes[hdr+3]=3; // 1024-byte record
            le16(hdr+4,8);
            le16(hdr+14,1024);
            bytes.insert(bytes.end(),1024,BYTE((track+sector)&0xff));
        }
    }
    le32(0x1c,std::uint32_t(bytes.size()));
    return bytes;
}

int main() {
    FloppyDevice floppy;
    constexpr std::size_t size2d=40u*2*16*256;
    constexpr std::size_t size2hd=77u*2*8*1024;
    unsigned writes=0;
    unsigned lastChannel=99,lastDrive=99;
    floppy.setSectorWriter([&](unsigned channel,unsigned drive,std::size_t,
                                const BYTE*,std::size_t size) {
        if(size!=(channel==0?256u:1024u))return false;
        ++writes;
        lastChannel=channel;
        lastDrive=drive;
        return true;
    });
    for(unsigned d=0;d<4;++d) {
        floppy.load2d(std::vector<BYTE>(size2d,BYTE(0x10+d)),d);
        floppy.load2hd(std::vector<BYTE>(size2hd,BYTE(0x20+d)),d);
    }
    for(unsigned channel=0;channel<2;++channel) {
        for(unsigned d=0;d<4;++d) {
            std::size_t offset=~std::size_t(0);
            if(!floppy.sectorOffset(channel,0,0,1,d,offset) || offset!=0)return 1;
            if(floppy.read(channel,0,d)!=BYTE((channel==0?0x10:0x20)+d))return 2;
            if(!floppy.beginSector(channel,0,0,1,d,true))return 3;
            const unsigned length=channel==0?256u:1024u;
            for(unsigned n=0;n<length;++n)floppy.transferWrite(BYTE(0x80+d));
            if(floppy.transferStatus()!=0 || floppy.read(channel,0,d)!=BYTE(0x80+d))
                return 4;
            if(lastChannel!=channel || lastDrive!=d || writes!=channel*4+d+1)return 5;
            for(unsigned other=0;other<4;++other) {
                if(other==d)continue;
                if(floppy.read(channel,0,other)==BYTE(0x80+d))return 6;
            }
        }
    }
    // Media must remain independent across CPU-board switching and reset.
    if(!floppy.dirty2d(3) || !floppy.dirty2hd(2))return 7;
    floppy.resetRegisters();
    if(floppy.read(0,0,3)!=0x83 || floppy.read(1,0,2)!=0x82)return 8;

    // D88 is accepted in every 2HD slot and retains the original container
    // layout. The sector callback must point into the D88 data, not the
    // contiguous RAW sector offset.
    const auto d88=make2hdD88();
    for(unsigned drive=0;drive<4;++drive) {
        floppy.loadD882hd(d88,drive);
        if(floppy.image2hd(drive)!=d88)return 9;
        // CHS 0/0/1 is the last physical record in the first D88 track.
        const auto rawOffset=std::size_t(0);
        const auto hostOffset=floppy.hostImageOffset(1,rawOffset,drive);
        if(hostOffset!=0x2b0+7*(16+1024)+16)return 10;
        if(floppy.read(1,rawOffset,drive)!=1)return 11;
        if(!floppy.beginSector(1,0,0,1,drive,true))return 12;
        for(unsigned n=0;n<1024;++n)floppy.transferWrite(BYTE(0xa0+drive));
        if(floppy.transferStatus()!=0 || lastChannel!=1 ||
           lastDrive!=drive)return 13;
        const auto updated=floppy.image2hd(drive);
        if(updated.size()!=d88.size() ||
           updated[hostOffset]!=BYTE(0xa0+drive) ||
           updated[hostOffset+1023]!=BYTE(0xa0+drive) ||
           updated[hostOffset-1]!=d88[hostOffset-1])return 14;
    }
    // Reject a 2D container in the 2HD channel, and malformed CHRN.
    try {floppy.loadD882hd(std::vector<BYTE>(0x2b0,0));return 15;}
    catch(const std::invalid_argument&) {}
    auto malformed=d88;
    malformed[0x2b0+3]=1; // N=1 instead of 3
    try {floppy.loadD882hd(malformed);return 16;}
    catch(const std::invalid_argument&) {}
    std::puts("PASS: four 2D/2HD drives, 2HD D88 CHRN and write offsets");
    return 0;
}
