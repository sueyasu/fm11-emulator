#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
void prepare(Main8088Board& board) {
    // MOV [BX],AX ; NOP ; NOP ; JMP +2 ; NOP ; NOP ; NOP
    // MOV has enough clock budget to prefetch the first two NOPs.
    const BYTE program[]={
        0x89,0x07,0x90,0x90,0xeb,0x02,0x90,0x90,0x90,0x90,0x90
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    std::array<WORD,8> regs{};
    regs[0]=0x1234;
    regs[3]=0x0200;
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    static_cast<Main8088Board::Core&>(board.cpu())
        .setConformanceState(regs,seg,0,2);
}
}
int main() {
    Main8088Board board(CpuType::I8088);
    prepare(board);
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setPrefetchByteQueueEnabled(true);

    cpu.executeOne(); // MOV [BX],AX: two writes leave two refill slots.
    auto snapshot=cpu.prefetchTimingSnapshot();
    if(cpu.lastExecutionCycles()!=18 || cpu.ip()!=2 ||
       snapshot.depth!=2 || snapshot.refillBytes!=2 ||
       snapshot.memoryWriteBytes!=2)return 1;

    // External/DMA-style code mutation after the FIFO captured NOP.
    // The prefetched NOP is still executed, not the new HLT byte.
    board.bus().writeMemory(0x10002,0xf4);
    cpu.executeOne();
    snapshot=cpu.prefetchTimingSnapshot();
    if(cpu.halted() || cpu.lastExecutionCycles()!=3 || cpu.ip()!=3 ||
       snapshot.bufferedBytes!=1 || snapshot.depth!=1)return 2;
    cpu.executeOne();
    if(cpu.halted() || cpu.lastExecutionCycles()!=3 || cpu.ip()!=4 ||
       cpu.prefetchTimingSnapshot().depth!=0)return 3;

    cpu.executeOne(); // JMP rel8: flush the old stream and prefetch target.
    snapshot=cpu.prefetchTimingSnapshot();
    if(cpu.ip()!=8 || snapshot.flushes!=1 || snapshot.depth!=3 ||
       snapshot.refillBytes!=5)return 4;

    // The target was prefetched before this later external write.
    board.bus().writeMemory(0x10008,0xf4);
    cpu.executeOne();
    snapshot=cpu.prefetchTimingSnapshot();
    if(cpu.halted() || cpu.ip()!=9 || snapshot.bufferedBytes!=3 ||
       snapshot.depth!=2)return 5;

    // A reset must discard buffered bytes and instrumentation totals.
    prepare(board);
    if(cpu.prefetchTimingSnapshot().depth!=0 ||
       cpu.prefetchTimingSnapshot().instructionBytes!=0)return 6;
    cpu.setPrefetchByteQueueEnabled(false);
    board.bus().writeMemory(0x10002,0xf4);
    cpu.executeOne();
    cpu.executeOne();
    if(!cpu.halted() || cpu.ip()!=3)return 7; // direct fetch sees HLT

    // A taken JMP +0 still flushes the queue, even though the
    // resulting IP equals the sequential fall-through address.
    Main8088Board sameTarget(CpuType::I8088);
    prepare(sameTarget);
    auto& same=static_cast<Main8088Board::Core&>(sameTarget.cpu());
    sameTarget.bus().writeMemory(0x10000,0xeb);
    sameTarget.bus().writeMemory(0x10001,0x00);
    same.setPrefetchByteQueueEnabled(true);
    same.executeOne();
    auto jump=same.prefetchTimingSnapshot();
    if(same.ip()!=2 || jump.flushes!=1 || jump.depth!=3)return 9;

    // An untaken Jcc +0 does not invalidate the queue, but a
    // taken Jcc +0 must invalidate it.
    Main8088Board taken(CpuType::I8088);
    prepare(taken);
    auto& conditional=static_cast<Main8088Board::Core&>(taken.cpu());
    taken.bus().writeMemory(0x10000,0x72); // JC +0
    taken.bus().writeMemory(0x10001,0x00);
    std::array<WORD,8> initial{};
    initial[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    conditional.setConformanceState(initial,seg,0,3);
    conditional.setPrefetchByteQueueEnabled(true);
    conditional.executeOne();
    if(conditional.ip()!=2 ||
       conditional.prefetchTimingSnapshot().flushes!=1)return 10;
    conditional.setConformanceState(initial,seg,0,2);
    conditional.executeOne();
    if(conditional.ip()!=2 ||
       conditional.prefetchTimingSnapshot().flushes!=0)return 11;

    // The opt-in real FIFO is unavailable for NEC V20.
    Main8088Board v20(CpuType::NECV20);
    prepare(v20);
    auto& vc=static_cast<Main8088Board::Core&>(v20.cpu());
    vc.setPrefetchByteQueueEnabled(true);
    vc.executeOne();
    if(vc.prefetchTimingSnapshot().instructionBytes!=0)return 8;
    return 0;
}
