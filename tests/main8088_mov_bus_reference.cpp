#include "Main8088Board.h"
#include "Intel8088MovBusReference.h"
#include <array>
#include <iostream>

namespace {
using Ref=Intel8088MovBusReference;
using Kind=Intel8088BusEvents::Kind;

bool run(BYTE opcode,BYTE modrm,WORD bx,WORD ax,unsigned clocks,
         unsigned bytes,Kind direction) {
    Main8088Board board(CpuType::I8088);
    board.bus().writeMemory(0x10000,opcode);
    board.bus().writeMemory(0x10001,modrm);
    board.bus().writeMemory(bx,0x56);
    board.bus().writeMemory(bx+1,0x34);
    std::array<WORD,8> regs{};
    regs[0]=ax;
    regs[3]=bx;
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& core=static_cast<Main8088Board::Core&>(board.cpu());
    core.setConformanceState(regs,seg,0,2);
    core.setBusEventTrackingEnabled(true);
    core.executeOne();
    auto result=Ref::validate(opcode,modrm,5,
                              static_cast<unsigned>(core.lastExecutionCycles()),
                              core.busEvents());
    if(result.status!=Ref::Status::Valid ||
       result.expectedClocks!=clocks ||
       result.expectedOperandBytes!=bytes ||
       result.latestFirstTState!=clocks-4*bytes ||
       core.lastExecutionCycles()!=clocks) {
        std::cerr<<"MOV opcode "<<std::hex<<unsigned(opcode)
                 <<": clocks "<<std::dec<<core.lastExecutionCycles()
                 <<", status "<<int(result.status)<<"\n";
        return false;
    }
    if(bytes && core.busEvents()[2].kind!=direction)return false;
    return true;
}
}
int main() {
    // Intel reference: MOV r/m8,r8=9+EA, MOV r8,r/m8=8+EA
    // and 8088 MOV word adds four clocks on the byte-wide bus.
    if(!run(0x88,0x07,0x200,0x1234,14,1,Kind::MemoryWrite))return 1;
    if(!run(0x89,0x07,0x200,0x1234,18,2,Kind::MemoryWrite))return 2;
    if(!run(0x8a,0x07,0x200,0x1234,13,1,Kind::MemoryRead))return 3;
    if(!run(0x8b,0x07,0x200,0x1234,17,2,Kind::MemoryRead))return 4;

    // Register-only MOV does not occupy the external data bus.
    Main8088Board registers(CpuType::I8088);
    registers.bus().writeMemory(0x10000,0x89);
    registers.bus().writeMemory(0x10001,0xd8); // MOV AX,BX
    std::array<WORD,8> regs{};regs[3]=0x200;regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(registers.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    cpu.setBusEventTrackingEnabled(true);
    cpu.executeOne();
    auto result=Ref::validate(0x89,0xd8,0,
                              unsigned(cpu.lastExecutionCycles()),cpu.busEvents());
    if(result.status!=Ref::Status::Valid ||
       result.expectedOperandBytes!=0 || result.expectedClocks!=2)return 5;

    // Reject fabricated timestamps and incorrect bus event metadata.
    auto bad=cpu.busEvents();
    bad.push_back({Kind::MemoryWrite,0x200,0x12,2});
    if(Ref::validate(0x89,0xd8,0,2,bad).status!=
       Ref::Status::EventMismatch)return 6;
    if(Ref::validate(0x89,0xd8,0,3,cpu.busEvents()).status!=
       Ref::Status::ClockMismatch)return 7;
    if(Ref::validate(0x90,0xd8,0,2,cpu.busEvents()).status!=
       Ref::Status::Unsupported)return 8;

    // For a memory word, two ascending byte addresses are required.
    Main8088Board mem(CpuType::I8088);
    mem.bus().writeMemory(0x10000,0x8b);
    mem.bus().writeMemory(0x10001,0x07);
    mem.bus().writeMemory(0x200,0x34);
    mem.bus().writeMemory(0x201,0x12);
    auto& m=static_cast<Main8088Board::Core&>(mem.cpu());
    m.setConformanceState(regs,seg,0,2);
    m.setBusEventTrackingEnabled(true);
    m.executeOne();
    auto events=m.busEvents();
    events[3].address=0x202;
    if(Ref::validate(0x8b,0x07,5,17,events).status!=
       Ref::Status::EventMismatch)return 9;
    return 0;
}
