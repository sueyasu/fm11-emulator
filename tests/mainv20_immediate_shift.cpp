#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <cstdio>

namespace {
constexpr WORD FLAGS=0x0293; // CF=1; includes pre-existing OF=0.
constexpr WORD STATUS=0x08c5; // OF SF ZF PF CF

struct State {WORD ax,ip,sp,cs,flags;WORD value;};

State execute(CpuType type,BYTE opcode,unsigned extension,unsigned count,
              bool memoryForm,WORD initial=0x8001) {
    Main8088Board board(type);
    board.bus().enableConformanceRam();
    auto& bus=board.bus();
    constexpr WORD offset=0x0200;
    constexpr std::uint32_t address=0x30200;
    unsigned pc=0x10000;
    bus.writeMemory(pc++,opcode);
    // V20 C0/C1 both use ModR/M followed by imm8.
    bus.writeMemory(pc++,BYTE((memoryForm?0x06:0xc0)|(extension<<3)));
    if(memoryForm) {
        bus.writeMemory(pc++,BYTE(offset));
        bus.writeMemory(pc++,BYTE(offset>>8));
    }
    bus.writeMemory(pc++,BYTE(count));
    bus.writeMemory(address,BYTE(initial));
    bus.writeMemory(address+1,BYTE(initial>>8));
    std::array<WORD,8> regs{};
    std::array<WORD,4> seg{};
    regs[0]=initial;
    regs[4]=0x8000;
    seg[1]=0x1000;seg[2]=0x2200;seg[3]=0x3000;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,FLAGS);
    cpu.executeOne();
    const WORD stored=WORD(bus.readRam(address)|(WORD(bus.readRam(address+1))<<8));
    return {cpu.reg(0),cpu.ip(),cpu.reg(4),cpu.cs(),cpu.flags(),
            memoryForm?stored:cpu.reg(0)};
}
bool check(const char* label,BYTE opcode,unsigned extension,
           unsigned count,bool memoryForm,WORD initial,
           WORD expectedValue,WORD expectedFlags,WORD flagMask) {
    const State s=execute(CpuType::NECV20,opcode,extension,count,memoryForm,initial);
    const WORD widthMask=opcode==0xc0?0x00ff:0xffff;
    const WORD original=memoryForm?initial:initial;
    const WORD expectedAx=memoryForm?initial:
        WORD((original&~widthMask)|(expectedValue&widthMask));
    const WORD expectedMemory=memoryForm?
        WORD((initial&~widthMask)|(expectedValue&widthMask)):expectedAx;
    const WORD length=memoryForm?5:3;
    const bool ok=s.ip==length && s.sp==0x8000 && s.cs==0x1000 &&
       s.ax==expectedAx && s.value==expectedMemory &&
       ((s.flags^expectedFlags)&flagMask)==0;
    if(!ok)std::fprintf(stderr,
        "%s op=%02X /%u imm=%u mem=%d: AX=%04X MEM=%04X IP=%04X "
        "SP=%04X FLAGS=%04X expected AX=%04X MEM=%04X FLAGS=%04X mask=%04X\n",
        label,unsigned(opcode),extension,count,int(memoryForm),
        unsigned(s.ax),unsigned(s.value),unsigned(s.ip),unsigned(s.sp),
        unsigned(s.flags),unsigned(expectedAx),unsigned(expectedMemory),
        unsigned(expectedFlags),unsigned(flagMask));
    return ok;
}
}
int main() {
    // A zero count does not modify any flags or operand (even with CF set).
    for(BYTE opcode:{BYTE(0xc0),BYTE(0xc1)})
        for(unsigned extension:{0u,1u,2u,3u,4u,5u,7u})
            for(bool memoryForm:{false,true}) {
                if(!check("count 0",opcode,extension,0,memoryForm,0x8001,
                          0x8001,FLAGS,0xffff))return 1;
            }

    // Single-count reference results: 81h/8001h, incoming CF=1.
    // OF is defined for count=1. Check the full arithmetic status set.
    constexpr std::array<WORD,8> byteResults={
        0x03,0xc0,0x03,0xc0,0x02,0x40,0x02,0xc0};
    constexpr std::array<WORD,8> wordResults={
        0x0003,0xc000,0x0003,0xc000,
        0x0002,0x4000,0x0002,0xc000};
    for(BYTE opcode:{BYTE(0xc0),BYTE(0xc1)})
        for(unsigned extension:{0u,1u,2u,3u,4u,5u,7u})
            for(bool memoryForm:{false,true}) {
                const bool isWord=opcode==0xc1;
                const WORD before=isWord?0x8001:0x0081;
                const WORD expected=(isWord?wordResults:byteResults)[extension];
                const bool cf=true;
                const bool of=(extension==0||extension==2||
                               extension==4||extension==5);
                const bool sign=(expected&(isWord?0x8000:0x80))!=0;
                const bool zero=expected==0;
                unsigned parityByte=BYTE(expected),ones=0;
                for(unsigned bit=0;bit<8;++bit)ones+=(parityByte>>bit)&1u;
                const bool pf=(ones%2)==0;
                WORD flags=WORD(FLAGS&~STATUS);
                if(cf)flags|=0x0001;
                if(of)flags|=0x0800;
                if(sign && extension>=4)flags|=0x0080;
                if(zero && extension>=4)flags|=0x0040;
                if(pf && extension>=4)flags|=0x0004;
                // Rotates preserve SF/ZF/PF, shifts recalculate them.
                if(extension<4)flags=WORD((flags&~0x00c4)|(FLAGS&0x00c4));
                if(!check("count 1",opcode,extension,1,memoryForm,before,
                          expected,flags,STATUS))return 2;
            }
    // Two-bit counts: exercise actual shift values and final CF, not
    // merely decoding/IP. OF is undefined for counts above one.
    constexpr std::array<WORD,8> byteTwice={
        0x06,0x60,0x07,0xe0,0x04,0x20,0x04,0xe0};
    constexpr std::array<WORD,8> wordTwice={
        0x0006,0x6000,0x0007,0xe000,
        0x0004,0x2000,0x0004,0xe000};
    for(BYTE opcode:{BYTE(0xc0),BYTE(0xc1)})
        for(unsigned extension:{0u,1u,2u,3u,4u,5u,7u})
            for(bool memoryForm:{false,true}) {
                const WORD before=opcode==0xc0?0x0081:0x8001;
                const WORD expected=(opcode==0xc0?byteTwice:wordTwice)[extension];
                // Each of the seven groups shifts out a zero bit on
                // the second iteration, clearing CF.
                if(!check("count 2",opcode,extension,2,memoryForm,before,
                          expected,0,0x0001))return 3;
            }
    return 0;
}
