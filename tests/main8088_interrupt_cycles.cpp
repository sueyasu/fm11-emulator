#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
bool check(const char* label,unsigned actual,unsigned expected) {
    if(actual==expected)return true;
    std::cerr<<label<<": expected "<<expected<<" got "<<actual<<"\n";
    return false;
}
void vector(Main8088Bus& bus,unsigned number,WORD ip=0x0200,WORD cs=0x1000) {
    const unsigned p=number*4;
    bus.writeMemory(p,BYTE(ip));
    bus.writeMemory(p+1,BYTE(ip>>8));
    bus.writeMemory(p+2,BYTE(cs));
    bus.writeMemory(p+3,BYTE(cs>>8));
}
void state(Main8088Board& board,WORD flags) {
    std::array<WORD,8> regs{};
    regs[4]=0x8000;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& core=static_cast<Main8088Board::Core&>(board.cpu());
    core.setConformanceState(regs,segments,0,flags);
}
}
int main() {
    {
        Main8088Board b;
        vector(b.bus(),2);
        state(b,0x0202);
        b.cpu().nmi();
        const auto& core=static_cast<const Main8088Board::Core&>(b.cpu());
        if(!check("NMI entry",core.lastExecutionCycles(),50))return 1;
        if(core.ip()!=0x0200 || core.cs()!=0x1000 || core.reg(4)!=0x7ffa)return 2;
        if(core.flags()&0x0300)return 3;
        b.bus().writeMemory(0x10200,0xcf);
        b.cpu().executeOne();
        if(!check("IRET",core.lastExecutionCycles(),44))return 4;
        if(core.ip()!=0 || core.reg(4)!=0x8000 || !(core.flags()&0x0200))return 5;
    }
    {
        Main8088Board b;
        vector(b.bus(),0x23);
        state(b,0x0202);
        // ICW1, ICW2, ICW4: one PIC with automatic EOI.
        b.bus().writeIo(0,0x13);
        b.bus().writeIo(1,0x20);
        b.bus().writeIo(1,0x03);
        b.bus().requestIrq(3);
        b.cpu().executeOne();
        const auto& core=static_cast<const Main8088Board::Core&>(b.cpu());
        if(!check("INTR entry",core.lastExecutionCycles(),61))return 6;
        if(core.ip()!=0x0200 || core.reg(4)!=0x7ffa)return 7;
    }
    {
        Main8088Board b;
        state(b,2);
        b.bus().writeMemory(0x10000,0xce);
        b.cpu().executeOne();
        if(!check("INTO not taken",b.cpu().lastExecutionCycles(),4))return 8;
        if(static_cast<const Main8088Board::Core&>(b.cpu()).ip()!=1)return 9;
    }
    {
        Main8088Board b;
        vector(b.bus(),4);
        state(b,0x0802);
        b.bus().writeMemory(0x10000,0xce);
        b.cpu().executeOne();
        if(!check("INTO taken",b.cpu().lastExecutionCycles(),73))return 10;
        const auto& core=static_cast<const Main8088Board::Core&>(b.cpu());
        if(core.ip()!=0x0200 || core.reg(4)!=0x7ffa)return 11;
    }
    {
        Main8088Board b;
        vector(b.bus(),3);
        state(b,2);
        b.bus().writeMemory(0x10000,0xcc);
        b.cpu().executeOne();
        if(!check("INT3",b.cpu().lastExecutionCycles(),72))return 12;
    }
    {
        Main8088Board b;
        vector(b.bus(),0x21);
        state(b,2);
        b.bus().writeMemory(0x10000,0xcd);
        b.bus().writeMemory(0x10001,0x21);
        b.cpu().executeOne();
        if(!check("INT imm8",b.cpu().lastExecutionCycles(),71))return 13;
    }
    return 0;
}
