#include "Main8088Board.h"
#include <array>
#include <iostream>

// Exercise every primary ALU ModR/M family: ADD, OR, ADC, SBB,
// AND, SUB, XOR, CMP. Memory forms use the documented direction-based 8088
// totals; the word bus cost is already included in those values.
// BX addressing has EA=5. No optional FIFO or T-state scheduler.
int main() {
    const unsigned families[]={0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38};
    for(unsigned family:families) {
        for(unsigned direction=0;direction<2;++direction) {
            for(unsigned word=0;word<2;++word) {
                for(unsigned memory=0;memory<2;++memory) {
                    Main8088Board board(CpuType::I8088);
                    const BYTE opcode=BYTE(family|direction*2|word);
                    const BYTE modrm=BYTE(memory?0x07:0xc3);
                    board.bus().writeMemory(0x10000,opcode);
                    board.bus().writeMemory(0x10001,modrm);
                    board.bus().writeMemory(0x200,0x01);
                    board.bus().writeMemory(0x201,0x00);
                    std::array<WORD,8> regs{};
                    regs[0]=0x0102;
                    regs[3]=0x0200;
                    regs[4]=0x8000;
                    const std::array<WORD,4> segments{0,0x1000,0,0};
                    auto& core=static_cast<Main8088Board::Core&>(board.cpu());
                    core.setConformanceState(regs,segments,0,2);
                    core.executeOne();
                    const unsigned expected=!memory?3u:
                        (direction?13u:24u)+5u;
                    if(core.lastExecutionCycles()!=expected || core.ip()!=2) {
                        std::cerr<<"ALU opcode "<<std::hex<<unsigned(opcode)
                            <<" modrm "<<unsigned(modrm)<<std::dec
                            <<": expected "<<expected<<" got "
                            <<core.lastExecutionCycles()<<"\n";
                        return 1;
                    }
                }
            }
        }
    }
    // Check prefix surcharge without changing the underlying
    // direction and transfer-width classification.
    Main8088Board board(CpuType::I8088);
    const BYTE p[]={0x26,0x01,0x07}; // ES: ADD [BX],AX (word RMW)
    for(unsigned i=0;i<sizeof(p);++i)
        board.bus().writeMemory(0x10000+i,p[i]);
    std::array<WORD,8> regs{};
    regs[3]=0x200;
    regs[4]=0x8000;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=31 || cpu.ip()!=3)return 2;
    return 0;
}
