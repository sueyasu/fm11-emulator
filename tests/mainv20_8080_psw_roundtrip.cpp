#include "Main8088Board.h"

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE native[]={0xbc,0x00,0x80,0xb8,0x00,0x20,0x8e,0xd8,
                         0x0f,0xff,0x40,0xf4};
    for(unsigned i=0;i<sizeof(native);++i)
        board.bus().writeMemory(0x10000+i,native[i]);
    board.bus().writeMemory(0x100,0);
    board.bus().writeMemory(0x101,1);
    board.bus().writeMemory(0x102,0);
    board.bus().writeMemory(0x103,3);
    const BYTE program[]={
        0x31,0x00,0x40, // LXI SP,4000
        0x3e,0x5a,      // MVI A,5A
        0x37,           // STC
        0xf5,           // PUSH PSW (A=5A, CY=1)
        0x3e,0x00,      // MVI A,00
        0xaf,           // XRA A (CY=0, Z=1)
        0xf1,           // POP PSW (restore A, CY; preserve native control)
        0xed,0xfd       // RETEM
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x3100+i,program[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<5;++i)board.cpu().executeOne(); // far JMP, SP, AX, DS, BRKEM
    for(int i=0;i<4;++i)board.cpu().executeOne(); // LXI SP, MVI, STC, PUSH
    if(cpu.reg(5)!=0x3ffe || board.bus().readMemory(0x23fff)!=0x5a ||
       (board.bus().readMemory(0x23ffe)&3)!=3)return 1;
    for(int i=0;i<3;++i)board.cpu().executeOne(); // MVI, XRA, POP
    if((cpu.reg(0)&0xff)!=0x5a || !(cpu.flags()&1) ||
       (cpu.flags()&0x40) || !(cpu.flags()&2) || cpu.reg(5)!=0x4000)return 2;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080() || cpu.reg(4)!=0x8000)return 3;
    board.cpu().executeOne(); // HLT
    if(!cpu.halted())return 4;
    return 0;
}
