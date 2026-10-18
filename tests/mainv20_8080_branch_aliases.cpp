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
        0x31,0x00,0x40, // 0100 LXI SP,4000
        0xcd,0x10,0x01, // 0103 CALL 0110
        0x3e,0x5a,      // 0106 MVI A,5A
        0xed,0xfd,      // 0108 RETEM
        0x00,0x00,0x00,0x00,0x00,0x00, // 010A...010F
        0xcb,0x14,0x01, // 0110 undocumented JMP 0114
        0x00,           // 0113 must be skipped
        0xd9            // 0114 undocumented RET
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x3100+i,program[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne(); // JMP MOV SP BRKEM
    board.cpu().executeOne(); // LXI SP
    board.cpu().executeOne(); // CALL
    if(cpu.ip()!=0x0110 || cpu.reg(5)!=0x3ffe)return 1;
    board.cpu().executeOne(); // CB jump alias
    if(cpu.ip()!=0x0114)return 2;
    board.cpu().executeOne(); // D9 ret alias
    if(cpu.ip()!=0x0106 || cpu.reg(5)!=0x4000)return 3;
    board.cpu().executeOne(); // MVI A
    if((cpu.reg(0)&255)!=0x5a)return 4;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080() || cpu.reg(4)!=0x8000)return 5;
    board.cpu().executeOne(); // native HLT
    if(!cpu.halted())return 6;
    return 0;
}
