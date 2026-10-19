#include "Main6809Board.h"

int main()
{
    Main6809Board board(CpuType::MC6809);
    // Two channels have independently completed; an acknowledgement of one
    // must not discard the other channel's completion indication.
    board.writeCpu(0xfd50,0x80);
    board.writeCpu(0xfd51,0x81);
    board.writeCpu(0xfd55,0x8f);
    if(board.readCpu(0xfd50)!=0x80)return 1;
    if(board.readCpu(0xfd50)!=0x00)return 2;
    if((board.readCpu(0xfd55)&0x80)==0)return 3;
    if(board.readCpu(0xfd51)!=0x81)return 4;
    if(board.readCpu(0xfd51)!=0x01)return 5;
    if((board.readCpu(0xfd55)&0x80)!=0)return 6;
    return 0;
}
