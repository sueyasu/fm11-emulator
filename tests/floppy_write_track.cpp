#include "FloppyDevice.h"
#include "Main8088Bus.h"
#include "Main6809Board.h"
#include <memory>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cstdio>

static std::vector<BYTE> stream(unsigned track,unsigned side,unsigned count,
                                unsigned size,unsigned transferLength) {
    std::vector<BYTE> v;
    for(unsigned id=1;id<=count;++id) {
        v.insert(v.end(),12,0);
        v.insert(v.end(),{0xf5,0xf5,0xf5,0xfe,
                          BYTE(track),BYTE(side),BYTE(id),
                          BYTE(size==128?0:size==256?1:3),0xf7});
        v.insert(v.end(),12,0);
        v.insert(v.end(),{0xf5,0xf5,0xf5,0xfb});
        v.insert(v.end(),size,BYTE(id+track));
        v.push_back(0xf7);
        v.insert(v.end(),8,0x4e);
    }
    if(v.size()>transferLength)throw std::logic_error("track stream too large");
    v.resize(transferLength,0x4e);
    return v;
}
static void put32(std::vector<BYTE>& v,std::size_t off,std::uint32_t n) {
    for(unsigned i=0;i<4;++i)v[off+i]=BYTE(n>>(8*i));
}
static std::vector<BYTE> d88(bool high) {
    const unsigned tracks=high?154:80, count=high?26:16;
    std::vector<BYTE> b(0x2b0,0);
    b[0x1b]=BYTE(high?0x20:0x10);
    for(unsigned t=0;t<tracks;++t) {
        put32(b,0x20+t*4,std::uint32_t(b.size()));
        const unsigned length=high && t==0?128:256;
        for(unsigned id=1;id<=count;++id) {
            const auto h=b.size();
            b.resize(h+16,0);
            b[h]=BYTE(t/2);b[h+1]=BYTE(t%2);
            b[h+2]=BYTE(id);b[h+3]=BYTE(length==128?0:1);
            b[h+4]=BYTE(count);
            b[h+6]=BYTE(high && t==0?0x40:0);
            b[h+14]=BYTE(length);
            b[h+15]=BYTE(length>>8);
            b.insert(b.end(),length,0xe5);
        }
    }
    put32(b,0x1c,std::uint32_t(b.size()));
    return b;
}
int main() {
    FloppyDevice floppy;
    unsigned persist=0;
    std::vector<BYTE> persisted;
    floppy.setImageWriter([&](unsigned,unsigned,const std::vector<BYTE>& img) {
        ++persist;persisted=img;return true;
    });
    for(unsigned ch=0;ch<2;++ch) {
        const auto img=d88(ch==1);
        for(unsigned drive=0;drive<4;++drive)
            if(ch)floppy.loadD882hd(img,drive);
            else floppy.loadD88(img,drive);
        const unsigned count=ch?26:16;
        const unsigned full=ch?10900:6400;
        const unsigned size=ch?128:256;
        auto fmt=stream(0,0,count,size,full);
        if(!floppy.formatTrack(ch,0,0,3,fmt))return 1;
        if(persisted!=(ch?floppy.image2hd(3):floppy.image2d(3)))return 2;
        if((ch?floppy.image2hd(2):floppy.image2d(2))!=img)return 3;
        std::size_t offset=0;
        if(!floppy.sectorOffset(ch,0,0,5,3,offset))return 4;
        if(floppy.read(ch,offset,3)!=5 ||
           floppy.sectorLength(ch,0,0,5,3)!=size)return 5;
        auto malformed=fmt;
        // Break the first valid address mark: no partial track replacement.
        for(std::size_t k=0;k<malformed.size();++k)
            if(malformed[k]==0xfe){malformed[k+3]=0;break;}
        const auto before=ch?floppy.image2hd(3):floppy.image2d(3);
        if(floppy.formatTrack(ch,0,0,3,malformed))return 6;
        if((ch?floppy.image2hd(3):floppy.image2d(3))!=before)return 7;
        // The following MFM side stays 256 bytes (2HD boot is FM only).
        if(ch) {
            auto normal=stream(0,1,26,256,10900);
            if(!floppy.formatTrack(1,0,1,3,normal))return 8;
            if(floppy.sectorLength(1,0,1,1,3)!=256)return 9;
            const auto formatted=floppy.image2hd(3);
            FloppyDevice reload;
            reload.loadD882hd(formatted,3);
            std::size_t sideOffset=0;
            if(!reload.sectorOffset(1,0,1,26,3,sideOffset) ||
               reload.read(1,sideOffset,3)!=26)return 10;
        }
    }
    if(persist!=3)return 11;

    // Exercise both real 8088 FDC command ports and 8237 channels,
    // not just the shared track parser.
    Main8088Bus bus;
    auto fd=std::make_shared<FloppyDevice>();
    fd->load2d(std::vector<BYTE>(40u*2*16*256,0xe5),1);
    fd->load2hd(std::vector<BYTE>(77u*2*26*256,0xe5),2);
    bus.setFloppyDevice(fd);
    for(unsigned channel=0;channel<2;++channel) {
        const unsigned len=channel?10900:6400;
        const unsigned address=channel?0x3000:0x1000;
        const unsigned drive=channel?2:1;
        const auto bytes=stream(0,channel?1:0,channel?26:16,256,len);
        for(unsigned i=0;i<len;++i)bus.writeMemory(address+i,bytes[i]);
        bus.writeIo(0x1c,0);   // 8237 base-address flip-flop
        const WORD addressPort=WORD(0x10+2*channel);
        bus.writeIo(addressPort,BYTE(address));
        bus.writeIo(addressPort,BYTE(address>>8));
        bus.writeIo(WORD(addressPort+1),BYTE(len-1));
        bus.writeIo(WORD(addressPort+1),BYTE((len-1)>>8));
        bus.writeIo(WORD(0x20+channel),0);
        bus.writeIo(0x1b,BYTE(0x48+channel));  // single, memory->FDC
        bus.writeIo(0x1a,BYTE(channel));       // unmask selected channel
        bus.writeIo(WORD(channel?0xfd31:0xfd19),0);
        bus.writeIo(WORD(channel?0xfd34:0xfd1c),BYTE(channel?1:0));
        bus.writeIo(WORD(channel?0xfd35:0xfd1d),BYTE(drive));
        bus.writeIo(WORD(channel?0xfd30:0xfd18),0xf0);
        if(bus.readIo(WORD(channel?0xfd30:0xfd18))!=0)return 12;
        if(!(bus.readIo(0x18)&(1u<<channel)))return 13;
        std::size_t offset=0;
        if(!fd->sectorOffset(channel,0,channel?1:0,16,drive,offset) ||
           fd->read(channel,offset,drive)!=16)return 14;
    }
    // 6809/6309 card uses the original 6844 DMA register bank.
    auto fmDisk=std::make_shared<FloppyDevice>();
    fmDisk->load2d(std::vector<BYTE>(40u*2*16*256,0xe5),1);
    fmDisk->load2hd(std::vector<BYTE>(77u*2*26*256,0xe5),2);
    Main6809Board main6809;
    main6809.setFloppyDevice(fmDisk);
    main6809.reset();
    for(unsigned channel=0;channel<2;++channel) {
        const unsigned len=channel?10900:6400;
        const WORD start=WORD(channel?0x5000:0x1000);
        const unsigned drive=channel?2:1;
        const auto bytes=stream(0,channel?1:0,channel?26:16,256,len);
        for(unsigned i=0;i<len;++i)main6809.writePhysical(start+i,bytes[i]);
        main6809.writeCpu(WORD(0xfd40+channel*4),BYTE(start>>8));
        main6809.writeCpu(WORD(0xfd41+channel*4),BYTE(start));
        main6809.writeCpu(WORD(0xfd42+channel*4),BYTE(len>>8));
        main6809.writeCpu(WORD(0xfd43+channel*4),BYTE(len));
        main6809.writeCpu(WORD(0xfd94+channel),0);
        main6809.writeCpu(WORD(0xfd50+channel),0x05); // enable + mem->I/O
        main6809.writeCpu(WORD(channel?0xfd31:0xfd19),0);
        main6809.writeCpu(WORD(channel?0xfd34:0xfd1c),BYTE(channel?1:0));
        main6809.writeCpu(WORD(channel?0xfd35:0xfd1d),BYTE(drive));
        main6809.writeCpu(WORD(channel?0xfd30:0xfd18),0xf0);
        const BYTE st=channel?main6809.readCpu(0xfd30):main6809.fdcStatus();
        if(st!=0)return 15;
        std::size_t offset=0;
        if(!fmDisk->sectorOffset(channel,0,channel?1:0,16,drive,offset) ||
           fmDisk->read(channel,offset,drive)!=16)return 16;
    }
    std::puts("PASS: 2D/2HD WRITE TRACK parser, 6809/8088 DMA0/1, D88 rebuild and reload");
    return 0;
}
