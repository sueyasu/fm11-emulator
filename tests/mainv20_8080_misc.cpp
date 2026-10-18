#include "Main8088Board.h"
int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE native[]={
        0xbc,0x00,0x80,           // MOV SP,8000h
        0xb8,0x00,0x20,0x8e,0xd8, // MOV AX,2000h; MOV DS,AX
        0x0f,0xff,0x40,0xf4      // BRKEM 40h; HLT
    };
    for(unsigned i=0;i<sizeof(native);++i)board.bus().writeMemory(0x10000+i,native[i]);
    board.bus().writeMemory(0x100,0);
    board.bus().writeMemory(0x101,1);
    board.bus().writeMemory(0x102,0);
    board.bus().writeMemory(0x103,3);
    const BYTE code[]={
        0x21,0xff,0xff, // LXI H,FFFF
        0x01,0x01,0x00, // LXI B,1
        0x09,           // DAD B => HL=0 CF=1
        0x23,           // INX H => HL=1, CF retained
        0x2b,           // DCX H => HL=0
        0x11,0x34,0x12, // LXI D,1234
        0xeb,           // XCHG => HL=1234, DE=0
        0x31,0x00,0x40, // LXI SP,4000
        0xe5,           // PUSH H
        0x21,0x78,0x56, // LXI H,5678
        0xe3,           // XTHL => HL=1234, stack=5678
        0xe1,           // POP H => HL=5678
        0x3e,0x80,      // MVI A,80
        0x07,           // RLC => A=01 CF=1
        0x17,           // RAL => A=03 CF=0
        0x37,           // STC
        0x1f,           // RAR => A=81 CF=1
        0x3f,           // CMC => CF=0
        0x2f,           // CMA => A=7E
        0xed,0xfd       // RETEM
    };
    for(unsigned i=0;i<sizeof(code);++i)board.bus().writeMemory(0x3100+i,code[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<5;++i)board.cpu().executeOne(); // JMP/MOV SP/MOV AX/MOV DS/BRKEM
    for(int i=0;i<3;++i)board.cpu().executeOne(); // LXI H, LXI B, DAD
    if(cpu.reg(3)!=0 || !(cpu.flags()&1))return 1;
    for(int i=0;i<9;++i)board.cpu().executeOne(); // INX DCX LXI D XCHG LXI SP PUSH LXI H XTHL POP
    if(cpu.reg(3)!=0x5678 || cpu.reg(2)!=0 || cpu.reg(5)!=0x4000 ||
       board.bus().readMemory(0x23ffe)!=0x78 ||
       board.bus().readMemory(0x23fff)!=0x56)return 2;
    board.cpu().executeOne(); // MVI A
    board.cpu().executeOne(); // RLC
    if((cpu.reg(0)&255)!=1 || !(cpu.flags()&1))return 3;
    board.cpu().executeOne(); // RAL
    if((cpu.reg(0)&255)!=3 || (cpu.flags()&1))return 4;
    board.cpu().executeOne(); // STC
    board.cpu().executeOne(); // RAR
    if((cpu.reg(0)&255)!=0x81 || !(cpu.flags()&1))return 5;
    board.cpu().executeOne(); // CMC
    board.cpu().executeOne(); // CMA
    if((cpu.reg(0)&255)!=0x7e || (cpu.flags()&1))return 6;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080())return 7;
    return 0;
}
