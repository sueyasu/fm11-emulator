#include "Main8088Board.h"
#include <array>
#include <cstdio>

namespace {
constexpr std::array<WORD,4> initialSegments={
    WORD(0x1111),WORD(0x1000),WORD(0x2222),WORD(0x3333)
};

bool check(BYTE opcode,unsigned field,bool memoryForm) {
    Main8088Board board(CpuType::I8088);
    board.bus().enableConformanceRam();
    const WORD address=0x2000; // effective offset (disp16)
    // ModR/M mod=00, r/m=110 uses DS:disp16, not physical disp16.
    // ES/CS/SS/DS are deliberately distinct to catch segment mistakes.
    const std::uint32_t physicalAddress=
        (std::uint32_t(initialSegments[3])*16u+address)&0xfffffu;
    const BYTE modrm=BYTE((memoryForm?0x06:0xc0)|(field<<3));
    board.bus().writeMemory(0x10000,opcode);
    board.bus().writeMemory(0x10001,modrm);
    if(memoryForm) {
        board.bus().writeMemory(0x10002,BYTE(address));
        board.bus().writeMemory(0x10003,BYTE(address>>8));
    }
    std::array<WORD,8> regs{};
    regs[0]=0x4444; // AX, source for register-form 8E
    regs[4]=0x8000;
    auto segments=initialSegments;
    if(opcode==0x8e && memoryForm) {
        board.bus().writeMemory(physicalAddress,0x88);
        board.bus().writeMemory(physicalAddress+1,0x77);
    }
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    const WORD flags=0x0297;
    cpu.setConformanceState(regs,segments,0,flags);
    cpu.executeOne();

    const unsigned which=field&3u;
    const WORD expectedSource=memoryForm?WORD(0x7788):WORD(0x4444);
    WORD observed=0, expected=0;
    if(opcode==0x8c) {
        expected=initialSegments[which];
        if(memoryForm)
            observed=WORD(board.bus().readRam(physicalAddress)|
                           (WORD(board.bus().readRam(physicalAddress+1))<<8));
        else observed=cpu.reg(0);
    } else {
        expected=expectedSource;
        observed=cpu.segment(which);
    }
    const WORD expectedIp=memoryForm?4:2;
    if(observed==expected && cpu.ip()==expectedIp &&
       cpu.flags()==flags && (opcode!=0x8c || cpu.cs()==0x1000))
        return true;

    std::fprintf(stderr,
                 "opcode=%02X /%u %s: actual=%04X expected=%04X "
                 "IP=%04X expected IP=%04X FLAGS=%04X\n",
                 unsigned(opcode),field,memoryForm?"memory":"register",
                 unsigned(observed),unsigned(expected),
                 unsigned(cpu.ip()),unsigned(expectedIp),
                 unsigned(cpu.flags()));
    return false;
}
}

int main() {
    for(const BYTE opcode:{BYTE(0x8c),BYTE(0x8e)})
        for(unsigned field=0;field<8;++field)
            for(bool memoryForm:{false,true})
                if(!check(opcode,field,memoryForm))return 1;
    return 0;
}
