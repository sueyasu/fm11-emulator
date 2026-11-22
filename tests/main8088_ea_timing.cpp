#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <iostream>

namespace {
bool check(const char* label, std::array<BYTE,6> bytes, unsigned length,
           std::array<WORD,8> regs, unsigned expected) {
    Main8088Board board(CpuType::I8088);
    std::array<WORD,4> segments{0,0x1000,0,0};
    for(unsigned i=0;i<length;++i)
        board.bus().writeMemory(0x10000+i,bytes[i]);
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,0x0002);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=expected) {
        std::cerr<<label<<": expected "<<expected
                 <<" got "<<cpu.lastExecutionCycles()<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    // Intel 8088 EA table: BX+SI=7, BX+DI=8, BP+SI=8,
    // BP+DI=7, SI/DI/BP/BX=5, direct disp16=6;
    // disp8/disp16 add 4 cycles (except direct disp16).
    const std::array<WORD,8> regs{0x1234,0,0,0x2000,0x4000,
                                   0x2000,0x0010,0x0020};
    const unsigned ea[8]={7,8,8,7,5,5,6,5};
    for(unsigned rm=0;rm<8;++rm) {
        const BYTE modrm=BYTE(rm); // MOV AL,[EA] / MOV AX,[EA]
        const std::array<BYTE,6> byteOp{0x8a,modrm,0x00,0x20,0,0};
        const std::array<BYTE,6> wordOp{0x8b,modrm,0x00,0x20,0,0};
        const unsigned len=rm==6?4:2;
        if(!check("MOV r8,m8",byteOp,len,regs,8+ea[rm]))return 1;
        if(!check("MOV r16,m16",wordOp,len,regs,12+ea[rm]))return 2;
    }
    for(unsigned mod=1;mod<=2;++mod) {
        const BYTE modrm=BYTE((mod<<6)|0); // BX+SI+disp
        const std::array<BYTE,6> mov{0x8a,modrm,0x10,0,0,0};
        if(!check("MOV disp",mov,mod==1?3:4,regs,8+7+4))return 3;
    }
    if(!check("MOV register",{0x8b,0xc3,0,0,0,0},2,regs,2))return 4;
    if(!check("MOV imm8 memory",{0xc6,0x07,0x42,0,0,0},3,regs,10+5))return 5;
    if(!check("MOV imm16 memory",{0xc7,0x07,0x42,0x12,0,0},4,regs,14+5))return 6;
    if(!check("ADD memory,reg",{0x00,0x07,0,0,0,0},2,regs,24+5))return 7;
    if(!check("ADD reg,memory",{0x02,0x07,0,0,0,0},2,regs,13+5))return 8;
    if(!check("ADD memory16,reg16",{0x01,0x07,0,0,0,0},2,regs,24+5))return 9;
    if(!check("CMP memory,reg",{0x38,0x07,0,0,0,0},2,regs,24+5))return 10;
    if(!check("segment override MOV",{0x26,0x8a,0x07,0,0,0},3,regs,2+8+5))return 11;
    if(!check("MOV byte memory destination",{0x88,0x07,0,0,0,0},2,regs,9+5))return 12;
    if(!check("MOV word memory destination",{0x89,0x07,0,0,0,0},2,regs,13+5))return 13;
    if(!check("MOV segment memory destination",{0x8c,0x07,0,0,0,0},2,regs,13+5))return 14;
    if(!check("MOV segment memory source",{0x8e,0x07,0,0,0,0},2,regs,12+5))return 15;
    return 0;
}
