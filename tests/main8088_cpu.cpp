#include "Machine.h"
#include "Main8088Board.h"
#include <memory>

int main()
{
    Main8088Board board(CpuType::I8088);
    Main8088Bus::Rom rom{};
    // FFFF:0000 = FFFF0. Far jump to 1000:0000.
    rom[0xff0]=0xea;rom[0xff1]=0x00;rom[0xff2]=0x00;
    rom[0xff3]=0x00;rom[0xff4]=0x10;
    board.bus().loadBootRom(rom);
    board.bus().writeMemory(0x10000,0xb8); // MOV AX,1234h
    board.bus().writeMemory(0x10001,0x34);
    board.bus().writeMemory(0x10002,0x12);
    board.bus().writeMemory(0x10003,0x90); // NOP
    board.bus().writeMemory(0x10004,0xf4); // HLT
    board.reset();
    if(board.cpu().type()!=CpuType::I8088 || board.cpu().pc()!=0xffff0)return 1;
    board.cpu().executeOne();
    if(board.cpu().pc()!=0x10000)return 2;
    board.cpu().executeOne();
    if(static_cast<Main8088Board::Core&>(board.cpu()).reg(0)!=0x1234 || board.cpu().pc()!=0x10003)return 3;
    board.cpu().executeOne();
    board.cpu().executeOne();
    if(!static_cast<Main8088Board::Core&>(board.cpu()).halted() || board.cpu().pc()!=0x10005)return 4;
    board.cpu().executeOne();
    if(board.cpu().pc()!=0x10005)return 5;
    Main8088Board v20(CpuType::NECV20);
    if(v20.cpu().type()!=CpuType::NECV20 || v20.cpu().pc()!=0xffff0)return 6;
    Machine m;
    auto a=m.addBoard(std::make_unique<Main8088Board>(CpuType::I8088));
    auto b=m.addBoard(std::make_unique<Main8088Board>(CpuType::NECV20));
    m.setActiveBoard(b);
    if(a==b || m.activeBoard()->cpu().type()!=CpuType::NECV20)return 7;
    return 0;
}
