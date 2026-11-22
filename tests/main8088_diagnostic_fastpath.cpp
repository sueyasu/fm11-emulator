#include "Main8088Board.h"
#include <array>
#include <iostream>

// The default non-diagnostic execution path must produce the same
// clocks and architectural results as the opt-in tracing paths.
namespace {
struct Result {
    std::array<unsigned,4> clocks{};
    std::array<WORD,4> ips{};
    WORD ax=0;
    unsigned fetches=0;
    unsigned dataReads=0;
    unsigned flushes=0;
    bool eventsObserved=false;
};
Result run(bool prefetch,bool events) {
    Main8088Board board(CpuType::I8088);
    // MOV AX,[BX]; ADD AX,1; JMP +0; NOP
    const BYTE program[]={0x8b,0x07,0x05,0x01,0x00,0xeb,0x00,0x90};
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    board.bus().writeMemory(0x0200,0x34);
    board.bus().writeMemory(0x0201,0x12);
    std::array<WORD,8> regs{};
    regs[3]=0x0200;
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    cpu.setPrefetchTrackingEnabled(prefetch);
    cpu.setBusEventTrackingEnabled(events);
    Result result{};
    for(unsigned i=0;i<4;++i) {
        cpu.executeOne();
        result.clocks[i]=unsigned(cpu.lastExecutionCycles());
        result.ips[i]=cpu.ip();
        if(events && !cpu.busEvents().empty())result.eventsObserved=true;
        if(!events && !cpu.busEvents().empty())
            result.eventsObserved=true;
    }
    result.ax=cpu.reg(0);
    const auto snapshot=cpu.prefetchTimingSnapshot();
    result.fetches=unsigned(snapshot.instructionBytes);
    result.dataReads=unsigned(snapshot.memoryReadBytes);
    result.flushes=unsigned(snapshot.flushes);
    return result;
}
}
int main() {
    const auto plain=run(false,false);
    if(plain.clocks!=std::array<unsigned,4>{17,4,15,3} ||
       plain.ips!=std::array<WORD,4>{2,5,7,8} ||
       plain.ax!=0x1235 || plain.fetches || plain.dataReads ||
       plain.flushes || plain.eventsObserved)return 1;
    for(unsigned prefetch=0;prefetch<2;++prefetch) {
        for(unsigned events=0;events<2;++events) {
            const auto other=run(prefetch!=0,events!=0);
            if(other.clocks!=plain.clocks || other.ips!=plain.ips ||
               other.ax!=plain.ax ||
               other.eventsObserved!=(events!=0)) {
                std::cerr<<"opt-in path mismatch: prefetch="<<prefetch
                         <<" events="<<events<<"\n";
                return 2;
            }
            if(prefetch) {
                if(other.fetches!=8 || other.dataReads!=2 ||
                   other.flushes==0)return 3;
            } else if(other.fetches || other.dataReads ||
                      other.flushes)return 4;
        }
    }
    return 0;
}
