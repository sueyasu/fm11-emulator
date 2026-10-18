#include "Main8088Board.h"

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE native[]={0xbc,0x00,0x80,0x0f,0xff,0x40,0xf4};
    for(unsigned i=0;i<sizeof(native);++i)
        board.bus().writeMemory(0x10000+i,native[i]);
    board.bus().writeMemory(0x100,0);
    board.bus().writeMemory(0x101,1);
    board.bus().writeMemory(0x102,0);
    board.bus().writeMemory(0x103,3);
    const BYTE program[]={
        0xf3,           // DI
        0x3e,0xa5,      // MVI A,A5
        0xd3,0x01,      // OUT PIC mask
        0x3e,0x00,      // MVI A,00
        0xdb,0x01,      // IN PIC mask
        0xfb,           // EI
        0xf3,           // DI
        0xed,0xfd       // RETEM
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x3100+i,program[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    board.cpu().executeOne(); // reset JMP
    board.cpu().executeOne(); // native MOV SP
    board.cpu().executeOne(); // BRKEM
    if(!cpu.emulation8080())return 1;
    board.cpu().executeOne(); // DI
    if(cpu.flags()&0x200)return 2;
    board.cpu().executeOne(); // MVI A
    board.cpu().executeOne(); // OUT
    if(board.bus().interruptMask()!=0xa5)return 3;
    board.cpu().executeOne(); // MVI A
    board.cpu().executeOne(); // IN
    if((cpu.reg(0)&0xff)!=0xa5)return 4;
    board.cpu().executeOne(); // EI
    if(!(cpu.flags()&0x200))return 5;
    board.cpu().executeOne(); // DI
    if(cpu.flags()&0x200)return 6;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080())return 7;
    board.cpu().executeOne(); // native HLT
    if(!cpu.halted())return 8;
    return 0;
}
