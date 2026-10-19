#include "Main8088Bus.h"
#include <vector>

static void channel0(Main8088Bus& b,WORD address,BYTE mode)
{
    b.writeIo(0x1c,0);
    b.writeIo(0x10,BYTE(address));
    b.writeIo(0x10,BYTE(address>>8));
    b.writeIo(0x11,0xff); // 8237 terminal count = length - 1
    b.writeIo(0x11,0);
    b.writeIo(0x1b,mode);
    b.writeIo(0x1a,0); // unmask CH0
}

int main()
{
    Main8088Bus b;
    std::vector<BYTE> disk(40*2*16*256);
    for(unsigned i=0;i<256;++i)disk[i]=BYTE(i^0x5a);
    b.loadFloppy2d(disk);
    b.reset();
    b.writeIo(0xfd19,0);
    b.writeIo(0xfd1a,1);
    b.writeIo(0xfd1c,0);
    b.writeIo(0xfd1d,0);
    channel0(b,0x2000,0x44); // Single, device -> memory
    b.writeIo(0xfd18,0x80);
    for(unsigned i=0;i<256;++i)
        if(b.readRam(0x2000+i)!=BYTE(i^0x5a))return 1;
    if(b.readIo(0xfd18)!=0 || b.dmaChannel(0).count!=0xffff)return 2;
    if((b.readIo(0x18)&1)==0)return 3;

    b.writeIo(0xfd1a,2);
    for(unsigned i=0;i<256;++i)b.writeMemory(0x3000+i,BYTE(i^0xa5));
    channel0(b,0x3000,0x48); // Single, memory -> device
    b.writeIo(0xfd18,0xa0);
    if(!b.floppyDirty() || b.readIo(0xfd18)!=0)return 4;
    const auto out=b.floppyImage();
    for(unsigned i=0;i<256;++i)
        if(out[256+i]!=BYTE(i^0xa5))return 5;
    return 0;
}
