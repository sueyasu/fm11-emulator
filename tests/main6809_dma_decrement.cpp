#include "Main6809Board.h"
#include <vector>

static void setup(Main6809Board& b,WORD start,BYTE sector)
{
    b.writeCpu(0xfd19,0);
    b.writeCpu(0xfd1a,sector);
    b.writeCpu(0xfd1c,0);
    b.writeCpu(0xfd1d,0);
    b.writeCpu(0xfd40,BYTE(start>>8));
    b.writeCpu(0xfd41,BYTE(start));
    b.writeCpu(0xfd42,1);
    b.writeCpu(0xfd43,0);
}

static bool checkRegisters(Main6809Board& b,WORD value)
{
    return b.readCpu(0xfd40)==BYTE(value>>8) &&
           b.readCpu(0xfd41)==BYTE(value) &&
           b.readCpu(0xfd42)==0 && b.readCpu(0xfd43)==0;
}

int main()
{
    Main6809Board b;
    b.reset();
    std::vector<BYTE> disk(40*2*16*256);
    for(unsigned i=0;i<256;++i)disk[i]=BYTE(i^0xa3);
    b.loadFloppy2d(disk);

    setup(b,0x31ff,1);
    b.writeCpu(0xfd50,0x0c); // FDD -> RAM, decrement addresses
    b.writeCpu(0xfd18,0x80);
    if(!checkRegisters(b,0x30ff))return 1;
    for(unsigned i=0;i<256;++i)
        if(b.readCpu(WORD(0x31ff-i))!=BYTE(i^0xa3))return 2;

    setup(b,0x42ff,2);
    for(unsigned i=0;i<256;++i)
        b.writeCpu(WORD(0x42ff-i),BYTE(i^0x47));
    b.writeCpu(0xfd50,0x0d); // RAM -> FDD, decrement addresses
    b.writeCpu(0xfd18,0xa0);
    if(!checkRegisters(b,0x41ff))return 3;
    const auto result=b.floppyImage();
    for(unsigned i=0;i<256;++i)
        if(result[256+i]!=BYTE(i^0x47))return 4;
    return 0;
}
