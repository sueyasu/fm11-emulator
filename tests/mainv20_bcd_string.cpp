#include "Main8088Board.h"

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE program[]={
        0xb8,0x00,0x10,0x8e,0xd8, // DS=1000
        0xb8,0x00,0x20,0x8e,0xc0, // ES=2000
        0xbe,0x00,0x01,           // SI=0100
        0xbf,0x00,0x02,           // DI=0200
        0xb1,0x04,                // CL=4 digits
        0x0f,0x20,                // ADD4S
        0x0f,0x22,                // SUB4S
        0x0f,0x26,                // CMP4S (destination - source)
        0xf4
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    board.bus().writeMemory(0x10100,0x01); // source 0001
    board.bus().writeMemory(0x10101,0x00);
    board.bus().writeMemory(0x20200,0x34); // destination 1234
    board.bus().writeMemory(0x20201,0x12);
    board.reset();board.cpu().executeOne();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<7;++i)board.cpu().executeOne();
    board.cpu().executeOne(); // ADD4S => 1235
    if(board.bus().readRam(0x20200)!=0x35 ||
       board.bus().readRam(0x20201)!=0x12 ||
       (cpu.flags()&1)!=0)return 1;
    board.cpu().executeOne(); // SUB4S => 1234
    if(board.bus().readRam(0x20200)!=0x34 ||
       board.bus().readRam(0x20201)!=0x12 ||
       (cpu.flags()&1)!=0)return 2;
    board.cpu().executeOne(); // CMP4S => 1234-0001 != 0
    if(cpu.flags()&0x40 || board.bus().readRam(0x20200)!=0x34)return 3;
    if(cpu.reg(6)!=0x100 || cpu.reg(7)!=0x200 ||
       (cpu.reg(1)&255)!=4)return 4;

    // Check a carry across the complete packed BCD string.
    Main8088Board overflow(CpuType::NECV20);
    overflow.bus().loadBootRom(rom);
    for(unsigned i=0;i<sizeof(program);++i)
        overflow.bus().writeMemory(0x10000+i,program[i]);
    overflow.bus().writeMemory(0x10100,0x01);
    overflow.bus().writeMemory(0x10101,0);
    overflow.bus().writeMemory(0x20200,0x99);
    overflow.bus().writeMemory(0x20201,0x99);
    overflow.reset();overflow.cpu().executeOne();
    for(int i=0;i<8;++i)overflow.cpu().executeOne(); // through ADD4S
    auto& oc=static_cast<Main8088Board::Core&>(overflow.cpu());
    if(overflow.bus().readRam(0x20200)!=0 ||
       overflow.bus().readRam(0x20201)!=0 ||
       (oc.flags()&0x41)!=0x41)return 5;
    return 0;
}
