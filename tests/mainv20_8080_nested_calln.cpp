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
    // BRKEM vector 40h: 0300:0100
    board.bus().writeMemory(0x100,0);
    board.bus().writeMemory(0x101,1);
    board.bus().writeMemory(0x102,0);
    board.bus().writeMemory(0x103,3);
    // CALLN vector 41h: 0400:0100
    board.bus().writeMemory(0x104,0);
    board.bus().writeMemory(0x105,1);
    board.bus().writeMemory(0x106,0);
    board.bus().writeMemory(0x107,4);
    // Native INT 42h: 0500:0100
    board.bus().writeMemory(0x108,0);
    board.bus().writeMemory(0x109,1);
    board.bus().writeMemory(0x10a,0);
    board.bus().writeMemory(0x10b,5);
    const BYTE emu[]={0xed,0xed,0x41,0x3e,0x5a,0xed,0xfd};
    for(unsigned i=0;i<sizeof(emu);++i)
        board.bus().writeMemory(0x3100+i,emu[i]);
    const BYTE handler[]={0xcd,0x42,0xcf}; // INT 42h; IRET
    for(unsigned i=0;i<sizeof(handler);++i)
        board.bus().writeMemory(0x4100+i,handler[i]);
    board.bus().writeMemory(0x5100,0xcf); // nested IRET
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne(); // reset, MOV SP, BRKEM
    board.cpu().executeOne(); // CALLN
    if(cpu.emulation8080() || cpu.pc()!=0x4100 || cpu.reg(4)!=0x7ff4)return 1;
    board.cpu().executeOne(); // native INT 42h
    if(cpu.pc()!=0x5100 || cpu.reg(4)!=0x7fee)return 2;
    board.cpu().executeOne(); // nested IRET
    if(cpu.emulation8080() || cpu.pc()!=0x4102 || cpu.reg(4)!=0x7ff4)return 3;
    board.cpu().executeOne(); // outer IRET
    if(!cpu.emulation8080() || cpu.pc()!=0x3103 || cpu.reg(4)!=0x7ffa)return 4;
    board.cpu().executeOne(); // 8080 MVI A,5A
    if((cpu.reg(0)&0xff)!=0x5a)return 5;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080() || cpu.pc()!=0x10006 || cpu.reg(4)!=0x8000)return 6;
    board.cpu().executeOne();
    if(!cpu.halted())return 7;
    return 0;
}
