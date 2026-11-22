#include "Main8088Board.h"
#include <array>
#include <iostream>

// Intel 8088 Group 1 immediate-to-r/m (80h-83h):
// register 4 clocks; memory 23+EA clocks for every ALU extension.
// This is an instruction-clock reference test, not a bus trace.
// In particular CMP need not write back to take the documented time.
int main() {
    const BYTE opcodes[]={0x80,0x81,0x82,0x83};
    for(BYTE opcode:opcodes) {
        for(unsigned ext=0;ext<8;++ext) {
            for(unsigned memory=0;memory<2;++memory) {
                Main8088Board board(CpuType::I8088);
                const BYTE modrm=BYTE((memory?0x07:0xc0)|(ext<<3));
                board.bus().writeMemory(0x10000,opcode);
                board.bus().writeMemory(0x10001,modrm);
                board.bus().writeMemory(0x10002,0x01);
                if(opcode==0x81)board.bus().writeMemory(0x10003,0x00);
                board.bus().writeMemory(0x0200,0x03);
                board.bus().writeMemory(0x0201,0x00);
                std::array<WORD,8> regs{};
                regs[0]=3;
                regs[3]=0x0200;
                regs[4]=0x8000;
                const std::array<WORD,4> segments{0,0x1000,0,0};
                auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
                cpu.setConformanceState(regs,segments,0,2);
                cpu.executeOne();
                const unsigned expected=memory?28u:4u; // EA [BX] = 5
                const unsigned expectedIp=opcode==0x81?4u:3u;
                if(cpu.lastExecutionCycles()!=expected ||
                   cpu.ip()!=expectedIp) {
                    std::cerr<<"Group1 opcode="<<std::hex
                        <<unsigned(opcode)<<" modrm="<<unsigned(modrm)
                        <<std::dec<<" expected="<<expected
                        <<" got="<<cpu.lastExecutionCycles()
                        <<" IP="<<cpu.ip()<<"\n";
                    return 1;
                }
            }
        }
    }

    // A segment override adds only the separately modelled prefix
    // overhead; it does not reapply a 16-bit memory bus penalty.
    Main8088Board prefixed(CpuType::I8088);
    const BYTE code[]={0x26,0x83,0x3f,0x01}; // ES CMP word [BX],1
    for(unsigned i=0;i<sizeof(code);++i)
        prefixed.bus().writeMemory(0x10000+i,code[i]);
    std::array<WORD,8> regs{};
    regs[3]=0x0200;regs[4]=0x8000;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(prefixed.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=30 || cpu.ip()!=4)return 2;
    return 0;
}
