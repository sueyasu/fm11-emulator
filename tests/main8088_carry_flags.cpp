#include "Main8088Board.h"

namespace {
constexpr WORD CF=1,PF=4,AF=0x10,ZF=0x40,SF=0x80,OF=0x800;
constexpr WORD MASK=CF|PF|AF|ZF|SF|OF;
struct Case {BYTE opcode;BYTE initial;BYTE immediate;BYTE result;WORD flags;bool carry;};
bool check(CpuType type,const Case& t)
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(type);
    board.bus().loadBootRom(rom);
    const BYTE code[]={0xb0,t.initial,
        BYTE(t.carry?0xf9:0xf8),t.opcode,t.immediate,0xf4};
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    board.reset();
    board.cpu().executeOne(); // startup JMP
    board.cpu().executeOne(); // MOV AL
    board.cpu().executeOne(); // STC / CLC
    board.cpu().executeOne(); // arithmetic
    const auto& cpu=static_cast<const Main8088Board::Core&>(board.cpu());
    return (cpu.reg(0)&0xff)==t.result &&
           (cpu.flags()&MASK)==t.flags;
}
}
int main()
{
    // Independent expected flag values, including incoming carry for AF.
    const Case cases[]={
        {0x14,0x0f,0x00,0x0f,PF,false},
        {0x14,0x0f,0x00,0x10,AF,true},
        {0x14,0xff,0x00,0x00,CF|AF|ZF|PF,true},
        {0x14,0x7f,0x00,0x80,OF|AF|SF,true},
        {0x1c,0x10,0x00,0x0f,AF|PF,true},
        {0x1c,0x00,0x00,0xff,CF|AF|SF|PF,true},
        {0x1c,0x80,0x00,0x7f,OF|AF,true},
        {0x1c,0x01,0x00,0x01,0,false},
    };
    for(const auto type:{CpuType::I8088,CpuType::NECV20})
        for(const auto& t:cases)
            if(!check(type,t))return 1;

    // INC and DEC change arithmetic flags but do not modify incoming CF.
    for(const auto type:{CpuType::I8088,CpuType::NECV20}) {
        Main8088Bus::Rom rom{};
        rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
        rom[0xff3]=0;rom[0xff4]=0x10;
        Main8088Board board(type);
        board.bus().loadBootRom(rom);
        const BYTE code[]={
            0xb8,0xff,0x7f, // AX=7FFF
            0xf9,           // STC
            0x40,           // INC AX => 8000, OF=1, CF remains 1
            0x48,           // DEC AX => 7FFF, OF=1, CF remains 1
            0xf8,           // CLC
            0x40,           // INC AX => 8000, CF remains 0
            0xf4
        };
        for(unsigned i=0;i<sizeof(code);++i)
            board.bus().writeMemory(0x10000+i,code[i]);
        board.reset();board.cpu().executeOne();
        auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
        board.cpu().executeOne();board.cpu().executeOne();
        board.cpu().executeOne();
        if(cpu.reg(0)!=0x8000 || (cpu.flags()&(CF|OF))!=(CF|OF))return 2;
        board.cpu().executeOne();
        if(cpu.reg(0)!=0x7fff || (cpu.flags()&(CF|OF))!=(CF|OF))return 3;
        board.cpu().executeOne();board.cpu().executeOne();
        if(cpu.reg(0)!=0x8000 || (cpu.flags()&(CF|OF))!=OF)return 4;
    }
    return 0;
}
