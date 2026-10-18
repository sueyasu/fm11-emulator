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
        0x3e,0x0f, // MVI A,0F
        0x37,      // STC
        0xde,0x00, // SBI 00: 0F-0-1=0E, AC=0, CY=0
        0x3e,0x10, // MVI A,10
        0x37,      // STC
        0xde,0x00, // SBI 00: 10-0-1=0F, AC=1, CY=0
        0x3e,0x0f, // MVI A,0F
        0x37,      // STC
        0xce,0x00, // ACI 00: 0F+0+1=10, AC=1, CY=0
        0x3e,0x00, // MVI A,00
        0x37,      // STC
        0xde,0x00, // SBI 00: 00-0-1=FF, AC=1, CY=1
        0xed,0xfd  // RETEM
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x3100+i,program[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne(); // JMP MOV SP BRKEM
    for(int i=0;i<3;++i)board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0x0e || (cpu.flags()&0x11)!=0)return 1;
    for(int i=0;i<3;++i)board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0x0f || (cpu.flags()&0x11)!=0x10)return 2;
    for(int i=0;i<3;++i)board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0x10 || (cpu.flags()&0x11)!=0x10)return 3;
    for(int i=0;i<3;++i)board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0xff || (cpu.flags()&0x11)!=0x11)return 4;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080())return 5;
    return 0;
}
