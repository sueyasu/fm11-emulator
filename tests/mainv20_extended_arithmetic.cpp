#include "Main8088Board.h"
#include <array>
#include <cstdio>

namespace {
constexpr WORD initialFlags=0x0883;
struct Result {WORD ax,cx,dx,si,di,flags,ip;BYTE dst;};
Result run(CpuType type,std::array<BYTE,8> code,unsigned length,
           WORD ax,WORD cx,WORD dx,WORD si,WORD di,BYTE source,BYTE dest) {
    Main8088Board board(type);
    board.bus().enableConformanceRam();
    auto& bus=board.bus();
    for(unsigned i=0;i<length;++i)bus.writeMemory(0x10000+i,code[i]);
    bus.writeMemory(0x30200,source);
    bus.writeMemory(0x30201,source);
    bus.writeMemory(0x40200,dest);
    bus.writeMemory(0x40201,0);
    std::array<WORD,8> regs{};
    std::array<WORD,4> segments{};
    regs[0]=ax;regs[1]=cx;regs[2]=dx;
    regs[4]=0x8000;regs[6]=si;regs[7]=di;
    segments[0]=0x4000;segments[1]=0x1000;
    segments[2]=0x5000;segments[3]=0x3000;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,initialFlags);
    cpu.executeOne();
    return {cpu.reg(0),cpu.reg(1),cpu.reg(2),cpu.reg(6),cpu.reg(7),
            cpu.flags(),cpu.ip(),bus.readRam(0x40200)};
}
bool verify(const char* name,bool condition,const Result& r) {
    if(condition)return true;
    std::fprintf(stderr,"%s: AX=%04X CX=%04X DX=%04X "
                        "SI=%04X DI=%04X FLAGS=%04X IP=%04X DST=%02X\n",
                 name,unsigned(r.ax),unsigned(r.cx),unsigned(r.dx),
                 unsigned(r.si),unsigned(r.di),unsigned(r.flags),
                 unsigned(r.ip),unsigned(r.dst));
    return false;
}
}
int main() {
    // V20 AAD always has radix 10, irrespective of encoded immediate.
    // The 8088 must continue using the actual immediate.
    const auto aadV=run(CpuType::NECV20,{0xd5,3},2,0x0205,0,0,0,0,0,0);
    if(!verify("V20 AAD radix",aadV.ax==0x0019 && aadV.ip==2,aadV))return 1;
    const auto aadI=run(CpuType::I8088,{0xd5,3},2,0x0205,0,0,0,0,0,0);
    if(!verify("8088 AAD radix",aadI.ax==0x000b && aadI.ip==2,aadI))return 2;

    // Immediate TEST1 clears CF/OF and updates SF/ZF/PF from the
    // masked bitwise result, including when that result is zero.
    const auto present=run(CpuType::NECV20,
                           {0x0f,0x18,0xc0,3},4,0x0008,0,0,0,0,0,0);
    if(!verify("TEST1 imm set",present.ax==0x0008 &&
               (present.flags&0x08c5)==0 && present.ip==4,present))return 3;
    const auto absent=run(CpuType::NECV20,
                          {0x0f,0x18,0xc0,3},4,0x0000,0,0,0,0,0,0);
    if(!verify("TEST1 imm clear",absent.ax==0 &&
               (absent.flags&0x08c5)==0x0044 && absent.ip==4,absent))return 4;
    const auto sign=run(CpuType::NECV20,
                        {0x0f,0x19,0xc0,15},4,0x8000,0,0,0,0,0,0);
    if(!verify("TEST1 imm word",sign.ax==0x8000 &&
               (sign.flags&0x08c5)==0x0084 && sign.ip==4,sign))return 5;

    // INS: DL initially has bit offset 9, and immediate 0 means 1 bit.
    // ModR/M.reg is ignored for immediate forms; DL must advance to 10.
    const auto insert=run(CpuType::NECV20,
                          {0x0f,0x39,0xfa,0x00},4,
                          0x0001,0,0x0009,0x0200,0x0200,0,0);
    // start register is DL (r/m=2), /7 is an alias.
    if(!verify("INS imm /7 bit offset",insert.dx==0x000a &&
               insert.di==0x0200 && insert.ip==4,insert))return 6;
    const auto extract=run(CpuType::NECV20,
                           {0x0f,0x3b,0xfa,0x00},4,
                           0x0000,0,0x0009,0x0200,0x0200,0x02,0);
    if(!verify("EXT imm /7 bit offset",extract.dx==0x000a &&
               extract.si==0x0200 && extract.ax==1 && extract.ip==4,
               extract))return 7;

    // BCD operations use binary arithmetic followed by decimal adjust.
    const auto add=run(CpuType::NECV20,{0x0f,0x20},2,
                       0,2,0,0x0200,0x0200,0x01,0x09);
    if(!verify("ADD4S valid BCD",add.dst==0x10 &&
               add.si==0x0200 && add.di==0x0200 && add.ip==2,add))return 8;
    const auto sub=run(CpuType::NECV20,{0x0f,0x22},2,
                       0,2,0,0x0200,0x0200,0x01,0x10);
    if(!verify("SUB4S valid BCD",sub.dst==0x09 &&
               sub.si==0x0200 && sub.di==0x0200 && sub.ip==2,sub))return 9;
    const auto cmp=run(CpuType::NECV20,{0x0f,0x26},2,
                       0,2,0,0x0200,0x0200,0x01,0x10);
    if(!verify("CMP4S unchanged memory",cmp.dst==0x10 &&
               cmp.si==0x0200 && cmp.di==0x0200 && cmp.ip==2,cmp))return 10;
    return 0;
}
