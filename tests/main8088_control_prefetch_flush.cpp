#include "Main8088Board.h"
#include <array>
#include <initializer_list>
#include <iostream>

// A control transfer flushes even if the destination happens to equal
// the fall-through address (or MOV CS reloads the current CS). This
// only affects the optional 8088 BIU shadow / FIFO diagnostics.
namespace {
struct Case {
    const char* name;
    std::initializer_list<BYTE> code;
    WORD targetIp;
    WORD targetCs;
    unsigned clocks;
};
bool check(const Case& test) {
    Main8088Board board(CpuType::I8088);
    std::uint32_t addr=0x10000;
    for(BYTE value:test.code)board.bus().writeMemory(addr++,value);
    std::array<WORD,8> regs{};
    regs[0]=0x0010; // AX: MOV CS,AX
    regs[3]=0x0200; // BX points to indirect branch target
    regs[4]=0x8000; // SP
    const std::array<WORD,4> segments{0,0x1000,0,0};
    // Indirect target: 1000:0010
    board.bus().writeMemory(0x0200,0x10);
    board.bus().writeMemory(0x0201,0x00);
    board.bus().writeMemory(0x0202,0x00);
    board.bus().writeMemory(0x0203,0x10);
    // Return frame: IP=0010, CS=1000, FLAGS=0002
    const BYTE frame[]={0x10,0x00,0x00,0x10,0x02,0x00};
    for(unsigned i=0;i<sizeof(frame);++i)
        board.bus().writeMemory(0x8000+i,frame[i]);
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    cpu.setPrefetchTrackingEnabled(true);
    cpu.executeOne();
    if(cpu.ip()!=test.targetIp || cpu.cs()!=test.targetCs ||
       cpu.lastExecutionCycles()!=test.clocks ||
       cpu.prefetchTimingSnapshot().flushes!=1) {
        std::cerr<<test.name<<": CS:IP="<<std::hex
                 <<cpu.cs()<<":"<<cpu.ip()<<std::dec
                 <<" clocks="<<cpu.lastExecutionCycles()
                 <<" flushes="<<cpu.prefetchTimingSnapshot().flushes<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    const Case tests[]={
        {"MOV CS,AX",{0x8e,0xc8},2,0x0010,2},
        {"MOV CS,[BX]",{0x8e,0x0f},2,0x0010,17},
        {"POP CS",{0x0f},1,0x0010,12},
        {"CALL far ptr",{0x9a,0x10,0x00,0x00,0x10},0x10,0x1000,36},
        {"CALL near [BX]",{0xff,0x17},0x10,0x1000,34},
        {"CALL far [BX]",{0xff,0x1f},0x10,0x1000,58},
        {"JMP near AX",{0xff,0xe0},0x10,0x1000,11},
        {"JMP far [BX]",{0xff,0x2f},0x10,0x1000,29},
        {"RET",{0xc3},0x10,0x1000,20},
        {"RETF",{0xcb},0x10,0x1000,33},
        {"RET imm",{0xc2,0x02,0x00},0x10,0x1000,24},
        {"RETF imm",{0xca,0x02,0x00},0x10,0x1000,34},
        {"RET alias C1",{0xc1},0x10,0x1000,20},
        {"RET imm alias C0",{0xc0,0x02,0x00},0x10,0x1000,24},
        {"RETF alias C9",{0xc9},0x10,0x1000,33},
        {"RETF imm alias C8",{0xc8,0x02,0x00},0x10,0x1000,34},
        {"IRET",{0xcf},0x10,0x1000,44}
    };
    for(const auto& t:tests)if(!check(t))return 1;
    // An ordinary non-control MOV must not flush the queue.
    Main8088Board b(CpuType::I8088);
    b.bus().writeMemory(0x10000,0x8e);
    b.bus().writeMemory(0x10001,0xd8); // MOV DS,AX
    std::array<WORD,8> regs{};
    regs[0]=0x0010;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(b.cpu());
    cpu.setConformanceState(regs,seg,0,2);
    cpu.setPrefetchTrackingEnabled(true);
    cpu.executeOne();
    if(cpu.prefetchTimingSnapshot().flushes!=0 ||
       cpu.lastExecutionCycles()!=2 || cpu.segment(3)!=0x10)return 2;
    return 0;
}
