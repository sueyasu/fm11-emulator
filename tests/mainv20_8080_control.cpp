#include "Main8088Board.h"

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE native[]={
        0xbc,0x00,0x80, // native SP
        0xb8,0x00,0x20,0x8e,0xd8, // DS=2000
        0x0f,0xff,0x40,0xf4 // BRKEM; HLT
    };
    for(unsigned i=0;i<sizeof(native);++i)
        board.bus().writeMemory(0x10000+i,native[i]);
    // BRKEM vector 40h -> CS:IP=0300:0100.
    board.bus().writeMemory(0x100,0);
    board.bus().writeMemory(0x101,1);
    board.bus().writeMemory(0x102,0);
    board.bus().writeMemory(0x103,3);
    const BYTE code[]={
        0x31,0x00,0x40, // 0100: LXI SP,4000
        0x21,0x34,0x12, // 0103: LXI H,1234
        0xe5,           // 0106: PUSH H
        0x21,0,0,      // 0107: LXI H,0000
        0xe1,           // 010A: POP H
        0x7c,           // 010B: MOV A,H => 12h
        0xfe,0x12,      // 010C: CPI 12 => Z
        0xc2,0x19,0x01, // 010E: JNZ 0119 (must NOT branch)
        0xcc,0x20,0x01, // 0111: CZ 0120
        0xc3,0x24,0x01, // 0114: JMP 0124
        0x00,0x00,0x00, // 0117
        0x00,0x00,0x00, // 011A
        0x00,0x00,0x00, // 011D
        0x3e,0x5a,      // 0120: MVI A,5A
        0xc9,           // 0122: RET
        0x00,           // 0123
        0xed,0xfd       // 0124: RETEM
    };
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x3100+i,code[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<5;++i)board.cpu().executeOne(); // native through BRKEM
    for(int i=0;i<7;++i)board.cpu().executeOne(); // LXI SP, LXI H, PUSH, LXI H, POP, MOV, CPI
    if(cpu.reg(3)!=0x1234 || (cpu.reg(0)&0xff)!=0x12 ||
       (cpu.flags()&0x40)==0 || cpu.reg(5)!=0x4000)return 1;
    board.cpu().executeOne(); // JNZ not taken
    if(cpu.ip()!=0x0111)return 2;
    board.cpu().executeOne(); // CZ
    if(cpu.ip()!=0x0120 || cpu.reg(5)!=0x3ffe)return 3;
    board.cpu().executeOne(); // MVI A
    board.cpu().executeOne(); // RET
    if((cpu.reg(0)&0xff)!=0x5a || cpu.ip()!=0x0114 || cpu.reg(5)!=0x4000)return 4;
    board.cpu().executeOne(); // JMP to RETEM
    if(cpu.ip()!=0x0124)return 5;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080() || cpu.reg(4)!=0x8000)return 6;
    board.cpu().executeOne(); // native HLT
    if(!cpu.halted())return 7;
    return 0;
}
