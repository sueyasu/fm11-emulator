#include "Main8088Board.h"

namespace {
constexpr WORD CF=0x0001,PF=0x0004,ZF=0x0040,SF=0x0080,OF=0x0800;
constexpr WORD MASK=CF|PF|ZF|SF|OF;
struct Case {
    BYTE opcode,modrm;
    WORD initial;
    BYTE count;
    WORD result;
    WORD expectedFlags;
    WORD flagMask;
};
bool check(CpuType type,const Case& t)
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(type);
    board.bus().loadBootRom(rom);
    const bool word=(t.opcode&1)!=0;
    const BYTE code[]={
        BYTE(word?0xb8:0xb0),BYTE(t.initial),BYTE(t.initial>>8),
        0xb1,t.count, // MOV CL,count
        0xd0,0xc0, // placeholder replaced below
        0xf4
    };
    const unsigned movSize=word?3u:2u;
    for(unsigned i=0;i<movSize;++i)board.bus().writeMemory(0x10000+i,code[i]);
    board.bus().writeMemory(0x10000+movSize,0xb1);
    board.bus().writeMemory(0x10000+movSize+1,t.count);
    board.bus().writeMemory(0x10000+movSize+2,t.opcode);
    board.bus().writeMemory(0x10000+movSize+3,t.modrm);
    board.bus().writeMemory(0x10000+movSize+4,0xf4);
    board.reset();board.cpu().executeOne();
    board.cpu().executeOne();board.cpu().executeOne();
    const auto& cpu=static_cast<const Main8088Board::Core&>(board.cpu());
    const WORD before=cpu.flags();
    board.cpu().executeOne();
    const WORD widthMask=word?0xffff:0xff;
    const WORD expected=WORD(t.result&widthMask);
    const WORD actual=WORD(cpu.reg(0)&widthMask);
    if(actual!=expected)return false;
    if((cpu.flags()&t.flagMask)!=t.expectedFlags)return false;
    if(t.count==0 && cpu.flags()!=before)return false;
    return true;
}
}
int main()
{
    const Case cases[]={
        // CL=0: registers and every flag unchanged (both data widths).
        {0xd2,0xe0,0x80,0,0x80,0,0},
        {0xd3,0xe8,0x8001,0,0x8001,0,0},
        // SHL by one: sign/overflow and carry at the byte boundary.
        {0xd2,0xe0,0x80,1,0x00,CF|OF|ZF|PF,MASK},
        {0xd3,0xe0,0x8000,1,0x0000,CF|OF|ZF|PF,MASK},
        // SHR by one: OF equals original sign, CF equals shifted out bit.
        {0xd2,0xe8,0x81,1,0x40,CF|OF, MASK},
        {0xd3,0xe8,0x8001,1,0x4000,CF|OF|PF,MASK},
        // SAR sign preservation; CF is the low bit shifted out.
        {0xd2,0xf8,0x81,1,0xc0,CF|SF|PF,MASK},
        {0xd3,0xf8,0x8001,1,0xc000,CF|SF|PF,MASK},
        // Rotates update CF/OF, but leave ZF/SF/PF untouched.
        {0xd2,0xc0,0x80,1,0x01,CF|OF,CF|OF},
        {0xd2,0xc8,0x01,1,0x80,CF|OF,CF|OF},
        // Multiple-count shift: OF is undefined, so exclude it.
        {0xd2,0xe0,0x01,8,0x00,CF|ZF|PF,CF|ZF|SF|PF},
        {0xd3,0xe8,0x8001,16,0x0000,CF|ZF|PF,CF|ZF|SF|PF},
    };
    for(const auto type:{CpuType::I8088,CpuType::NECV20})
        for(const auto& t:cases)
            if(!check(type,t))return 1;
    return 0;
}
