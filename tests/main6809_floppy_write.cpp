#include "Main6809Board.h"
#include <vector>
int main() {
    Main6809Board b; b.reset();
    std::vector<BYTE> img(40*2*16*256);
    b.loadFloppy2d(img);
    b.writeCpu(0xfd19,1); b.writeCpu(0xfd1a,2);
    b.writeCpu(0xfd1c,0); b.writeCpu(0xfd1d,0);
    b.writeCpu(0xfd40,0x20); b.writeCpu(0xfd41,0);
    b.writeCpu(0xfd42,1); b.writeCpu(0xfd43,0);
    for(unsigned i=0;i<256;++i)b.writeCpu(0x2000+i,BYTE(i^0x95));
    b.writeCpu(0xfd50,0x05);
    b.writeCpu(0xfd18,0xa0);
    if((b.readCpu(0xfd50)&0x80)==0 || !b.floppyDirty())return 1;
    b.writeCpu(0xfd50,0x04);b.writeCpu(0xfd18,0x80);
    for(unsigned i=0;i<256;++i)
        if(b.readCpu(0x2000+i)!=BYTE(i^0x95))return 2;
    const auto raw=b.floppyImage();
    const std::size_t off=((1*2)*16+1)*256;
    for(unsigned i=0;i<256;++i)if(raw[off+i]!=BYTE(i^0x95))return 3;
    b.clearFloppyDirty(); if(b.floppyDirty())return 4;
    b.writeCpu(0xfd1a,17);b.writeCpu(0xfd50,0x05);b.writeCpu(0xfd18,0xa0);
    if((b.readCpu(0xfd50)&0x40)==0 || b.floppyDirty())return 5;
    return 0;
}
