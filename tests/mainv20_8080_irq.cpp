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
    // BRKEM 40h -> 0300:0100; IRQ0 default PIC vector 08h -> 0400:0000.
    board.bus().writeMemory(0x100,0);
    board.bus().writeMemory(0x101,1);
    board.bus().writeMemory(0x102,0);
    board.bus().writeMemory(0x103,3);
    board.bus().writeMemory(0x20,0);
    board.bus().writeMemory(0x21,0);
    board.bus().writeMemory(0x22,0);
    board.bus().writeMemory(0x23,4);
    const BYTE emu[]={0xfb,0x00,0x3e,0x5a,0xed,0xfd}; // EI NOP MVI A RETEM
    for(unsigned i=0;i<sizeof(emu);++i)board.bus().writeMemory(0x3100+i,emu[i]);
    board.bus().writeMemory(0x4000,0xcf); // native IRET
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne(); // JMP, MOV SP, BRKEM
    if(!cpu.emulation8080())return 1;
    board.cpu().executeOne(); // EI
    board.bus().requestIrq(0);
    board.cpu().executeOne(); // NOP executes despite IRQ (EI delay)
    if(!cpu.emulation8080() || cpu.ip()!=0x0102)return 2;
    board.cpu().executeOne(); // IRQ acknowledge, native handler
    if(cpu.emulation8080() || cpu.pc()!=0x4000)return 3;
    board.cpu().executeOne(); // native IRET restores 8080 mode
    if(!cpu.emulation8080() || cpu.ip()!=0x0102)return 4;
    board.cpu().executeOne(); // MVI A,5A
    if((cpu.reg(0)&255)!=0x5a)return 5;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080() || cpu.reg(4)!=0x8000)return 6;
    board.cpu().executeOne(); // native HLT
    if(!cpu.halted())return 7;
    return 0;
}
