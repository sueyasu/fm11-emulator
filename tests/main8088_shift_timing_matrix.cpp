#include "Main8088Board.h"
#include <array>
#include <iostream>

// Intel 8088 opcode table D0-D3: single-bit shifts 2/23+EA,
// CL-controlled shifts 8+4n / 28+EA+4n (reg/memory).
// Published 8088 table entries already include word-memory timing.
int main() {
    constexpr unsigned counts[]={0,1,3,31,255};
    for(unsigned extension=0;extension<8;++extension) {
        for(unsigned word=0;word<2;++word) {
            for(unsigned variable=0;variable<2;++variable) {
                for(unsigned memory=0;memory<2;++memory) {
                    for(unsigned value:counts) {
                        if(!variable && value!=0)continue;
                        Main8088Board board(CpuType::I8088);
                        const BYTE opcode=BYTE(0xd0u+2*variable+word);
                        const BYTE modrm=BYTE((memory?0x07u:0xc0u)|
                            (extension<<3u));
                        board.bus().writeMemory(0x10000,opcode);
                        board.bus().writeMemory(0x10001,modrm);
                        board.bus().writeMemory(0x0200,0x81);
                        board.bus().writeMemory(0x0201,0x40);
                        std::array<WORD,8> regs{};
                        regs[0]=0x4081;
                        regs[1]=WORD(value); // CL
                        regs[3]=0x0200; // BX
                        regs[4]=0x8000;
                        const std::array<WORD,4> segments{0,0x1000,0,0};
                        auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
                        cpu.setConformanceState(regs,segments,0,2);
                        cpu.executeOne();
                        const unsigned count=variable?value:1u;
                        const unsigned expected=variable
                            ? (memory?28u+5u+4u*count:
                                      8u+4u*count)
                            : (memory?23u+5u:2u);
                        if(cpu.lastExecutionCycles()!=expected || cpu.ip()!=2) {
                            std::cerr<<"shift/rotate op="<<std::hex
                                <<unsigned(opcode)<<" modrm="<<unsigned(modrm)
                                <<std::dec<<" CL="<<value<<" expected="
                                <<expected<<" got="<<cpu.lastExecutionCycles()
                                <<" IP="<<cpu.ip()<<"\n";
                            return 1;
                        }
                    }
                }
            }
        }
    }
    // A segment override adds the existing 2-cycle prefix surcharge.
    Main8088Board board(CpuType::I8088);
    const BYTE program[]={0x26,0xd1,0x07};
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    std::array<WORD,8> regs{};
    regs[3]=0x0200;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=30 || cpu.ip()!=3)return 2;
    return 0;
}
