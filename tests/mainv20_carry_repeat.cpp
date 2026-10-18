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
        0xbe,0x00,0x02, // MOV SI,0200
        0xbf,0x00,0x03, // MOV DI,0300
        0xb9,0x03,0x00, // MOV CX,3
        0x64,0xa6,      // REPNC CMPSB: equal, equal, then borrow
        0xbe,0x00,0x02, // MOV SI,0200
        0xbf,0x00,0x03, // MOV DI,0300
        0xb9,0x03,0x00, // MOV CX,3
        0x65,0xa6,      // REPC CMPSB: equal -> stop after one
        0xb9,0x00,0x00, // MOV CX,0
        0x64,0xa6,      // REPNC with CX=0: no operand access
        0xf4
    };
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    board.bus().writeMemory(0x200,0x10);
    board.bus().writeMemory(0x201,0x20);
    board.bus().writeMemory(0x202,0x10);
    board.bus().writeMemory(0x300,0x10);
    board.bus().writeMemory(0x301,0x20);
    board.bus().writeMemory(0x302,0x20);
    board.reset();board.cpu().executeOne();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne();
    for(int i=0;i<3;++i)board.cpu().executeOne();
    if(cpu.reg(1)!=0 || cpu.reg(6)!=0x203 || cpu.reg(7)!=0x303 ||
       !(cpu.flags()&1))return 1;
    for(int i=0;i<3;++i)board.cpu().executeOne();
    board.cpu().executeOne(); // REPC CMPSB exits after first no-borrow
    if(cpu.reg(1)!=2 || cpu.reg(6)!=0x201 || cpu.reg(7)!=0x301 ||
       (cpu.flags()&1))return 2;
    board.cpu().executeOne(); // MOV CX,0
    board.cpu().executeOne(); // REPNC CMPSB, skipped
    if(cpu.reg(1)!=0 || cpu.reg(6)!=0x201 || cpu.reg(7)!=0x301)return 3;
    board.cpu().executeOne();
    if(!cpu.halted())return 4;
    Main8088Board intel(CpuType::I8088);
    intel.bus().loadBootRom(rom);
    intel.bus().writeMemory(0x10000,0x64);
    intel.reset();intel.cpu().executeOne();
    try {intel.cpu().executeOne();}
    catch(const std::runtime_error&) {return 0;}
    return 5;
}
