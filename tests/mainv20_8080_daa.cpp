#include "Main8088Board.h"

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE native[]={0xbc,0x00,0x80,0x0f,0xff,0x40,0xf4};
    for(unsigned i=0;i<sizeof(native);++i)board.bus().writeMemory(0x10000+i,native[i]);
    board.bus().writeMemory(0x100,0);
    board.bus().writeMemory(0x101,1);
    board.bus().writeMemory(0x102,0);
    board.bus().writeMemory(0x103,3);
    const BYTE program[]={
        0x3e,0x09, // MVI A,09
        0xc6,0x01, // ADI 01 => 0A
        0x27,      // DAA => 10
        0x3e,0x99, // MVI A,99
        0xc6,0x01, // ADI 01 => 9A
        0x27,      // DAA => 00, carry
        0x3e,0x00, // MVI A,00
        0xee,0x00, // XRI 00: clear AC and CY
        0x37,      // STC: CY=1, AC=0
        0x27,      // DAA => 60 (carry retained)
        0xed,0xfd // RETEM
    };
    for(unsigned i=0;i<sizeof(program);++i)board.bus().writeMemory(0x3100+i,program[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne(); // JMP/MOV SP/BRKEM
    board.cpu().executeOne();board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0x10 || (cpu.flags()&1))return 1;
    board.cpu().executeOne();board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0 || (cpu.flags()&0x41)!=0x41)return 2;
    board.cpu().executeOne();board.cpu().executeOne();
    board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0x60 || !(cpu.flags()&1))return 3;
    board.cpu().executeOne();
    if(cpu.emulation8080())return 4;
    return 0;
}
