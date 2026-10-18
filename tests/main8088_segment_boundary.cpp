#include "Main8088Board.h"

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::I8088);
    board.bus().loadBootRom(rom);
    // DS=1000, SS=2000. Check a word straddling offset FFFF/0000
    // and BP addressing, which must default to SS rather than DS.
    const BYTE program[]={
        0xb8,0x00,0x10,0x8e,0xd8, // MOV AX,1000; MOV DS,AX
        0xb8,0x00,0x20,0x8e,0xd0, // MOV AX,2000; MOV SS,AX
        0xbb,0xff,0xff,           // MOV BX,FFFF
        0xc7,0x07,0x34,0x12,      // MOV word [BX],1234
        0x8b,0x07,                // MOV AX,[BX]
        0xbd,0xff,0xff,           // MOV BP,FFFF
        0xc7,0x46,0x00,0x78,0x56, // MOV word [BP],5678 (SS)
        0x8b,0x46,0x00,           // MOV AX,[BP]
        0x3e,0x8b,0x46,0x00,      // MOV AX,DS:[BP] => 1234
        0xf4
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    board.cpu().executeOne();
    for(int i=0;i<5;++i)board.cpu().executeOne(); // DS, SS, BX
    board.cpu().executeOne(); // write DS:FFFF
    if(board.bus().readMemory(0x1ffff)!=0x34 ||
       board.bus().readMemory(0x10000)!=0x12)return 1;
    board.cpu().executeOne(); // read DS:FFFF
    if(cpu.reg(0)!=0x1234)return 2;
    board.cpu().executeOne(); // BP
    board.cpu().executeOne(); // write SS:FFFF
    if(board.bus().readMemory(0x2ffff)!=0x78 ||
       board.bus().readMemory(0x20000)!=0x56)return 3;
    board.cpu().executeOne(); // read SS:FFFF
    if(cpu.reg(0)!=0x5678)return 4;
    board.cpu().executeOne(); // DS override
    if(cpu.reg(0)!=0x1234)return 5;
    board.cpu().executeOne();
    if(!cpu.halted())return 6;
    return 0;
}
