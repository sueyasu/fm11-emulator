#include "Main8088Board.h"
#include <cstdio>

namespace {
constexpr WORD CF=0x0001, PF=0x0004, AF=0x0010, ZF=0x0040;
constexpr WORD SF=0x0080, OF=0x0800;
constexpr WORD ARITH=CF|PF|AF|ZF|SF|OF;

bool check16(CpuType type, BYTE opcode, WORD lhs, WORD rhs, bool carry,
             WORD expected, WORD expectedFlags)
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;
    rom[0xff1]=0;rom[0xff2]=0;rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(type);
    board.bus().loadBootRom(rom);
    const BYTE code[]={
        0xb8,BYTE(lhs),BYTE(lhs>>8),  // MOV AX,lhs
        BYTE(carry?0xf9:0xf8),       // STC / CLC
        opcode,BYTE(rhs),BYTE(rhs>>8),0xf4
    };
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    board.reset();
    for(unsigned i=0;i<4;++i)board.cpu().executeOne(); // far JMP, MOV, STC/CLC, ADC/SBB
    const auto& cpu=static_cast<const Main8088Board::Core&>(board.cpu());
    if(cpu.reg(0)!=expected || (cpu.flags()&ARITH)!=expectedFlags) {
        std::fprintf(stderr,"16-bit %s opcode=%02X lhs=%04X rhs=%04X carry=%d "
                     "got AX=%04X FLAGS=%04X, expected AX=%04X FLAGS=%04X\n",
                     type==CpuType::I8088?"8088":"V20",unsigned(opcode),
                     unsigned(lhs),unsigned(rhs),int(carry),unsigned(cpu.reg(0)),
                     unsigned(cpu.flags()&ARITH),unsigned(expected),
                     unsigned(expectedFlags));
        return false;
    }
    return true;
}

bool checkPushSp(CpuType type,bool group5,WORD initialSp)
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;
    rom[0xff1]=0;rom[0xff2]=0;rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(type);
    board.bus().loadBootRom(rom);
    // MOV SP,initial ; PUSH SP (54h or FF F4) ; POP AX ; HLT
    const BYTE code[]={
        0xbc,BYTE(initialSp),BYTE(initialSp>>8),
        BYTE(group5?0xff:0x54),
        BYTE(group5?0xf4:0x58),       // FF /6 SP or POP AX
        BYTE(group5?0x58:0xf4),       // POP AX or HLT
        0xf4
    };
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    board.reset();
    board.cpu().executeOne(); // reset far jump
    board.cpu().executeOne(); // MOV SP
    board.cpu().executeOne(); // PUSH SP
    const auto& cpu=static_cast<const Main8088Board::Core&>(board.cpu());
    const WORD decremented=WORD(initialSp-2);
    const auto base=std::uint32_t(decremented);
    const WORD stacked=WORD(board.bus().readRam(base)|
                            (WORD(board.bus().readRam(WORD(base+1)))<<8));
    if(cpu.reg(4)!=decremented || stacked!=decremented) {
        std::fprintf(stderr,"PUSH SP %s group5=%d initial=%04X "
                     "SP=%04X stack=%04X expected=%04X\n",
                     type==CpuType::I8088?"8088":"V20",int(group5),
                     unsigned(initialSp),unsigned(cpu.reg(4)),
                     unsigned(stacked),unsigned(decremented));
        return false;
    }
    board.cpu().executeOne(); // POP AX
    if(cpu.reg(0)!=decremented || cpu.reg(4)!=initialSp)return false;
    return true;
}
}

int main()
{
    for(const auto type:{CpuType::I8088,CpuType::NECV20}) {
        // Word ADC: AF/OF depend on the original operand, and carry wraps.
        if(!check16(type,0x15,0x0000,0x000f,true,0x0010,AF))return 1;
        if(!check16(type,0x15,0x7fff,0x0000,true,0x8000,AF|OF|SF|PF))return 2;
        if(!check16(type,0x15,0x0000,0xffff,true,0x0000,CF|AF|ZF|PF))return 3;
        // Word SBB: the extra borrow affects CF as well as the result.
        if(!check16(type,0x1d,0x0010,0x000f,true,0x0000,AF|ZF|PF))return 4;
        if(!check16(type,0x1d,0x8000,0x0000,true,0x7fff,AF|OF|PF))return 5;
        if(!check16(type,0x1d,0x0000,0x0000,true,0xffff,CF|AF|SF|PF))return 6;
        if(!check16(type,0x1d,0x0000,0xffff,true,0x0000,CF|AF|ZF|PF))return 7;
        // Opcode 54h and the ModR/M register form FF F4 must agree.
        for(const WORD sp:{WORD(0x1000),WORD(0x0000)}) {
            if(!checkPushSp(type,false,sp))return 8;
            if(!checkPushSp(type,true,sp))return 9;
        }
    }
    return 0;
}
