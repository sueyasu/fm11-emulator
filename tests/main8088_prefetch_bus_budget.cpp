#include "Main8088Board.h"
#include <array>
#include <iostream>

int main() {
    Main8088Board board(CpuType::I8088);
    // MOV [BX],AX ; MOV AX,[BX] ; IN AX,30h ; OUT 30h,AL.
    // On the 8088 each 16-bit memory / IO access reserves two
    // individual byte-wide bus cycles.
    const BYTE program[]={0x89,0x07,0x8b,0x07,0xe5,0x30,0xe6,0x30,0x90};
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    std::array<WORD,8> regs{};
    regs[0]=0x1234;
    regs[3]=0x0200; // BX
    regs[4]=0x8000; // SP
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    cpu.setPrefetchTrackingEnabled(true);

    cpu.executeOne(); // MOV [BX],AX: two memory writes, 18 clocks.
    auto v=cpu.prefetchTimingSnapshot();
    if(cpu.lastExecutionCycles()!=18 || v.memoryWriteBytes!=2 ||
       v.memoryReadBytes!=0 || v.operandBusCycles!=8 ||
       v.depth!=2 || v.refillBytes!=2 || v.demandBytes!=2)return 1;
    cpu.executeOne(); // MOV AX,[BX]: two reads, 17 clocks.
    v=cpu.prefetchTimingSnapshot();
    if(cpu.lastExecutionCycles()!=17 || cpu.reg(0)!=0x1234 ||
       v.memoryReadBytes!=2 || v.memoryWriteBytes!=2 ||
       v.operandBusCycles!=16 || v.depth!=2 ||
       v.refillBytes!=4 || v.bufferedBytes!=2)return 2;
    cpu.executeOne(); // IN AX,30h: two byte IO reads, 14 clocks.
    v=cpu.prefetchTimingSnapshot();
    if(cpu.lastExecutionCycles()!=14 || v.ioReadBytes!=2 ||
       v.ioWriteBytes!=0 || v.operandBusCycles!=24 ||
       v.depth!=1 || v.refillBytes!=5 ||
       v.instructionBytes!=6)return 3;
    cpu.executeOne(); // OUT 30h,AL: one IO write, 14 clocks.
    v=cpu.prefetchTimingSnapshot();
    if(cpu.lastExecutionCycles()!=14 || v.ioWriteBytes!=1 ||
       v.operandBusCycles!=28 || v.depth!=2 ||
       v.refillBytes!=7 || v.instructionBytes!=8 ||
       v.bufferedBytes!=5 || v.demandBytes!=3 || v.flushes!=0)return 4;

    // Disabled tracking must leave CPU semantics and clocks unchanged.
    cpu.setPrefetchTrackingEnabled(false);
    cpu.executeOne(); // NOP
    v=cpu.prefetchTimingSnapshot();
    if(cpu.lastExecutionCycles()!=3 || v.operandBusCycles!=0 ||
       v.memoryWriteBytes!=0 || v.refillBytes!=0)return 5;

    // 8088-only diagnostics; the existing V20 timing remains independent.
    Main8088Board v20(CpuType::NECV20);
    auto& core=static_cast<Main8088Board::Core&>(v20.cpu());
    for(unsigned i=0;i<2;++i)
        v20.bus().writeMemory(0x10000+i,program[i]);
    core.setConformanceState(regs,seg,0,2);
    core.setPrefetchTrackingEnabled(true);
    core.executeOne();
    if(core.prefetchTimingSnapshot().memoryWriteBytes!=0)return 6;
    return 0;
}
