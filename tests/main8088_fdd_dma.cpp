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
    // Standard FDC uses a different register window and 8237 CH1.
    std::vector<BYTE> hd(77u*2*8*1024);
    for(unsigned i=0;i<1024;++i)hd[i]=BYTE(i^0x3c);
    b.loadFloppy2hd(hd);
    b.writeIo(0xfd31,0);
    b.writeIo(0xfd32,1);
    b.writeIo(0xfd34,0);
    b.writeIo(0xfd35,0);
    b.writeIo(0x1c,0);
    b.writeIo(0x12,0x00);
    b.writeIo(0x12,0x40);
    b.writeIo(0x13,0xff);
    b.writeIo(0x13,0x03); // 1024 bytes minus one
    b.writeIo(0x1b,0x45); // CH1 single, device -> memory
    b.writeIo(0x1a,1);    // unmask CH1
    b.writeIo(0xfd30,0x80);
    for(unsigned i=0;i<1024;++i)
        if(b.readRam(0x4000+i)!=BYTE(i^0x3c))return 6;
    if(b.dmaChannel(1).count!=0xffff || (b.readIo(0x18)&2)==0)return 7;

    for(unsigned i=0;i<1024;++i)b.writeMemory(0x5000+i,BYTE(i^0x67));
    b.writeIo(0xfd32,2);
    b.writeIo(0x1c,0);
    b.writeIo(0x12,0x00);
    b.writeIo(0x12,0x50);
    b.writeIo(0x13,0xff);
    b.writeIo(0x13,0x03);
    b.writeIo(0x1b,0x49); // CH1 single, memory -> device
    b.writeIo(0x1a,1);
    b.writeIo(0xfd30,0xa0);
    const auto hdResult=b.floppy2hdImage();
    for(unsigned i=0;i<1024;++i)
        if(hdResult[1024+i]!=BYTE(i^0x67))return 8;
    return 0;
}
