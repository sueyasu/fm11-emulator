#include "Main8088Board.h"
#include <array>
#include <cstdio>

// V20 native aliases and nibble rotates, including operand=AL overlap.
// Test all ModR/M.reg extensions and both register/memory operands.
namespace {
constexpr WORD FLAGS=0x0297;
struct Result {WORD ax,sp,ip,flags; BYTE memory;WORD stack;};
Result run(BYTE op1,BYTE op2,BYTE modrm,WORD ax,WORD sp,WORD imm) {
    Main8088Board board(CpuType::NECV20);
    board.bus().enableConformanceRam();
    auto& bus=board.bus();
    const bool nibble=op1==0x0f;
    const bool mov=op1==0xc6 || op1==0xc7;
    const bool memory=(modrm&0xc0)!=0xc0;
    const WORD offset=0x0200;
    unsigned pos=0;
    bus.writeMemory(0x10000+pos++,op1);
    if(nibble)bus.writeMemory(0x10000+pos++,op2);
    bus.writeMemory(0x10000+pos++,modrm);
    if(memory) {
        bus.writeMemory(0x10000+pos++,BYTE(offset));
        bus.writeMemory(0x10000+pos++,BYTE(offset>>8));
    }
    if(mov) {
        bus.writeMemory(0x10000+pos++,BYTE(imm));
        if(op1==0xc7)bus.writeMemory(0x10000+pos++,BYTE(imm>>8));
    }
    const std::uint32_t memoryAddress=0x30200;
    bus.writeMemory(memoryAddress,BYTE(0x95));
    bus.writeMemory(memoryAddress+1,0xa5);
    std::array<WORD,8> regs{};
    std::array<WORD,4> seg{};
    regs[0]=ax;regs[4]=sp;
    seg[1]=0x1000;seg[2]=0x2200;seg[3]=0x3000;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,FLAGS);
    cpu.executeOne();
    const auto stackAddr=(0x22000u+WORD(sp-2))&0xfffffu;
    const WORD stacked=WORD(bus.readRam(stackAddr) |
                            (WORD(bus.readRam((stackAddr+1)&0xfffffu))<<8));
    return {cpu.reg(0),cpu.reg(4),cpu.ip(),cpu.flags(),
            bus.readRam(memoryAddress),stacked};
}
bool verify(bool good,const char* category,unsigned ext,
            const Result& r,WORD expected) {
    if(good)return true;
    std::fprintf(stderr,"%s /%u: AX=%04X SP=%04X IP=%04X FLAGS=%04X "
                        "MEM=%02X STACK=%04X expected=%04X\n",
                 category,ext,unsigned(r.ax),unsigned(r.sp),unsigned(r.ip),
                 unsigned(r.flags),unsigned(r.memory),unsigned(r.stack),
                 unsigned(expected));
    return false;
}
}
int main() {
    for(unsigned ext=0;ext<8;++ext) {
        for(const BYTE opcode:{BYTE(0xc6),BYTE(0xc7)}) {
            const bool word=opcode==0xc7;
            const WORD imm=word?0x468b:0x008b;
            for(bool memory:{false,true}) {
                const BYTE modrm=BYTE((memory?0x06:0xc0)|(ext<<3));
                const Result r=run(opcode,0,modrm,0x1234,0x8000,imm);
                const WORD wantAx=memory?0x1234:WORD(word?imm:0x128b);
                const BYTE wantMem=memory?0x8b:0x95;
                if(!verify(r.ax==wantAx && r.memory==wantMem &&
                           r.sp==0x8000 && r.flags==FLAGS &&
                           r.ip==WORD(memory?(word?6:5):(word?4:3)),
                           "MOV immediate",ext,r,wantAx))return 1;
            }
        }
        // FF /7 is the same PUSH AX as FF /6.
        if(ext==6 || ext==7) {
            const Result r=run(0xff,0,BYTE(0xc0|(ext<<3)),
                               0x1234,0x8000,0);
            if(!verify(r.stack==0x1234 && r.sp==0x7ffe &&
                       r.ip==2 && r.flags==FLAGS,
                       "FF PUSH",ext,r,0x1234))return 2;
        }
        for(const BYTE op:{BYTE(0x28),BYTE(0x2a)}) {
            // Operand=AL: upper nibble comes from the destination write.
            const Result r=run(0x0f,op,BYTE(0xc0|(ext<<3)),
                               0x0095,0x8000,0);
            const BYTE expectAl=op==0x28?0x59:0x55;
            if(!verify(r.ax==expectAl && r.ip==3 &&
                       r.flags==FLAGS && r.sp==0x8000,
                       "nibble AL",ext,r,expectAl))return 3;
            // The memory operand does not alias AL.
            const Result m=run(0x0f,op,BYTE(0x06|(ext<<3)),
                               0x0003,0x8000,0);
            const BYTE expectMem=op==0x28?0x53:0x39;
            const BYTE expectAlMem=op==0x28?0x09:0x05;
            if(!verify(m.memory==expectMem && m.ax==expectAlMem &&
                       m.ip==5 && m.flags==FLAGS && m.sp==0x8000,
                       "nibble memory",ext,m,expectAlMem))return 4;
        }
    }
    return 0;
}
