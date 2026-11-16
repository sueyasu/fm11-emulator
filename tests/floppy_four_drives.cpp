#include "FloppyDevice.h"
#include <array>
#include <vector>
#include <cstdio>

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
    std::puts("PASS: four independent 2D and four independent 2HD drives");
    return 0;
}
