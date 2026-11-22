#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
bool check(const char* label,std::initializer_list<BYTE> instruction,
           unsigned expected,WORD ax=0x0003,WORD flags=2) {
    Main8088Board board(CpuType::I8088);
    unsigned address=0x10000;
    for(BYTE b:instruction)board.bus().writeMemory(address++,b);
    // Isolated memory-operand and stack fixtures.
    board.bus().writeMemory(0x0200,0x52);
    board.bus().writeMemory(0x0201,0x34);
    board.bus().writeMemory(0x8000,0x00);
    board.bus().writeMemory(0x8001,0x10); // POP CS -> 1000h
    std::array<WORD,8> regs{};
    regs[0]=ax;
    regs[3]=0x01fd; // XLAT BX+AL = 0200h when AL=03h
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,flags);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=expected) {
        std::cerr<<label<<": expected "<<expected
                 <<" clocks, got "<<cpu.lastExecutionCycles()<<"\n";
        return false;
    }
    if(instruction.begin()!=instruction.end() &&
       *instruction.begin()==0x0f && cpu.cs()!=0x1000)return false;
    return true;
}
}
int main() {
    // Intel 8088 opcode timings: direct accumulator memory MOV is
    // 14 cycles in both byte and word forms. XLAT is 11 cycles.
    if(!check("MOV AL,moffs",{0xa0,0x00,0x02},14))return 1;
    if(!check("MOV AX,moffs",{0xa1,0x00,0x02},14))return 2;
    if(!check("MOV moffs,AL",{0xa2,0x00,0x02},14))return 3;
    if(!check("MOV moffs,AX",{0xa3,0x00,0x02},14))return 4;
    if(!check("XLAT",{0xd7},11))return 5;
    if(!check("POP CS",{0x0f},12))return 6;
    // Prefix decoding still charges the existing two-clock
    // surcharge; no speculative bus/queue timing is enabled.
    if(!check("ES MOV AL,moffs",{0x26,0xa0,0x00,0x02},16))return 7;
    if(!check("ES XLAT",{0x26,0xd7},13))return 8;

    // The original 8088 accepts 60h-6Fh as conditional branch
    // aliases. A taken zero-displacement branch must flush even
    // though the next IP is identical to the fall-through.
    Main8088Board board(CpuType::I8088);
    board.bus().writeMemory(0x10000,0x62); // undocumented JC +0
    board.bus().writeMemory(0x10001,0x00);
    std::array<WORD,8> regs{};
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,3); // CF=1
    cpu.setPrefetchTrackingEnabled(true);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=16 || cpu.ip()!=2 ||
       cpu.prefetchTimingSnapshot().flushes!=1)return 9;
    cpu.setConformanceState(regs,seg,0,2); // CF=0
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=4 || cpu.ip()!=2 ||
       cpu.prefetchTimingSnapshot().flushes!=0)return 10;
    // JCXZ +0 takes a branch even though IP equals fall-through.
    board.bus().writeMemory(0x10000,0xe3);
    board.bus().writeMemory(0x10001,0x00);
    regs[1]=0; // CX
    cpu.setConformanceState(regs,seg,0,2);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=18 || cpu.ip()!=2 ||
       cpu.prefetchTimingSnapshot().flushes!=1)return 11;
    regs[1]=1;
    cpu.setConformanceState(regs,seg,0,2);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=6 || cpu.ip()!=2 ||
       cpu.prefetchTimingSnapshot().flushes!=0)return 12;
    return 0;
}
