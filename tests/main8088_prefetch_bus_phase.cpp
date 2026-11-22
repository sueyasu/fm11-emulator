#include "Main8088Board.h"
#include <array>
#include <iostream>

int main() {
    Main8088Board board(CpuType::I8088);
    // Four 3-clock NOPs; word store; NOP. The FIFO's speculative
    // bytes must remain coherent with the sequential program.
    const BYTE program[]={0x90,0x90,0x90,0x90,0x89,0x07,0x90,0x90,0x90,0x90};
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    std::array<WORD,8> regs{};
    regs[0]=0x1234;
    regs[3]=0x0200;
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    cpu.setPrefetchBusPhaseEnabled(true);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=3 ||
       cpu.prefetchTimingSnapshot().depth!=0 ||
       cpu.prefetchBusPhaseSnapshot().partial!=3)return 1;
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=3 ||
       cpu.prefetchTimingSnapshot().depth!=1 ||
       cpu.prefetchBusPhaseSnapshot().partial!=2)return 2;
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=3 ||
       cpu.prefetchTimingSnapshot().depth!=1 ||
       cpu.prefetchBusPhaseSnapshot().partial!=1)return 3;
    cpu.executeOne();
    auto shadow=cpu.prefetchTimingSnapshot();
    auto bus=cpu.prefetchBusPhaseSnapshot();
    if(cpu.lastExecutionCycles()!=3 || shadow.depth!=1 ||
       shadow.demandBytes!=2 || shadow.bufferedBytes!=2 ||
       shadow.refillBytes!=3 || bus.partial!=0 ||
       bus.prefetch!=12 || bus.elapsed!=12)return 4;

    // The next opcode was prefetched before the word store; the
    // ModR/M byte must still be fetched from main memory.
    cpu.executeOne();
    shadow=cpu.prefetchTimingSnapshot();
    bus=cpu.prefetchBusPhaseSnapshot();
    if(cpu.lastExecutionCycles()!=18 || cpu.ip()!=6 ||
       shadow.memoryWriteBytes!=2 ||
       shadow.operandBusCycles!=8 || shadow.depth!=2 ||
       shadow.refillBytes!=5 || shadow.demandBytes!=3 ||
       bus.operand!=8 || bus.prefetch!=20 ||
       bus.elapsed!=30 || bus.partial!=2)return 5;
    cpu.executeOne();
    shadow=cpu.prefetchTimingSnapshot();
    bus=cpu.prefetchBusPhaseSnapshot();
    if(cpu.lastExecutionCycles()!=3 || cpu.ip()!=7 ||
       shadow.depth!=2 || shadow.refillBytes!=6 ||
       bus.partial!=1 || bus.prefetch!=24)return 6;

    // This mode must not turn on the earlier demand-stall penalty.
    if(shadow.demandStallCycles!=0)return 7;
    cpu.setPrefetchBusPhaseEnabled(false);
    if(cpu.prefetchBusPhaseSnapshot().elapsed!=0 ||
       cpu.prefetchTimingSnapshot().instructionBytes!=0)return 8;

    Main8088Board v20(CpuType::NECV20);
    v20.bus().writeMemory(0x10000,0x90);
    auto& vc=static_cast<Main8088Board::Core&>(v20.cpu());
    vc.setConformanceState(regs,seg,0,2);
    vc.setPrefetchBusPhaseEnabled(true);
    vc.executeOne();
    if(vc.prefetchBusPhaseSnapshot().elapsed!=0 ||
       vc.prefetchTimingSnapshot().instructionBytes!=0)return 9;
    return 0;
}
