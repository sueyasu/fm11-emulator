#include "Main6809Board.h"
#include <vector>

int main()
{
    Main6809Board board(CpuType::MC6809);
    board.reset();
    std::vector<BYTE> image(40*2*16*256);
    for(unsigned i=0;i<256;++i)image[i]=BYTE(i^0x5a);
    board.loadFloppy2d(image);
    board.writeCpu(0xfd19,0); // track
    board.writeCpu(0xfd1a,1); // sector
    board.writeCpu(0xfd1c,0); // side
    board.writeCpu(0xfd1d,0); // drive
    board.writeCpu(0xfd40,0x20); // DMA address 2000h
    board.writeCpu(0xfd41,0x00);
    board.writeCpu(0xfd42,0x01); // 256-byte count
    board.writeCpu(0xfd43,0x00);
    board.writeCpu(0xfd50,0x04); // DMA enabled, channel 0
    board.writeCpu(0xfd51,0x01); // other channel unaffected
    board.writeCpu(0xfd55,0x0f);
    // Software cannot set bit 7 of the channel control register.
    board.writeCpu(0xfd50,0x84);
    if(board.readCpu(0xfd50)!=0x04)return 1;
    board.writeCpu(0xfd18,0x80); // FDC read via DMA -> actual completion
    if((board.readCpu(0xfd55)&0x80)==0)return 2;
    if((board.readCpu(0xfd50)&0x80)==0)return 3;
    if((board.readCpu(0xfd50)&0x80)!=0)return 4;
    if((board.readCpu(0xfd55)&0x80)!=0)return 5;
    if(board.readCpu(0xfd51)!=0x01)return 6;
    for(unsigned i=0;i<256;++i)
        if(board.readCpu(WORD(0x2000+i))!=BYTE(i^0x5a))return 7;
    return 0;
}
