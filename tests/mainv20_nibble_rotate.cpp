#include "Main8088Board.h"

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE program[]={
        0xb0,0x03,         // MOV AL,03
        0xb3,0x95,         // MOV BL,95
        0xf9,              // STC
        0x0f,0x28,0xc3,   // ROL4 BL => BL=53 AL=09
        0x0f,0x2a,0xc3,   // ROR4 BL => BL=95 AL=03
        0xbb,0x00,0x02,   // MOV BX,0200
        0x0f,0x28,0x07,   // ROL4 byte [BX]
        0x0f,0x2a,0x07,   // ROR4 byte [BX]
        0xf4
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    board.bus().writeMemory(0x200,0x95);
    board.reset();
    auto& core=static_cast<Main8088Board::Core&>(board.cpu());
    board.cpu().executeOne(); // reset vector
    board.cpu().executeOne(); // AL
    board.cpu().executeOne(); // BL
    board.cpu().executeOne(); // STC
    const WORD flags=core.flags();
    board.cpu().executeOne(); // ROL4 BL
    if((core.reg(3)&0xff)!=0x53 || (core.reg(0)&0xff)!=0x09 ||
       core.flags()!=flags)return 1;
    board.cpu().executeOne(); // ROR4 BL
    if((core.reg(3)&0xff)!=0x95 || (core.reg(0)&0xff)!=0x03 ||
       core.flags()!=flags)return 2;
    board.cpu().executeOne(); // BX
    board.cpu().executeOne(); // ROL4 [BX]
    if(board.bus().readMemory(0x200)!=0x53 || (core.reg(0)&0xff)!=9 ||
       core.flags()!=flags)return 3;
    board.cpu().executeOne(); // ROR4 [BX]
    if(board.bus().readMemory(0x200)!=0x95 || (core.reg(0)&0xff)!=3 ||
       core.flags()!=flags)return 4;
    board.cpu().executeOne(); // HLT
    if(!core.halted())return 5;
    return 0;
}
