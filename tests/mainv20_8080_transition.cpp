#include "Main8088Board.h"
#include <stdexcept>

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE native[]={0xbc,0x00,0x80,0x0f,0xff,0x40,0xb8,0x34,0x12,0xf4};
    for(unsigned i=0;i<sizeof(native);++i)
        board.bus().writeMemory(0x10000+i,native[i]);
    // Vector 40h targets 2000:0100.
    board.bus().writeMemory(0x100,0x00);
    board.bus().writeMemory(0x101,0x01);
    board.bus().writeMemory(0x102,0x00);
    board.bus().writeMemory(0x103,0x20);
    board.bus().writeMemory(0x20100,0x00); // 8080 NOP
    board.bus().writeMemory(0x20101,0xed); // RETEM
    board.bus().writeMemory(0x20102,0xfd);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    board.cpu().executeOne(); // reset JMP
    board.cpu().executeOne(); // MOV SP
    board.cpu().executeOne(); // BRKEM
    if(!cpu.emulation8080() || cpu.pc()!=0x20100 ||
       cpu.reg(4)!=0x7ffa)return 1;
    board.cpu().executeOne(); // emulated NOP
    if(cpu.pc()!=0x20101 || !cpu.emulation8080())return 2;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080() || cpu.pc()!=0x10006 ||
       cpu.reg(4)!=0x8000)return 3;
    board.cpu().executeOne(); // native MOV AX
    if(cpu.reg(0)!=0x1234)return 4;
    board.cpu().executeOne(); // native HLT
    if(!cpu.halted())return 5;
    board.reset();
    if(cpu.emulation8080())return 6;
    Main8088Board intel(CpuType::I8088);
    intel.bus().loadBootRom(rom);
    intel.bus().writeMemory(0x10000,0x0f); // POP CS, not BRKEM
    intel.reset();intel.cpu().executeOne();
    intel.cpu().executeOne();
    if(static_cast<Main8088Board::Core&>(intel.cpu()).emulation8080())return 7;
    return 0;
}
