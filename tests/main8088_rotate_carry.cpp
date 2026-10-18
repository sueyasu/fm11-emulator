#include "Main8088Board.h"

namespace {
constexpr WORD CF=0x0001,OF=0x0800;
struct Case {
    BYTE opcode,modrm;
    WORD initial;
    BYTE count;
    bool initialCarry;
    WORD result;
    bool finalCarry;
    bool checkOverflow;
    bool overflow;
};
bool verify(CpuType type,const Case& c)
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(type);
    board.bus().loadBootRom(rom);
    const bool word=(c.opcode&1)!=0;
    const unsigned n=word?3u:2u;
    const BYTE mov=BYTE(word?0xb8:0xb0);
    board.bus().writeMemory(0x10000,mov);
    board.bus().writeMemory(0x10001,BYTE(c.initial));
    if(word)board.bus().writeMemory(0x10002,BYTE(c.initial>>8));
    board.bus().writeMemory(0x10000+n,0xb1);
    board.bus().writeMemory(0x10001+n,c.count);
    board.bus().writeMemory(0x10002+n,c.initialCarry?0xf9:0xf8);
    board.bus().writeMemory(0x10003+n,c.opcode);
    board.bus().writeMemory(0x10004+n,c.modrm);
    board.bus().writeMemory(0x10005+n,0xf4);
    board.reset();
    for(int i=0;i<4;++i)board.cpu().executeOne(); // reset, MOV, MOV CL, STC/CLC
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    const WORD before=cpu.flags();
    board.cpu().executeOne();
    const WORD mask=word?0xffff:0xff;
    if((cpu.reg(0)&mask)!=c.result)return false;
    if(bool(cpu.flags()&CF)!=c.finalCarry)return false;
    if(c.checkOverflow && bool(cpu.flags()&OF)!=c.overflow)return false;
    if(c.count==0 && cpu.flags()!=before)return false;
    return true;
}
}
int main()
{
    const Case cases[]={
        // RCL, through carry, differs from a simple ROL.
        {0xd2,0xd0,0x80,1,false,0x00,true,true,true},
        {0xd2,0xd0,0x80,1,true,0x01,true,true,true},
        {0xd2,0xd0,0x00,1,true,0x01,false,true,false},
        {0xd2,0xd0,0x01,2,false,0x04,false,false,false},
        {0xd2,0xd0,0x80,0,true,0x80,true,true,false},
        // RCR, through carry, differs from a simple ROR.
        {0xd2,0xd8,0x01,1,false,0x00,true,true,false},
        {0xd2,0xd8,0x01,1,true,0x80,true,true,true},
        {0xd2,0xd8,0x00,1,true,0x80,false,true,true},
        {0xd2,0xd8,0x02,2,false,0x80,true,false,false},
        {0xd2,0xd8,0x01,0,true,0x01,true,true,false},
        // 16-bit values at the sign-bit boundary.
        {0xd3,0xd0,0x8000,1,false,0x0000,true,true,true},
        {0xd3,0xd0,0x8000,1,true,0x0001,true,true,true},
        {0xd3,0xd8,0x0001,1,true,0x8000,true,true,true},
        {0xd3,0xd8,0x0002,2,false,0x8000,true,false,false},
        {0xd3,0xd0,0x8000,0,true,0x8000,true,true,false},
    };
    for(const auto type:{CpuType::I8088,CpuType::NECV20})
        for(const auto& item:cases)
            if(!verify(type,item))return 1;
    return 0;
}
