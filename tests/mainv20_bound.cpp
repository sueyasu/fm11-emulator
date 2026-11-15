#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <cstdio>

namespace {
constexpr WORD INITIAL_FLAGS=0x0a93;
constexpr WORD ENTRY_CS=0x1000;
constexpr WORD ENTRY_SS=0x5000;
constexpr WORD ENTRY_SP=0x8000;
constexpr WORD VECTOR_CS=0x6600;
constexpr WORD VECTOR_IP=0x1234;

struct Result {
    WORD ax,cs,ip,sp,flags,stackIp,stackCs,stackFlags;
};
Result execute(BYTE opcode,BYTE modrm,WORD index,
               WORD lower,WORD upper,bool useSs=false) {
    Main8088Board board(CpuType::NECV20);
    board.bus().enableConformanceRam();
    auto& bus=board.bus();
    const WORD base=0x0200;
    unsigned address=0x10000;
    if(useSs)bus.writeMemory(address++,0x36);
    bus.writeMemory(address++,opcode);
    bus.writeMemory(address++,modrm);
    bus.writeMemory(address++,BYTE(base));
    bus.writeMemory(address++,BYTE(base>>8));
    const std::uint32_t bounds=(useSs?0x50200u:0x30200u);
    bus.writeMemory(bounds,BYTE(lower));
    bus.writeMemory(bounds+1,BYTE(lower>>8));
    bus.writeMemory(bounds+2,BYTE(upper));
    bus.writeMemory(bounds+3,BYTE(upper>>8));
    // Interrupt vector 5.
    bus.writeMemory(20,BYTE(VECTOR_IP));
    bus.writeMemory(21,BYTE(VECTOR_IP>>8));
    bus.writeMemory(22,BYTE(VECTOR_CS));
    bus.writeMemory(23,BYTE(VECTOR_CS>>8));
    std::array<WORD,8> regs{};
    std::array<WORD,4> segs{};
    regs[0]=index;regs[4]=ENTRY_SP;
    segs[1]=ENTRY_CS;segs[2]=ENTRY_SS;segs[3]=0x3000;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segs,0,INITIAL_FLAGS);
    cpu.executeOne();
    const WORD sp=cpu.reg(4);
    auto readStack=[&](WORD off)->WORD {
        const auto ptr=(0x50000u+off)&0xfffffu;
        return WORD(bus.readRam(ptr)|
                    (WORD(bus.readRam((ptr+1)&0xfffffu))<<8));
    };
    return {cpu.reg(0),cpu.cs(),cpu.ip(),sp,cpu.flags(),
            readStack(sp),readStack(WORD(sp+2)),readStack(WORD(sp+4))};
}
bool check(const char* label,const Result& result,bool expectFault,
           unsigned instructionSize,WORD expectedIndex) {
    const WORD expectedSp=expectFault?WORD(ENTRY_SP-6):ENTRY_SP;
    const WORD expectedCs=expectFault?VECTOR_CS:ENTRY_CS;
    const WORD expectedIp=expectFault?VECTOR_IP:WORD(instructionSize);
    const WORD expectedFlags=expectFault
        ? WORD(INITIAL_FLAGS&~0x0300):INITIAL_FLAGS;
    const bool ok=result.ax==expectedIndex &&
        result.sp==expectedSp && result.cs==expectedCs &&
        result.ip==expectedIp && result.flags==expectedFlags &&
        (!expectFault || (result.stackIp==instructionSize &&
                          result.stackCs==ENTRY_CS &&
                          result.stackFlags==INITIAL_FLAGS));
    if(!ok)std::fprintf(stderr,
        "%s: AX=%04X CS:IP=%04X:%04X SP=%04X FLAGS=%04X "
        "saved IP/CS/FLAGS=%04X/%04X/%04X\n",
        label,unsigned(result.ax),unsigned(result.cs),
        unsigned(result.ip),unsigned(result.sp),
        unsigned(result.flags),unsigned(result.stackIp),
        unsigned(result.stackCs),unsigned(result.stackFlags));
    return ok;
}
}
int main() {
    // In range, including both endpoints, and signed negative bounds.
    for(const WORD index:{WORD(0xfffe),WORD(0xffff),WORD(0),
                           WORD(1),WORD(2)}) {
        const auto r=execute(0x62,0x06,index,0xfffe,2);
        if(!check("62 BOUND in range",r,false,4,index))return 1;
    }
    for(const WORD index:{WORD(0xfffd),WORD(3),WORD(0x7fff),
                           WORD(0x8000)}) {
        const auto r=execute(0x62,0x06,index,0xfffe,2);
        if(!check("62 BOUND exception",r,true,4,index))return 2;
    }
    // The hardware corpus shows 63h is not a BOUND alias and
    // does not push an interrupt frame, even when a 62h BOUND
    // with the same operands would raise INT 5.
    for(const WORD index:{WORD(0xfffd),WORD(0xffff),WORD(0),
                           WORD(1),WORD(3)}) {
        const auto r=execute(0x63,0x06,index,0xfffe,2);
        if(!check("63 nontrapping decode",r,false,4,index))return 3;
    }
    // ModR/M effective-address decoding must still consume an
    // optional segment override and displacement.
    const auto undefinedSs=execute(0x63,0x06,3,0xfffe,2,true);
    if(!check("63 SS override",undefinedSs,false,5,3))return 5;
    // Segment overrides must apply to both bound words.
    const auto ss=execute(0x62,0x06,0,0xfffe,2,true);
    if(!check("62 SS override",ss,false,5,0))return 4;
    return 0;
}
