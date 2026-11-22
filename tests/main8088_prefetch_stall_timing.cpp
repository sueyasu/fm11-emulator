#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
using Core=Main8088Board::Core;
void install(Main8088Board& board,const std::array<BYTE,16>& bytes) {
    for(unsigned i=0;i<bytes.size();++i)
        board.bus().writeMemory(0x10000+i,bytes[i]);
    std::array<WORD,8> regs{};
    regs[0]=0x1234;
    regs[3]=0x0200;
    regs[4]=0x8000;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    static_cast<Core&>(board.cpu()).setConformanceState(regs,segments,0,2);
}
bool expect(const char* label,const Core& cpu,unsigned clocks,
            unsigned depth,unsigned demand,unsigned stall) {
    const auto v=cpu.prefetchTimingSnapshot();
    if(cpu.lastExecutionCycles()==clocks && v.depth==depth &&
       v.demandBytes==demand && v.demandStallCycles==stall)return true;
    std::cerr<<label<<": got cycles "<<cpu.lastExecutionCycles()
             <<" depth "<<v.depth<<" demand "<<v.demandBytes
             <<" stall "<<v.demandStallCycles
             <<"; expected "<<clocks<<","<<depth<<","
             <<demand<<","<<stall<<"\n";
    return false;
}
}
int main() {
    // MOV AX,imm16 ; NOP ; NOP ; JMP +0 ; NOP
    const std::array<BYTE,16> seq={
        0xb8,0x34,0x12,0x90,0x90,0xeb,0x00,0x90,
        0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90
    };
    Main8088Board board(CpuType::I8088);
    install(board,seq);
    auto& cpu=static_cast<Core&>(board.cpu());
    cpu.setPrefetchStallTimingEnabled(true);
    cpu.executeOne(); // three misses: 4 reference + 3*4 demand
    if(!expect("MOV immediate",cpu,16,1,3,12))return 1;
    cpu.executeOne(); // buffered NOP, no additional demand
    if(!expect("buffered NOP",cpu,3,0,3,12))return 2;
    cpu.executeOne(); // empty FIFO => one 4-clock demand
    if(!expect("empty FIFO NOP",cpu,7,0,4,16))return 3;
    cpu.executeOne(); // JMP +0 => 2 misses, flush, refill 3 slots
    if(!expect("JMP +0",cpu,23,3,6,24))return 4;
    cpu.executeOne(); // buffered target NOP; no new penalty
    if(!expect("jump target",cpu,3,2,6,24))return 5;

    // FIFO-only mode preserves the previously tested instruction
    // clocks, even when it must fetch instruction bytes from RAM.
    install(board,seq);
    cpu.setPrefetchByteQueueEnabled(true);
    cpu.executeOne();
    if(!expect("FIFO only",cpu,4,1,3,0))return 6;

    // Passive shadow remains a pure observer.
    install(board,seq);
    cpu.setPrefetchTrackingEnabled(true);
    cpu.executeOne();
    if(!expect("passive",cpu,4,1,3,0))return 7;

    // Memory transfers are already included in Intel's execution
    // reference clock total; only *instruction* misses add stalls.
    const std::array<BYTE,16> memory={
        0x89,0x07,0x90,0x90,0x90,0x90,0x90,0x90,
        0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90
    };
    install(board,memory);
    cpu.setPrefetchStallTimingEnabled(true);
    cpu.executeOne(); // 18 reference + 2 instruction-byte misses * 4
    auto v=cpu.prefetchTimingSnapshot();
    if(!expect("MOV memory",cpu,26,2,2,8) ||
       v.memoryWriteBytes!=2 || v.operandBusCycles!=8 ||
       v.refillBytes!=2)return 8;

    // V20 keeps its own timing/fetch pipeline.
    Main8088Board v20(CpuType::NECV20);
    install(v20,seq);
    auto& vc=static_cast<Core&>(v20.cpu());
    vc.setPrefetchStallTimingEnabled(true);
    vc.executeOne();
    if(vc.prefetchTimingSnapshot().instructionBytes!=0 ||
       vc.prefetchTimingSnapshot().demandStallCycles!=0)return 9;
    return 0;
}
