#include "Main6809Board.h"
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <memory>

static void put16(std::vector<BYTE>& b,std::size_t p,unsigned v) {
    b[p]=BYTE(v); b[p+1]=BYTE(v>>8);
}
static void put32(std::vector<BYTE>& b,std::size_t p,unsigned v) {
    put16(b,p,v);put16(b,p+2,v>>16);
}
int main() {
    std::vector<BYTE> img(0x2b0);
    img[0x1b]=0; // 2D
    for(unsigned t=0;t<80;++t) {
        put32(img,0x20+t*4,static_cast<unsigned>(img.size()));
        for(unsigned j=0;j<16;++j) {
            // Reverse sector records to require CHRN-based reconstruction.
            const unsigned sector=16-j;
            const auto pos=img.size();
            img.resize(pos+16+256);
            img[pos]=BYTE(t/2);img[pos+1]=BYTE(t%2);
            img[pos+2]=BYTE(sector);img[pos+3]=1;
            put16(img,pos+4,16);put16(img,pos+14,256);
            for(unsigned k=0;k<256;++k)
                img[pos+16+k]=BYTE(t+sector+k);
        }
    }
    put32(img,0x1c,static_cast<unsigned>(img.size()));
    Main6809Board b; b.reset();
    b.loadFloppyD88(img);
    // Real blank_2d.d88 uses media type 10h (2DD) with this exact
    // 40x2x16x256 Mini-FDC geometry; both 00h and 10h must work.
    auto doubleDensity=img;
    doubleDensity[0x1b]=0x10;
    b.loadFloppyD88(doubleDensity);
    if(b.floppyImage()!=doubleDensity)return 11;
    b.writeCpu(0xfd19,1);b.writeCpu(0xfd1c,1);
    b.writeCpu(0xfd1a,3);b.writeCpu(0xfd1d,0);
    b.writeCpu(0xfd18,0x80);
    for(unsigned k=0;k<256;++k)
        if(b.readCpu(0xfd1b)!=BYTE(3+3+k))return 1;
    auto bad=img;
    bad[0x1b]=0x20;
    try { b.loadFloppyD88(bad); return 2; }
    catch(const std::invalid_argument&) {}
    bad=img; bad.resize(bad.size()-1);
    try { b.loadFloppyD88(bad); return 3; }
    catch(const std::invalid_argument&) {}
    bad=img; bad[0x2b0+2]=0;
    try { b.loadFloppyD88(bad); return 4; }
    catch(const std::invalid_argument&) {}
    // The NitrOS-9 native 2HD IPL reads 128-byte sectors with DMA
    // disabled, polling FD30.DRQ and consuming FD33 one byte at a time.
    std::vector<BYTE> native(0x2b0,0);
    native[0x1b]=0x20;
    for(unsigned t=0;t<154;++t) {
        put32(native,0x20+t*4,static_cast<unsigned>(native.size()));
        const unsigned length=t==0?128:256;
        for(unsigned id=1;id<=26;++id) {
            const auto pos=native.size();
            native.resize(pos+16+length,0);
            native[pos]=BYTE(t/2);
            native[pos+1]=BYTE(t&1);
            native[pos+2]=BYTE(id);
            native[pos+3]=BYTE(t==0?0:1);
            put16(native,pos+4,26);
            native[pos+6]=BYTE(t==0?0x40:0);
            put16(native,pos+14,length);
            for(unsigned k=0;k<length;++k)
                native[pos+16+k]=BYTE((t+id+k)&255);
        }
    }
    put32(native,0x1c,static_cast<unsigned>(native.size()));
    auto hd=std::make_shared<FloppyDevice>();
    hd->loadD882hd(native);
    Main6809Board hdBoard;
    hdBoard.setFloppyDevice(hd);
    hdBoard.reset();
    hdBoard.writeCpu(0xfd35,0);
    hdBoard.writeCpu(0xfd31,0);
    hdBoard.writeCpu(0xfd34,0);
    hdBoard.writeCpu(0xfd32,1);
    hdBoard.writeCpu(0xfd51,0); // PIO, no DMA
    hdBoard.writeCpu(0xfd30,0x80);
    for(unsigned k=0;k<128;++k) {
        if(!(hdBoard.readCpu(0xfd30)&2))return 5;
        if(hdBoard.readCpu(0xfd33)!=BYTE((1+k)&255))return 6;
    }
    if(hdBoard.readCpu(0xfd30)&2)return 7;
    hdBoard.writeCpu(0xfd34,1);
    hdBoard.writeCpu(0xfd32,1);
    hdBoard.writeCpu(0xfd30,0x80);
    for(unsigned k=0;k<256;++k) {
        if(!(hdBoard.readCpu(0xfd30)&2))return 8;
        if(hdBoard.readCpu(0xfd33)!=BYTE((2+k)&255))return 9;
    }
    if(hdBoard.readCpu(0xfd30)&2)return 10;
    return 0;
}
