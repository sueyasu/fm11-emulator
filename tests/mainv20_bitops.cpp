#include "Main8088Board.h"
#include <stdexcept>

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE code[]={
        0xb0,0x01,             // MOV AL,1
        0x0f,0x18,0xc0,0x00,   // TEST1 AL,0 -> ZF=0
        0x0f,0x1a,0xc0,0x00,   // CLR1 AL,0 => 0
        0x0f,0x18,0xc0,0x00,   // TEST1 AL,0 -> ZF=1
        0x0f,0x1c,0xc0,0x07,   // SET1 AL,7 => 80h
        0x0f,0x1e,0xc0,0x07,   // NOT1 AL,7 => 0
        0xb1,0x0f,             // MOV CL,15
        0xb8,0x00,0x00,       // MOV AX,0
        0x0f,0x15,0xc0,       // SET1 AX,CL => 8000h
        0x0f,0x11,0xc0,       // TEST1 AX,CL => ZF=0
        0x0f,0x13,0xc0,       // CLR1 AX,CL => 0
        0xbb,0x00,0x02,       // MOV BX,0200
        0x0f,0x1c,0x07,0x03, // SET1 byte [BX],3 => 08
        0xf4
    };
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    board.reset();board.cpu().executeOne();
    auto& core=static_cast<Main8088Board::Core&>(board.cpu());
    board.cpu().executeOne();board.cpu().executeOne();
    if(core.flags()&0x40)return 1;
    board.cpu().executeOne();if((core.reg(0)&255)!=0)return 2;
    board.cpu().executeOne();if(!(core.flags()&0x40))return 3;
    board.cpu().executeOne();if((core.reg(0)&255)!=0x80)return 4;
    board.cpu().executeOne();if((core.reg(0)&255)!=0)return 5;
    board.cpu().executeOne();board.cpu().executeOne();
    board.cpu().executeOne();if(core.reg(0)!=0x8000)return 6;
    board.cpu().executeOne();if(core.flags()&0x40)return 7;
    board.cpu().executeOne();if(core.reg(0)!=0)return 8;
    board.cpu().executeOne();board.cpu().executeOne();
    if(board.bus().readRam(0x200)!=8)return 9;
    board.cpu().executeOne();if(!core.halted())return 10;
    return 0;
}
