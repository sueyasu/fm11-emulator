#include "Main8088Board.h"
#include <stdexcept>

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea; rom[0xff1]=0; rom[0xff2]=0;
    rom[0xff3]=0; rom[0xff4]=0x10;
    Main8088Board board(CpuType::I8088);
    board.bus().loadBootRom(rom);
    const BYTE program[]={
        0xbc,0x00,0x80, // MOV SP,8000
        0xb8,0x00,0x20, // MOV AX,2000
        0x50,           // PUSH AX
        0x0f            // POP CS => CS=2000, IP remains 0008
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    board.bus().writeMemory(0x20008,0xd8); // ESC /r, memory, disp16
    board.bus().writeMemory(0x20009,0x06);
    board.bus().writeMemory(0x2000a,0x34);
    board.bus().writeMemory(0x2000b,0x12);
    board.bus().writeMemory(0x2000c,0xdb); // ESC register operand
    board.bus().writeMemory(0x2000d,0xc0);
    board.bus().writeMemory(0x2000e,0xf4); // HLT
    board.reset();
    for(unsigned i=0;i<5;++i)board.cpu().executeOne();
    auto& core=static_cast<Main8088Board::Core&>(board.cpu());
    if(core.cs()!=0x2000 || core.ip()!=8 || core.reg(4)!=0x8000)return 1;
    board.cpu().executeOne();
    if(core.ip()!=12)return 2;
    board.cpu().executeOne();
    if(core.ip()!=14)return 3;
    board.cpu().executeOne();
    if(!core.halted())return 4;
    Main8088Board v20(CpuType::NECV20);
    v20.bus().loadBootRom(rom);
    v20.bus().writeMemory(0x10000,0x0f);
    v20.reset();v20.cpu().executeOne();
    try {
        v20.cpu().executeOne();
        return 5;
    } catch(const std::runtime_error&) {
        // Do not silently decode V20's 0F extended instruction as POP CS.
    }
    return 0;
}
