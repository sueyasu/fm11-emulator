#include "Main8088Board.h"
#include <array>
#include <iostream>

// Intel 8088 F6/F7 Group 3, ext 0/1 TEST, 2 NOT, 3 NEG:
// reg TEST 5, NOT/NEG 3; memory TEST 11+EA,
// NOT/NEG 24+EA. Word forms use the same published totals.
int main() {
    for(unsigned word=0;word<2;++word) {
        for(unsigned ext=0;ext<4;++ext) {
            for(unsigned memory=0;memory<2;++memory) {
                for(unsigned prefix=0;prefix<2;++prefix) {
                    Main8088Board board(CpuType::I8088);
                    unsigned ip=0;
                    if(prefix)board.bus().writeMemory(0x10000+ip++,0x26);
                    board.bus().writeMemory(0x10000+ip++,BYTE(0xf6+word));
                    const BYTE modrm=BYTE((ext<<3)|(memory?0x07:0xc0));
                    board.bus().writeMemory(0x10000+ip++,modrm);
                    if(ext<=1) {
                        board.bus().writeMemory(0x10000+ip++,0x01);
                        if(word)board.bus().writeMemory(0x10000+ip++,0x00);
                    }
                    board.bus().writeMemory(0x0200,0x03);
                    board.bus().writeMemory(0x0201,0x00);
                    std::array<WORD,8> regs{};
                    regs[0]=3;regs[3]=0x0200;regs[4]=0x8000;
                    const std::array<WORD,4> segments{0,0x1000,0,0};
                    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
                    cpu.setConformanceState(regs,segments,0,2);
                    cpu.executeOne();
                    const unsigned expected=(memory
                        ? (ext<=1?11u:24u)+5u
                        : (ext<=1?5u:3u))+(prefix?2u:0u);
                    if(cpu.lastExecutionCycles()!=expected || cpu.ip()!=ip) {
                        std::cerr<<"F"<<(6+word)<<" /"<<ext
                                 <<" memory="<<memory<<" prefix="<<prefix
                                 <<": expected "<<expected
                                 <<" cycles and IP="<<ip
                                 <<", got "<<cpu.lastExecutionCycles()
                                 <<" and IP="<<cpu.ip()<<"\n";
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
