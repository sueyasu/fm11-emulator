#include "Main6809Board.h"
#include <vector>
int main()
{
    Main6809Board b;
    b.reset();
    std::vector<BYTE> disk(40*2*16*256);
    for(unsigned i=0;i<256;++i)disk[i]=BYTE(i^0xa7);
    b.loadFloppy2d(disk);
    b.writeCpu(0xfd19,0);b.writeCpu(0xfd1a,1);
    b.writeCpu(0xfd1c,0);b.writeCpu(0xfd1d,0);
    b.writeCpu(0xfd40,0);b.writeCpu(0xfd41,0x7f);
    b.writeCpu(0xfd42,1);b.writeCpu(0xfd43,0);
    b.writeCpu(0xfd50,0x0c); // decrement, FDD -> RAM
    b.writeCpu(0xfd18,0x80);
    for(unsigned i=0;i<256;++i)
        if(b.readPhysical(WORD(0x007f-i))!=BYTE(i^0xa7))return 1;
    if(b.readCpu(0xfd40)!=0xff || b.readCpu(0xfd41)!=0x7f)return 2;
    if((b.readCpu(0xfd50)&0x80)==0)return 3;
    return 0;
}
