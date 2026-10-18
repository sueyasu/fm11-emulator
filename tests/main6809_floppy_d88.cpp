#include "Main6809Board.h"
#include <cstdint>
#include <stdexcept>
#include <vector>

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
    return 0;
}
