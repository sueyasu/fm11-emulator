#include "Main6809Board.h"
#include <vector>

static void setup(Main6809Board& b,WORD addr,BYTE sector)
{
    b.writeCpu(0xfd19,0);
    b.writeCpu(0xfd1a,sector);
    b.writeCpu(0xfd1c,0);
    b.writeCpu(0xfd1d,0);
    b.writeCpu(0xfd40,BYTE(addr>>8));
    b.writeCpu(0xfd41,BYTE(addr));
    b.writeCpu(0xfd42,1);
    b.writeCpu(0xfd43,0);
}

static bool registersAdvanced(Main6809Board& b,WORD start)
{
    const WORD result=WORD(start+256);
    return b.readCpu(0xfd40)==BYTE(result>>8) &&
           b.readCpu(0xfd41)==BYTE(result) &&
           b.readCpu(0xfd42)==0 && b.readCpu(0xfd43)==0;
}

int main()
{
    Main6809Board b;
    b.reset();
    std::vector<BYTE> disk(40*2*16*256);
    for(unsigned i=0;i<256;++i)disk[i]=BYTE(i^0xa5);
    b.loadFloppy2d(disk);
    setup(b,0x20f0,1);
    b.writeCpu(0xfd50,0x04);
    b.writeCpu(0xfd18,0x80);
    if(!registersAdvanced(b,0x20f0))return 1;
    for(unsigned i=0;i<256;++i)
        if(b.readCpu(WORD(0x20f0+i))!=BYTE(i^0xa5))return 2;
    setup(b,0x31f0,2);
    for(unsigned i=0;i<256;++i)b.writeCpu(WORD(0x31f0+i),BYTE(i^0x36));
    b.writeCpu(0xfd50,0x05);
    b.writeCpu(0xfd18,0xa0);
    if(!registersAdvanced(b,0x31f0))return 3;
    auto result=b.floppyImage();
    for(unsigned i=0;i<256;++i)
        if(result[256+i]!=BYTE(i^0x36))return 4;
    // On error, no bytes transferred: retain the programmed DMA registers.
    setup(b,0x4000,17);
    b.writeCpu(0xfd18,0x80);
    if(b.readCpu(0xfd40)!=0x40 || b.readCpu(0xfd41)!=0 ||
       b.readCpu(0xfd42)!=1 || b.readCpu(0xfd43)!=0)return 5;
    return 0;
}
