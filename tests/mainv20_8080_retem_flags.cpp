#include "Main8088Board.h"
int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE native[]={0xbc,0x00,0x80,0xf9,0x0f,0xff,0x40,0xf4};
    for(unsigned i=0;i<sizeof(native);++i)board.bus().writeMemory(0x10000+i,native[i]);
    board.bus().writeMemory(0x100,0);
    board.bus().writeMemory(0x101,1);
    board.bus().writeMemory(0x102,0);
    board.bus().writeMemory(0x103,3);
    const BYTE code[]={0xf3,0x3e,0x0f,0xc6,0x01,0xed,0xfd};
    for(unsigned i=0;i<sizeof(code);++i)board.bus().writeMemory(0x3100+i,code[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<4;++i)board.cpu().executeOne(); // JMP, MOV SP, STC, BRKEM
    if(!cpu.emulation8080())return 1;
    for(int i=0;i<3;++i)board.cpu().executeOne(); // DI, MVI, ADI
    if((cpu.flags()&0x210)!=0x10)return 2;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080() || cpu.reg(4)!=0x8000 ||
       (cpu.flags()&0x203)!=0x003 || cpu.ip()!=7)return 3;
    board.cpu().executeOne();
    if(!cpu.halted())return 4;
    return 0;
}
