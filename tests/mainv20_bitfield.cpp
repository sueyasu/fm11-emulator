#include "Main8088Board.h"

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    // CL is the start-bit register; DL encodes bit-count minus one.
    const BYTE program[]={
        0xbe,0x00,0x02,         // MOV SI,0200
        0xbf,0x00,0x03,         // MOV DI,0300
        0xb1,0x07,             // MOV CL,7
        0xb2,0x08,             // MOV DL,8 => 9 bits
        0x0f,0x33,0xd1,        // EXT CL,DL
        0xb1,0x07,             // MOV CL,7
        0x0f,0x39,0xc1,0x08,   // INS CL,8 => 9 bits
        0xf4
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    board.bus().writeMemory(0x200,0x80);
    board.bus().writeMemory(0x201,0x55);
    board.bus().writeMemory(0x300,0x00);
    board.bus().writeMemory(0x301,0x00);
    board.reset();board.cpu().executeOne();
    auto& core=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<4;++i)board.cpu().executeOne();
    const WORD flags=core.flags();
    board.cpu().executeOne(); // EXT: bits 7..15 => 0xAB
    if(core.reg(0)!=0xab || core.reg(6)!=0x202 ||
       (core.reg(1)&255)!=0 || core.flags()!=flags)return 1;
    board.cpu().executeOne(); // reset CL
    board.cpu().executeOne(); // INS
    if(board.bus().readMemory(0x300)!=0x80 ||
       board.bus().readMemory(0x301)!=0x55 ||
       core.reg(7)!=0x302 || (core.reg(1)&255)!=0 ||
       core.flags()!=flags)return 2;
    board.cpu().executeOne();
    if(!core.halted())return 3;
    return 0;
}
