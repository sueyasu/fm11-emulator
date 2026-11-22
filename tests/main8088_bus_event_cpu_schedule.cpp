#include "Main8088Board.h"
#include <array>
#include <iostream>

int main() {
    Main8088Board board(CpuType::I8088);
    // MOV AX,[BX] ; MOV [BX],AX ; NOP
    const BYTE code[]={0x8b,0x07,0x89,0x07,0x90};
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x10000+i,code[i]);
    board.bus().writeMemory(0x200,0x34);
    board.bus().writeMemory(0x201,0x12);
    std::array<WORD,8> regs{};
    regs[3]=0x200;
    regs[4]=0x8000;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);

    using Status=Intel8088BusEventSchedule::Status;
    if(cpu.busEventScheduleResult().status!=Status::Unspecified)return 1;
    // These are explicitly supplied test offsets, not claimed as
    // measured Intel 8088 micro-operation timing.
    cpu.setBusEventScheduleOffsets({5,9});
    cpu.executeOne();
    auto r=cpu.busEventScheduleResult();
    if(cpu.reg(0)!=0x1234 || cpu.lastExecutionCycles()!=17 ||
       r.status!=Status::Valid || r.operandEvents!=2 ||
       r.prefetchGrants!=2 || r.bus.elapsed!=17 ||
       r.bus.operand!=8 || r.bus.interrupted!=1 ||
       cpu.busEvents().size()!=4)return 2;

    // Missing timing metadata must be rejected without running an
    // invented "data transfers first" schedule.
    cpu.setBusEventScheduleOffsets({3});
    cpu.executeOne();
    r=cpu.busEventScheduleResult();
    if(cpu.lastExecutionCycles()!=18 || r.status!=Status::CountMismatch ||
       r.bus.elapsed!=0 || cpu.busEvents().size()!=4)return 3;

    // The next instruction does not implicitly replay old offsets.
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=3 ||
       cpu.busEventScheduleResult().status!=Status::CountMismatch)return 4;

    // Conformance reset removes any stale scheduling metadata.
    cpu.setConformanceState(regs,segments,0,2);
    if(cpu.busEventScheduleResult().status!=Status::Unspecified)return 5;
    cpu.setBusEventScheduleOffsets({7,11});
    cpu.executeOne();
    r=cpu.busEventScheduleResult();
    if(r.status!=Status::Valid || r.bus.elapsed!=17 ||
       r.bus.operand!=8 || cpu.reg(0)!=0x1234)return 6;
    return 0;
}
