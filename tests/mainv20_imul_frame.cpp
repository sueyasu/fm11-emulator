#include "Main8088Board.h"
#include <stdexcept>

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE code[]={
        0xbc,0x00,0x80,       // MOV SP,8000
        0xbd,0x34,0x12,       // MOV BP,1234
        0xb8,0xfd,0xff,       // MOV AX,-3
        0x6b,0xc8,0xfe,       // IMUL CX,AX,-2 => 6
        0x69,0xd0,0x00,0x40,  // IMUL DX,AX,4000 => overflow
        0xc8,0x10,0x00,0x00,  // ENTER 16,0
        0xc9,                 // LEAVE
        0xf4
    };
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    board.reset();board.cpu().executeOne();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne();
    board.cpu().executeOne();
    if(cpu.reg(1)!=6 || (cpu.flags()&0x801)!=0)return 1;
    board.cpu().executeOne();
    if(cpu.reg(2)!=0x4000 || (cpu.flags()&0x801)!=0x801)return 2;
    const WORD priorFlags=cpu.flags();
    board.cpu().executeOne();
    if(cpu.reg(5)!=0x7ffe || cpu.reg(4)!=0x7fee ||
       board.bus().readMemory(0x7ffe)!=0x34 ||
       board.bus().readMemory(0x7fff)!=0x12 ||
       cpu.flags()!=priorFlags)return 3;
    board.cpu().executeOne();
    if(cpu.reg(5)!=0x1234 || cpu.reg(4)!=0x8000 ||
       cpu.flags()!=priorFlags)return 4;
    board.cpu().executeOne();
    if(!cpu.halted())return 5;
    Main8088Board intel(CpuType::I8088);
    intel.bus().loadBootRom(rom);
    intel.bus().writeMemory(0x10000,0xc8);
    intel.reset();intel.cpu().executeOne();
    try {intel.cpu().executeOne();}
    catch(const std::runtime_error&) {return 0;}
    return 6;
}
