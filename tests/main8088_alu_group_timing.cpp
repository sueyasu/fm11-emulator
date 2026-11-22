#include "Main8088Board.h"
#include <array>
#include <iostream>

struct Case {
    const char* label;
    std::array<BYTE,8> bytes;
    unsigned length;
    unsigned expected;
};

int main() {
    const Case cases[]={
        {"ADD AL,imm", {0x80,0xc0,0x01},3,4},
        {"ADD mem8,imm", {0x80,0x07,0x01},3,28},
        {"ADD mem16,imm", {0x81,0x07,0x01,0x00},4,28},
        {"CMP AL,imm", {0x80,0xf8,0x01},3,4},
        {"CMP mem8,imm", {0x80,0x3f,0x01},3,28},
        {"CMP mem16,imm", {0x81,0x3f,0x01,0x00},4,28},
        {"ADD mem16,imm8", {0x83,0x07,0x01},3,28},
        {"CMP mem16,imm8", {0x83,0x3f,0x01},3,28},
        {"NOT AL", {0xf6,0xd0},2,3},
        {"NOT mem8", {0xf6,0x17},2,29},
        {"NOT mem16", {0xf7,0x17},2,29},
        {"NEG AX", {0xf7,0xd8},2,3},
        {"NEG mem16", {0xf7,0x1f},2,29},
        {"TEST AL,imm", {0xf6,0xc0,0x01},3,5},
        {"TEST AX,imm", {0xf7,0xc0,0x01,0x00},4,5},
        {"TEST mem8,imm", {0xf6,0x07,0x01},3,16},
        {"TEST mem16,imm", {0xf7,0x07,0x01,0x00},4,16},
        {"TEST mem16,imm alias", {0xf7,0x0f,0x01,0x00},4,16}
    };
    for(const auto& test:cases) {
        Main8088Board board(CpuType::I8088);
        std::array<WORD,8> regs{};
        regs[0]=1;
        regs[3]=0x0200;
        regs[4]=0x8000;
        const std::array<WORD,4> seg{0,0x1000,0,0};
        for(unsigned i=0;i<test.length;++i)
            board.bus().writeMemory(0x10000+i,test.bytes[i]);
        board.bus().writeMemory(0x0200,1);
        auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
        cpu.setConformanceState(regs,seg,0,2);
        cpu.executeOne();
        if(cpu.lastExecutionCycles()!=test.expected) {
            std::cerr<<test.label<<": expected "<<test.expected
                     <<" got "<<cpu.lastExecutionCycles()<<"\n";
            return 1;
        }
    }
    return 0;
}
