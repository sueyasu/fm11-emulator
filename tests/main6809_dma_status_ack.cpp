#include "Main6809Board.h"

int main()
{
    Main6809Board board(CpuType::MC6809);
    // Register backing and read-to-acknowledge behavior in original MainCpu.cpp.
    board.writeCpu(0xfd51,0x19);
    board.writeCpu(0xfd52,0x2a);
    if(board.readCpu(0xfd51)!=0x19 || board.readCpu(0xfd52)!=0x2a)return 1;
    // A channel-control read clears the completion flag of that channel.
    // Acknowledgment must not corrupt the other channels' registers.
    board.writeCpu(0xfd50,0x01);
    if(board.readCpu(0xfd50)!=0x01 || board.readCpu(0xfd50)!=0x01)return 2;
    if(board.readCpu(0xfd51)!=0x19 || board.readCpu(0xfd52)!=0x2a)return 3;
    board.reset();
    if(board.readCpu(0xfd50)!=0 || board.readCpu(0xfd51)!=0)return 4;
    return 0;
}
