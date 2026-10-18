#include "Main8088Board.h"

// Boundary/flag regression tests for the V20 packed-BCD string operations.
int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE code[]={
        0xb8,0x00,0x20, 0x8e,0xc0, // ES=2000
        0xbe,0x00,0x01,             // SI=0100 (DS=0)
        0xbf,0x00,0x02,             // DI=0200
        0xb1,0x00,                  // CL=0
        0xf9,                       // STC
        0x0f,0x20,                  // ADD4S with 0 digits
        0xb1,0x04,                  // CL=4
        0x0f,0x22,                  // SUB4S: 0000 - 0001 => 9999, borrow
        0x0f,0x26,                  // CMP4S: 9999 - 0001 => 9998, no write
        0xf4
    };
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    board.bus().writeMemory(0x100,0x01);
    board.bus().writeMemory(0x101,0x00);
    board.bus().writeMemory(0x20200,0x00);
    board.bus().writeMemory(0x20201,0x00);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    board.cpu().executeOne(); // reset JMP
    for(int i=0;i<6;++i)board.cpu().executeOne(); // ES, SI, DI, CL, STC
    // Above: MOV AX, MOV ES, MOV SI, MOV DI, MOV CL, STC
    const WORD saved=cpu.flags();
    board.cpu().executeOne(); // ADD4S CL=0
    if(cpu.flags()!=saved || board.bus().readMemory(0x20200)!=0)return 1;
    board.cpu().executeOne(); // CL=4
    board.cpu().executeOne(); // SUB4S
    if(board.bus().readMemory(0x20200)!=0x99 ||
       board.bus().readMemory(0x20201)!=0x99 ||
       (cpu.flags()&0x41)!=0x01)return 2;
    board.cpu().executeOne(); // CMP4S
    if(board.bus().readMemory(0x20200)!=0x99 ||
       board.bus().readMemory(0x20201)!=0x99 ||
       (cpu.flags()&0x41)!=0)return 3;
    if(cpu.reg(6)!=0x100 || cpu.reg(7)!=0x200 ||
       (cpu.reg(1)&0xff)!=4)return 4;
    board.cpu().executeOne();
    if(!cpu.halted())return 5;
    return 0;
}
