#include "Main8088Board.h"
#include <cstdint>
#include <initializer_list>

namespace {
constexpr WORD FLAG_CF=0x0001,FLAG_PF=0x0004,FLAG_AF=0x0010;
constexpr WORD FLAG_ZF=0x0040,FLAG_SF=0x0080,FLAG_OF=0x0800;
constexpr WORD ARITH_FLAGS=FLAG_CF|FLAG_PF|FLAG_AF|FLAG_ZF|FLAG_SF|FLAG_OF;
struct Case {
    BYTE opcode;
    WORD initial;
    WORD immediate;
    WORD result;
    WORD flags;
};
bool runCase(CpuType type,const Case& test)
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(type);
    board.bus().loadBootRom(rom);
    const bool word=(test.opcode&1)!=0;
    const BYTE code[]={
        BYTE(word?0xb8:0xb0),BYTE(test.initial),BYTE(test.initial>>8),
        test.opcode,BYTE(test.immediate),BYTE(test.immediate>>8),0xf4
    };
    const unsigned movSize=word?3u:2u;
    const unsigned immSize=word?2u:1u;
    for(unsigned i=0;i<movSize;++i)board.bus().writeMemory(0x10000+i,code[i]);
    board.bus().writeMemory(0x10000+movSize,test.opcode);
    for(unsigned i=0;i<immSize;++i)
        board.bus().writeMemory(0x10000+movSize+1+i,BYTE(test.immediate>>(8*i)));
    board.bus().writeMemory(0x10000+movSize+1+immSize,0xf4);
    board.reset();board.cpu().executeOne(); // reset far jump
    board.cpu().executeOne(); // MOV AL/AX
    board.cpu().executeOne(); // ALU
    const auto& cpu=static_cast<const Main8088Board::Core&>(board.cpu());
    return (cpu.reg(0)&(word?0xffffu:0xffu))==test.result &&
           (cpu.flags()&ARITH_FLAGS)==test.flags;
}
}
int main()
{
    // All six arithmetic flags, including half carry and odd/even parity.
    // Explicit expected values are independent of the production ALU.
    const Case cases[]={
        {0x04,0x7f,0x01,0x80,FLAG_OF|FLAG_AF|FLAG_SF},
        {0x04,0xff,0x01,0x00,FLAG_CF|FLAG_AF|FLAG_ZF|FLAG_PF},
        {0x04,0x0f,0x01,0x10,FLAG_AF},
        {0x04,0x00,0x00,0x00,FLAG_ZF|FLAG_PF},
        {0x2c,0x00,0x01,0xff,FLAG_CF|FLAG_AF|FLAG_SF|FLAG_PF},
        {0x2c,0x80,0x01,0x7f,FLAG_OF|FLAG_AF},
        {0x2c,0x10,0x01,0x0f,FLAG_AF|FLAG_PF},
        {0x05,0x7fff,0x0001,0x8000,FLAG_OF|FLAG_AF|FLAG_SF|FLAG_PF},
        {0x05,0xffff,0x0001,0x0000,FLAG_CF|FLAG_AF|FLAG_ZF|FLAG_PF},
        {0x2d,0x0000,0x0001,0xffff,FLAG_CF|FLAG_AF|FLAG_SF|FLAG_PF},
        {0x2d,0x8000,0x0001,0x7fff,FLAG_OF|FLAG_AF|FLAG_PF},
    };
    for(CpuType type:{CpuType::I8088,CpuType::NECV20})
        for(const auto& test:cases)
            if(!runCase(type,test))return 1;
    return 0;
}
