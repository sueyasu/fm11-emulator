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
        0x3e,0x01, // MVI A,01
        0xe6,0x01, // ANI 01: bit3=0 on both, AC=0
        0x3e,0x08, // MVI A,08
        0xe6,0x00, // ANI 00: lhs bit3=1, AC=1
        0x3e,0x00, // MVI A,00
        0x06,0x08, // MVI B,08
        0xa0,      // ANA B: rhs bit3=1, AC=1
        0x3e,0x07, // MVI A,07
        0xe6,0x07, // ANI 07: bit3=0, AC=0
        0xed,0xfd  // RETEM
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x3100+i,program[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne(); // JMP MOV SP BRKEM
    board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=1 || (cpu.flags()&0x11)!=0)return 1;
    board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0 || (cpu.flags()&0x11)!=0x10)return 2;
    board.cpu().executeOne();board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0 || (cpu.flags()&0x11)!=0x10)return 3;
    board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=7 || (cpu.flags()&0x11)!=0)return 4;
    board.cpu().executeOne();
    if(cpu.emulation8080() || cpu.reg(4)!=0x8000)return 5;
    return 0;
}
