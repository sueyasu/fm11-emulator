#include "Main8088Board.h"
#include <array>
#include <iostream>
#include <stdexcept>

// Intel 8088 undocumented opcode D6h (SALC).
// AL is set from the carry flag, AH/FLAGS are unchanged,
// and the opcode takes two instruction clocks.
namespace {
bool check(const char* name,WORD ax,WORD flags,
           bool prefix,WORD expected) {
    Main8088Board board(CpuType::I8088);
    if(prefix)board.bus().writeMemory(0x10000,0x26);
    board.bus().writeMemory(0x10000+(prefix?1:0),0xd6);
    std::array<WORD,8> regs{};
    regs[0]=ax;
    regs[4]=0x8000;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,flags);
    cpu.executeOne();
    const unsigned clocks=prefix?4u:2u;
    if(cpu.reg(0)!=expected || cpu.flags()!=flags ||
       cpu.ip()!=(prefix?2u:1u) ||
       cpu.lastExecutionCycles()!=clocks) {
        std::cerr<<name<<": AX="<<std::hex<<cpu.reg(0)
                 <<" FLAGS="<<cpu.flags()<<std::dec
                 <<" clocks="<<cpu.lastExecutionCycles()
                 <<" IP="<<cpu.ip()<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    if(!check("CF clear",0x12ab,0x0002,false,0x1200))return 1;
    if(!check("CF set",0x34ab,0x0003,false,0x34ff))return 2;
    if(!check("FLAGS unchanged",0xab01,0x0ed7,false,0xabff))return 3;
    if(!check("prefix no carry",0x5678,0x0ed6,true,0x5600))return 4;
    if(!check("prefix carry",0x9a78,0x0ed7,true,0x9aff))return 5;
    // Avoid making an unsupported assumption about V20's D6h decoding.
    Main8088Board v20(CpuType::NECV20);
    v20.bus().writeMemory(0x10000,0xd6);
    std::array<WORD,8> regs{};
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(v20.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    try {
        cpu.executeOne();
    } catch(const std::runtime_error&) {
        return 0; // V20 behavior intentionally not changed.
    }
    std::cerr<<"unexpected V20 SALC acceptance\n";
    return 6;
}
