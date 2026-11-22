#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
using Snapshot=Intel8088PrefetchTiming::Snapshot;
bool check(const char* name,const Snapshot& actual,
           unsigned depth,unsigned fetched,unsigned demanded,
           unsigned buffered,unsigned refilled,unsigned flushes) {
    if(actual.depth==depth && actual.instructionBytes==fetched &&
       actual.demandBytes==demanded && actual.bufferedBytes==buffered &&
       actual.refillBytes==refilled && actual.flushes==flushes)return true;
    std::cerr<<name<<": depth "<<actual.depth<<" fetch "<<actual.instructionBytes
             <<" demand "<<actual.demandBytes<<" buffered "<<actual.bufferedBytes
             <<" refill "<<actual.refillBytes<<" flush "<<actual.flushes<<"\n";
    return false;
}
void install(Main8088Board& board) {
    // MOV AX,1234 ; NOP ; JMP +2 ; padding ; NOP at target.
    const BYTE program[]={0xb8,0x34,0x12,0x90,0xeb,0x02,0x90,0x90,0x90};
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    std::array<WORD,8> regs{};
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& core=static_cast<Main8088Board::Core&>(board.cpu());
    core.setConformanceState(regs,seg,0,2);
}
}
int main() {
    Main8088Board board(CpuType::I8088);
    install(board);
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    // Disabled by default: no changes to reference instruction clocks.
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=4 ||
       !check("disabled",cpu.prefetchTimingSnapshot(),0,0,0,0,0,0))return 1;
    install(board);
    cpu.setPrefetchTrackingEnabled(true);
    cpu.executeOne(); // MOV AX,imm16: three demand bytes; one refill slot.
    if(cpu.lastExecutionCycles()!=4 ||
       !check("MOV",cpu.prefetchTimingSnapshot(),1,3,3,0,1,0))return 2;
    cpu.executeOne(); // NOP: consumes buffered slot.
    if(cpu.lastExecutionCycles()!=3 ||
       !check("NOP",cpu.prefetchTimingSnapshot(),0,4,3,1,1,0))return 3;
    cpu.executeOne(); // JMP rel8: changed IP flushes speculative queue.
    if(cpu.lastExecutionCycles()!=15 || cpu.ip()!=8 ||
       !check("JMP",cpu.prefetchTimingSnapshot(),3,6,5,1,4,1))return 4;
    cpu.executeOne(); // NOP at target: served by shadow queue.
    if(cpu.lastExecutionCycles()!=3 ||
       !check("target",cpu.prefetchTimingSnapshot(),2,7,5,2,4,1))return 5;
    // Conformance state resets the diagnostic counters but not opt-in.
    install(board);
    if(!check("reset",cpu.prefetchTimingSnapshot(),0,0,0,0,0,0))return 6;
    cpu.setPrefetchTrackingEnabled(false);
    cpu.executeOne();
    if(!check("disabled again",cpu.prefetchTimingSnapshot(),0,0,0,0,0,0))return 7;
    // NEC V20 must not use the Intel 8088 queue model.
    Main8088Board v20(CpuType::NECV20);
    install(v20);
    auto& vc=static_cast<Main8088Board::Core&>(v20.cpu());
    vc.setPrefetchTrackingEnabled(true);
    vc.executeOne();
    if(!check("V20 unaffected",vc.prefetchTimingSnapshot(),0,0,0,0,0,0))return 8;
    return 0;
}
