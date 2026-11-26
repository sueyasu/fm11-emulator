#include "MonitorDisassembler.h"
#include <array>
#include <cstdint>
#include <string>

using namespace MonitorDisassembler;
int main() {
    std::array<std::uint8_t,1<<20> ram{};
    const Reader read=[&](std::uint32_t addr){return ram[addr&0xfffff];};
    auto check=[&](Architecture arch,std::initializer_list<unsigned> bytes,
                   unsigned size,const std::string& mnemonic)->bool {
        ram.fill(0);
        unsigned i=0;
        for(auto b:bytes)ram[i++]=static_cast<std::uint8_t>(b);
        const auto decoded=decode(arch,read,0);
        return decoded.size==size && decoded.mnemonic==mnemonic;
    };
    if(!check(Architecture::M6809,{0x12},1,"NOP"))return 1;
    if(!check(Architecture::M6809,{0x86,0x34},2,"LDA #$34"))return 2;
    if(!check(Architecture::M6809,{0x8e,0x12,0x34},3,"LDX #$1234"))return 3;
    if(!check(Architecture::M6809,{0x20,0xfe},2,"BRA $0000"))return 4;
    if(!check(Architecture::M6809,{0x10,0x26,0x00,0x08},4,"LBNE $000C"))return 5;
    if(!check(Architecture::M6809,{0xa6,0x88,0x12},3,"LDA $12,X"))return 6;
    if(!check(Architecture::HD6309,{0x12},1,"NOP"))return 7;
    if(!check(Architecture::Z80,{0x00},1,"NOP"))return 8;
    if(!check(Architecture::Z80,{0x21,0x34,0x12},3,"LD HL,$1234"))return 9;
    if(!check(Architecture::Z80,{0xc3,0x34,0x12},3,"JP $1234"))return 10;
    if(!check(Architecture::Z80,{0xdd,0x21,0x34,0x12},4,"LD IX,$1234"))return 11;
    if(!check(Architecture::Z80,{0xcb,0x7e},2,"BIT 7,(HL)"))return 12;
    if(!check(Architecture::I8088,{0x90},1,"NOP"))return 13;
    if(!check(Architecture::I8088,{0xb8,0x34,0x12},3,"MOV AX,$1234"))return 14;
    if(!check(Architecture::V20,{0xcd,0x21},2,"INT $21"))return 15;
    if(!check(Architecture::V20,{0x62},1,"DB $62"))return 16;
    return 0;
}
