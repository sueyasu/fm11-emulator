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
        0xba,0x00,0x02, // MOV DX,0200
        0xbe,0x00,0x01, // MOV SI,0100
        0xbf,0x00,0x02, // MOV DI,0200
        0xb9,0x02,0x00, // MOV CX,2
        0xf3,0x6e,     // REP OUTSB
        0xfd,          // STD
        0xbe,0x01,0x01, // MOV SI,0101
        0xb9,0x02,0x00, // MOV CX,2
        0xf3,0x6e,     // REP OUTSB backwards
        0xfc,          // CLD
        0x6c,          // INSB
        0xf4
    };
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    board.bus().writeMemory(0x10100,0x34);
    board.bus().writeMemory(0x10101,0x56);
    board.reset();board.cpu().executeOne();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<4;++i)board.cpu().executeOne();
    const WORD before=cpu.flags();
    board.cpu().executeOne(); // REP OUTSB iteration 1
    board.cpu().executeOne(); // iteration 2
    if(cpu.reg(6)!=0x102 || cpu.reg(1)!=0 || cpu.flags()!=before)return 1;
    board.cpu().executeOne(); // STD
    board.cpu().executeOne(); // MOV SI
    board.cpu().executeOne(); // MOV CX
    board.cpu().executeOne();board.cpu().executeOne(); // REP OUTSB reverse
    if(cpu.reg(6)!=0x00ff || cpu.reg(1)!=0 || !(cpu.flags()&0x400))return 2;
    board.cpu().executeOne(); // CLD
    board.cpu().executeOne(); // INSB
    if(cpu.reg(7)!=0x201)return 3;
    board.cpu().executeOne();
    if(!cpu.halted())return 4;

    Main8088Board intel(CpuType::I8088);
    intel.bus().loadBootRom(rom);
    intel.bus().writeMemory(0x10000,0x6c);
    intel.reset();intel.cpu().executeOne();
    try {intel.cpu().executeOne();}
    catch(const std::runtime_error&) {return 0;}
    return 5;
}
