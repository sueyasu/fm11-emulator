#include "Main8088Board.h"

int main()
{
    for(const auto type:{CpuType::I8088,CpuType::NECV20}) {
        Main8088Bus::Rom rom{};
        rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
        rom[0xff3]=0;rom[0xff4]=0x10;
        Main8088Board board(type);
        board.bus().loadBootRom(rom);
        const BYTE program[]={
            0xb8,0x00,0x20, // MOV AX,2000h
            0x8e,0xd0,      // MOV SS,AX
            0xbc,0x01,0x00, // MOV SP,0001h
            0xb8,0x34,0x12, // MOV AX,1234h
            0x50,           // PUSH AX => SS:FFFF=34, SS:0000=12
            0x5b,           // POP BX
            0xf4
        };
        for(unsigned i=0;i<sizeof(program);++i)
            board.bus().writeMemory(0x10000+i,program[i]);
        board.reset();board.cpu().executeOne();
        for(int i=0;i<5;++i)board.cpu().executeOne();
        auto& core=static_cast<Main8088Board::Core&>(board.cpu());
        if(core.reg(4)!=0xffff ||
           board.bus().readMemory(0x2ffff)!=0x34 ||
           board.bus().readMemory(0x20000)!=0x12)return 1;
        board.cpu().executeOne();
        if(core.reg(3)!=0x1234 || core.reg(4)!=1)return 2;
        board.cpu().executeOne();
        if(!core.halted())return 3;
    }
    return 0;
}
