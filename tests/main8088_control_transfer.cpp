#include "Main8088Board.h"
#include <stdexcept>

int main()
{
    Main8088Board board(CpuType::I8088);
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    board.bus().loadBootRom(rom);
    const BYTE program[]={
        0xbc,0x00,0x80,       // MOV SP,8000
        0xb8,0x34,0x12,       // MOV AX,1234
        0x50,                 // PUSH AX
        0x8f,0x06,0x00,0x02, // POP word [0200]
        0xf0,0xff,0x06,0x00,0x02, // LOCK INC word [0200]
        0x9b,                 // WAIT
        0x9a,0x00,0x01,0x00,0x20, // CALL FAR 2000:0100
        0xcc,                 // INT3
        0xb0,0x7f,0x04,0x01, // MOV AL,7F; ADD AL,1 sets OF
        0xce,                 // INTO
        0xf4
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    board.bus().writeMemory(0x20100,0xcb); // RETF
    // INT3 handler 3000:0000, INTO handler 3000:0010
    board.bus().writeMemory(0x0c,0);board.bus().writeMemory(0x0d,0);
    board.bus().writeMemory(0x0e,0);board.bus().writeMemory(0x0f,0x30);
    board.bus().writeMemory(0x10,0x10);board.bus().writeMemory(0x11,0);
    board.bus().writeMemory(0x12,0);board.bus().writeMemory(0x13,0x30);
    board.bus().writeMemory(0x30000,0xcf);
    board.bus().writeMemory(0x30010,0xcf);
    board.reset();
    auto& core=static_cast<Main8088Board::Core&>(board.cpu());
    board.cpu().executeOne(); // reset far jump
    for(int i=0;i<4;++i)board.cpu().executeOne(); // MOV SP, MOV AX, PUSH, POP
    if(board.bus().readMemory(0x200)!=0x34 ||
       board.bus().readMemory(0x201)!=0x12 || core.reg(4)!=0x8000)return 1;
    board.cpu().executeOne(); // LOCK INC [0200]
    if(board.bus().readMemory(0x200)!=0x35)return 2;
    board.cpu().executeOne(); // WAIT
    board.cpu().executeOne(); // far CALL
    if(board.cpu().pc()!=0x20100 || core.reg(4)!=0x7ffc)return 3;
    board.cpu().executeOne(); // RETF
    if(board.cpu().pc()!=0x10016 || core.reg(4)!=0x8000)return 4;
    board.cpu().executeOne(); // INT3
    if(board.cpu().pc()!=0x30000)return 5;
    board.cpu().executeOne(); // IRET
    if(board.cpu().pc()!=0x10017)return 6;
    board.cpu().executeOne(); // MOV AL
    board.cpu().executeOne(); // ADD AL
    board.cpu().executeOne(); // INTO
    if(board.cpu().pc()!=0x30010)return 7;
    board.cpu().executeOne(); // IRET
    board.cpu().executeOne(); // HLT
    if(!core.halted())return 8;
    return 0;
}
