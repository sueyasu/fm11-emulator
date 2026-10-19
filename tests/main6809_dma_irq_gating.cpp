#include "Main6809Board.h"
#include <vector>

int main()
{
    Main6809Board b(CpuType::MC6809);
    b.reset();
    std::vector<BYTE> disk(40*2*16*256,0x5a);
    b.loadFloppy2d(disk);
    b.writeCpu(0xfd19,0);
    b.writeCpu(0xfd1a,1);
    b.writeCpu(0xfd1c,0);
    b.writeCpu(0xfd1d,0);
    b.writeCpu(0xfd40,0x20);
    b.writeCpu(0xfd41,0);
    b.writeCpu(0xfd42,1);
    b.writeCpu(0xfd43,0);
    b.writeCpu(0xfd50,0x04);
    b.writeCpu(0xfd55,0x00); // DMA IRQ disabled
    b.writeCpu(0xfd18,0x80);
    if((b.readCpu(0xfd55)&0x80)==0)return 1;
    if(b.readCpu(0xfd03)&0x10)return 2;
    b.writeCpu(0xfd55,0x01); // enable CH0 IRQ after completion
    if((b.readCpu(0xfd03)&0x10)==0)return 3;
    b.writeCpu(0xfd55,0x00); // mask without acknowledging completion
    if(b.readCpu(0xfd03)&0x10)return 4;
    if((b.readCpu(0xfd55)&0x80)==0)return 5;
    b.writeCpu(0xfd55,0x01);
    b.writeCpu(0xfd50,0x04); // reprogram and acknowledge CH0
    if((b.readCpu(0xfd55)&0x80)!=0 || (b.readCpu(0xfd03)&0x10)!=0)return 6;
    if((b.readCpu(0xfd50)&0x80)!=0)return 7;
    // A software write cannot fabricate a DMA completion.
    b.writeCpu(0xfd55,0x81);
    if((b.readCpu(0xfd55)&0x80)!=0)return 8;
    if((b.readCpu(0xfd03)&0x10)!=0)return 9;
    return 0;
}
