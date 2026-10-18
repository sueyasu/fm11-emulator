#include "Main8088Board.h"

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE native[]={
        0xbc,0x00,0x80, // MOV SP,8000h
        0x0f,0xff,0x40, // BRKEM 40h
        0xf4            // native HLT
    };
    for(unsigned i=0;i<sizeof(native);++i)
        board.bus().writeMemory(0x10000+i,native[i]);
    // Vector 40h -> 0300:0100 (emulation).
    board.bus().writeMemory(0x100,0x00);
    board.bus().writeMemory(0x101,0x01);
    board.bus().writeMemory(0x102,0x00);
    board.bus().writeMemory(0x103,0x03);
    // Vector 41h -> 0400:0100 (native).
    board.bus().writeMemory(0x104,0x00);
    board.bus().writeMemory(0x105,0x01);
    board.bus().writeMemory(0x106,0x00);
    board.bus().writeMemory(0x107,0x04);
    const BYTE emulated[]={
        0x3e,0x12,      // MVI A,12
        0xed,0xed,0x41, // CALLN 41h
        0x06,0x34,      // MVI B,34 (after IRET)
        0xed,0xfd       // RETEM
    };
    for(unsigned i=0;i<sizeof(emulated);++i)
        board.bus().writeMemory(0x3100+i,emulated[i]);
    const BYTE handler[]={
        0xb8,0x78,0x56, // MOV AX,5678h
        0xcf            // IRET
    };
    for(unsigned i=0;i<sizeof(handler);++i)
        board.bus().writeMemory(0x4100+i,handler[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne(); // JMP, MOV SP, BRKEM
    if(!cpu.emulation8080() || cpu.pc()!=0x3100 || cpu.reg(4)!=0x7ffa)return 1;
    board.cpu().executeOne(); // MVI A
    board.cpu().executeOne(); // CALLN
    if(cpu.emulation8080() || cpu.pc()!=0x4100 || cpu.reg(4)!=0x7ff4)return 2;
    board.cpu().executeOne(); // native MOV AX
    if(cpu.reg(0)!=0x5678)return 3;
    board.cpu().executeOne(); // IRET
    if(!cpu.emulation8080() || cpu.pc()!=0x3105 || cpu.reg(4)!=0x7ffa)return 4;
    board.cpu().executeOne(); // MVI B
    if((cpu.reg(1)>>8)!=0x34)return 5;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080() || cpu.pc()!=0x10006 || cpu.reg(4)!=0x8000)return 6;
    board.cpu().executeOne(); // HLT
    if(!cpu.halted())return 7;
    return 0;
}
