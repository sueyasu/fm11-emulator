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
    board.bus().writeMemory(0x100,0x00);
    board.bus().writeMemory(0x101,0x01);
    board.bus().writeMemory(0x102,0x00);
    board.bus().writeMemory(0x103,0x03);
    const BYTE program[]={
        0x3e,0x7f, // MVI A,7F
        0xc6,0x01, // ADI 1 => 80h, S=1 AC=1
        0x06,0x01, // MVI B,1
        0x90,      // SUB B => 7Fh
        0xfe,0x7f, // CPI 7F => Z=1
        0x3e,0xff, // MVI A,FF
        0xc6,0x01, // ADI 1 => 00 CF=1 ZF=1
        0xce,0x00, // ACI 0 => 01 (CF=0)
        0x3d,      // DCR A => 00, retain CF
        0xe6,0x0f, // ANI 0F => 00, AF=1
        0x3e,0x12, // MVI A,12
        0xee,0x12, // XRI 12 => 00
        0xed,0xfd  // RETEM
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x3100+i,program[i]);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    board.cpu().executeOne();board.cpu().executeOne();board.cpu().executeOne();
    board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0x80 || (cpu.flags()&0x90)!=0x90)return 1;
    board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0x7f)return 2;
    board.cpu().executeOne();
    if(!(cpu.flags()&0x40) || (cpu.reg(0)&255)!=0x7f)return 3;
    board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0 || (cpu.flags()&0x41)!=0x41)return 4;
    board.cpu().executeOne();
    if((cpu.reg(0)&255)!=1 || (cpu.flags()&1))return 5;
    board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0 || !(cpu.flags()&0x40))return 6;
    board.cpu().executeOne();
    if((cpu.flags()&0x10)==0 || !(cpu.flags()&0x40))return 7;
    board.cpu().executeOne();board.cpu().executeOne();
    if((cpu.reg(0)&255)!=0 || !(cpu.flags()&0x40))return 8;
    board.cpu().executeOne();
    if(cpu.emulation8080())return 9;
    return 0;
}
