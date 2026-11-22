#include "Main8088Board.h"
#include <array>
#include <iostream>

namespace {
using Kind=Intel8088BusEvents::Kind;
using Event=Intel8088BusEvents::Event;
bool equal(const std::vector<Event>& actual,
           std::initializer_list<Event> expected,const char* label) {
    if(actual.size()!=expected.size()) {
        std::cerr<<label<<": events "<<actual.size()
                 <<", expected "<<expected.size()<<"\n";
        return false;
    }
    unsigned i=0;
    for(const auto& e:expected) {
        const auto& got=actual[i];
        if(got.kind!=e.kind || got.address!=e.address ||
           got.value!=e.value || got.order!=i) {
            std::cerr<<label<<" event "<<i<<": kind="
                <<static_cast<int>(got.kind)<<" addr="<<got.address
                <<" value="<<unsigned(got.value)<<"\n";
            return false;
        }
        ++i;
    }
    return true;
}
}
int main() {
    Main8088Board board(CpuType::I8088);
    // MOV AX,[BX] ; MOV [BX],AX ; OUT 30h,AL ; PUSH AX
    const BYTE program[]={0x8b,0x07,0x89,0x07,0xe6,0x30,0x50};
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    board.bus().writeMemory(0x0200,0x34);
    board.bus().writeMemory(0x0201,0x12);
    std::array<WORD,8> regs{};
    regs[3]=0x0200; // BX
    regs[4]=0x8000; // SP
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    cpu.setBusEventTrackingEnabled(true);
    cpu.executeOne();
    if(!equal(cpu.busEvents(),{
        {Kind::Fetch,0x10000,0x8b,0},
        {Kind::Fetch,0x10001,0x07,1},
        {Kind::MemoryRead,0x0200,0x34,2},
        {Kind::MemoryRead,0x0201,0x12,3}
    },"MOV load") || cpu.reg(0)!=0x1234)return 1;

    cpu.executeOne();
    if(!equal(cpu.busEvents(),{
        {Kind::Fetch,0x10002,0x89,0},
        {Kind::Fetch,0x10003,0x07,1},
        {Kind::MemoryWrite,0x0200,0x34,2},
        {Kind::MemoryWrite,0x0201,0x12,3}
    },"MOV store"))return 2;
    cpu.executeOne();
    if(!equal(cpu.busEvents(),{
        {Kind::Fetch,0x10004,0xe6,0},
        {Kind::Fetch,0x10005,0x30,1},
        {Kind::PortWrite,0x30,0x34,2}
    },"OUT AL"))return 3;
    cpu.executeOne();
    if(!equal(cpu.busEvents(),{
        {Kind::Fetch,0x10006,0x50,0},
        {Kind::MemoryWrite,0x7ffe,0x34,1},
        {Kind::MemoryWrite,0x7fff,0x12,2}
    },"PUSH AX"))return 4;

    // Tests can switch off instrumentation independently of the
    // passive prefetch timing counters; normal execution is unaffected.
    cpu.setBusEventTrackingEnabled(false);
    cpu.setConformanceState(regs,segments,0,2);
    cpu.executeOne();
    if(!cpu.busEvents().empty() || cpu.reg(0)!=0x1234)return 5;
    cpu.setBusEventTrackingEnabled(true);
    cpu.setConformanceState(regs,segments,0,2);
    if(!cpu.busEvents().empty())return 6;

    // Preserve the existing CPU behavior and V20 timing model.
    Main8088Board v20(CpuType::NECV20);
    v20.bus().writeMemory(0x10000,0x90);
    auto& vc=static_cast<Main8088Board::Core&>(v20.cpu());
    vc.setConformanceState(regs,segments,0,2);
    vc.setBusEventTrackingEnabled(true);
    vc.executeOne();
    if(!vc.busEvents().empty())return 7;
    return 0;
}
