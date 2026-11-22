#include "Main8088Board.h"
#include <array>
#include <initializer_list>
#include <iostream>

namespace {
bool check(const char* label, BYTE prefix, BYTE opcode, WORD cx,
           std::initializer_list<unsigned> expected, bool mismatch=false) {
    Main8088Board board(CpuType::I8088);
    std::array<WORD,8> regs{};
    regs[0]=0x0041; // AL = 'A'
    regs[1]=cx;
    regs[4]=0x8000;
    regs[6]=0x0200; // SI
    regs[7]=0x0300; // DI
    const std::array<WORD,4> segments{0,0x1000,0,0};
    const bool rep=prefix!=0;
    board.bus().writeMemory(0x10000,rep?prefix:opcode);
    if(rep)board.bus().writeMemory(0x10001,opcode);
    for(unsigned i=0;i<8;++i) {
        board.bus().writeMemory(0x0200+i,0x41);
        board.bus().writeMemory(0x0300+i,(mismatch && i==1)?0x42:0x41);
    }
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    unsigned n=0;
    for(unsigned clocks:expected) {
        cpu.executeOne();
        if(cpu.lastExecutionCycles()!=clocks) {
            std::cerr<<label<<" iteration "<<n<<": expected "<<clocks
                     <<" got "<<cpu.lastExecutionCycles()<<"\n";
            return false;
        }
        ++n;
    }
    if(rep && cpu.ip()!=2) {
        std::cerr<<label<<": expected to finish at IP=2, got "<<cpu.ip()<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    if(!check("MOVSB",0,0xa4,1,{18}))return 1;
    if(!check("MOVSW",0,0xa5,1,{26}))return 2;
    if(!check("CMPSB",0,0xa6,1,{30}))return 3;
    if(!check("CMPSW",0,0xa7,1,{38}))return 4;
    if(!check("STOSB",0,0xaa,1,{11}))return 5;
    if(!check("STOSW",0,0xab,1,{15}))return 6;
    if(!check("LODSB",0,0xac,1,{16}))return 7;
    if(!check("LODSW",0,0xad,1,{20}))return 8;
    if(!check("SCASB",0,0xae,1,{19}))return 9;
    if(!check("SCASW",0,0xaf,1,{23}))return 10;
    if(!check("REP MOVSB",0xf3,0xa4,3,{26,17,17}))return 11;
    if(!check("REP MOVSW",0xf3,0xa5,2,{34,25}))return 12;
    if(!check("REP STOSB",0xf3,0xaa,3,{19,10,10}))return 13;
    if(!check("REP LODSB",0xf3,0xac,2,{22,13}))return 14;
    if(!check("REPE CMPSB",0xf3,0xa6,3,{39,30,30}))return 15;
    if(!check("REPNE SCASB",0xf2,0xae,3,{24}))return 16;
    if(!check("REPE SCASB mismatch",0xf3,0xae,3,{24,15},true))return 17;
    if(!check("REP CX=0",0xf3,0xa4,0,{9}))return 18;
    return 0;
}
