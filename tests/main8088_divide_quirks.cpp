#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <cstdio>

// Intel 8088 divide behavior verified by SingleStepTests/8088:
//   - REP/REPNE before IDIV inverts the quotient (not the remainder)
//   - quotient -128 (byte) / -32768 (word) raises type-0 interrupt
//   - divide-error return IP is the offset AFTER the failing instruction
namespace {
constexpr WORD FLAGS=0x0082;
struct Result {
    WORD ax,dx,sp,ip,cs,flags;
    WORD stackedIp,stackedCs,stackedFlags;
};
WORD readWord(const Main8088Bus& bus,std::uint32_t addr) {
    return WORD(bus.readRam(addr)|(WORD(bus.readRam(addr+1))<<8));
}
Result run(const std::array<BYTE,4>& code,unsigned len,WORD ax,WORD dx,
           WORD bx,WORD di) {
    Main8088Board board(CpuType::I8088);
    board.bus().enableConformanceRam();
    for(unsigned i=0;i<len;++i)board.bus().writeMemory(0x10000+i,code[i]);
    // Divide error vector -> 0000:0400
    board.bus().writeMemory(0,0x00);
    board.bus().writeMemory(1,0x04);
    board.bus().writeMemory(2,0x00);
    board.bus().writeMemory(3,0x00);
    std::array<WORD,8> regs{};
    std::array<WORD,4> seg{};
    regs[0]=ax;regs[2]=dx;regs[3]=bx;regs[4]=0x8000;regs[7]=di;
    seg[1]=0x1000;seg[2]=0x3000;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,FLAGS);
    cpu.executeOne();
    const auto base=std::uint32_t(0x30000+0x7ffa);
    return {cpu.reg(0),cpu.reg(2),cpu.reg(4),cpu.ip(),cpu.cs(),cpu.flags(),
            readWord(board.bus(),base),readWord(board.bus(),base+2),
            readWord(board.bus(),base+4)};
}
bool normal(const char* name,const Result& r,WORD ax,WORD dx,WORD ip) {
    if(r.ax==ax && r.dx==dx && r.ip==ip && r.cs==0x1000 &&
       r.sp==0x8000 && r.flags==FLAGS)return true;
    std::fprintf(stderr,"%s: AX=%04X DX=%04X SP=%04X IP=%04X "
                 "CS=%04X FLAGS=%04X (expected AX=%04X DX=%04X IP=%04X)\n",
                 name,r.ax,r.dx,r.sp,r.ip,r.cs,r.flags,ax,dx,ip);
    return false;
}
bool exception(const char* name,const Result& r,WORD ax,WORD ip) {
    if(r.ax==ax && r.sp==0x7ffa && r.ip==0x0400 && r.cs==0 &&
       r.stackedIp==ip && r.stackedCs==0x1000 &&
       r.stackedFlags==FLAGS)return true;
    std::fprintf(stderr,"%s: AX=%04X SP=%04X CS:IP=%04X:%04X "
                 "stack=%04X,%04X,%04X expected IP=%04X\n",
                 name,r.ax,r.sp,r.cs,r.ip,r.stackedIp,r.stackedCs,
                 r.stackedFlags,ip);
    return false;
}
}
int main() {
    // IDIV BL: 42/2 = 21; REP makes it -21, remainder stays 0.
    if(!normal("IDIV",run({0xf6,0xfb,0,0},2,42,0,2,0),
               0x0015,0,2))return 1;
    if(!normal("REP IDIV",run({0xf3,0xf6,0xfb,0},3,42,0,2,0),
               0x00eb,0,3))return 2;
    if(!normal("REPNE IDIV",run({0xf2,0xf6,0xfb,0},3,42,0,2,0),
               0x00eb,0,3))return 3;
    // IDIV DI: signed 32-bit DX:AX / 2 = 21, or -21 with REP.
    if(!normal("word IDIV",run({0xf7,0xff,0,0},2,42,0,0,2),
               0x0015,0,2))return 4;
    if(!normal("word REP IDIV",run({0xf3,0xf7,0xff,0},3,42,0,0,2),
               0xffeb,0,3))return 5;
    // Divide by zero: software INT0 frame must save the NEXT IP.
    if(!exception("DIV zero",run({0xf6,0xf3,0,0},2,0x1234,0,0,0),
                  0x1234,2))return 6;
    // 8088 does not accept the most negative signed quotient.
    if(!exception("IDIV byte -128",run({0xf6,0xfb,0,0},2,0x0080,0,0x00ff,0),
                  0x0080,2))return 7;
    if(!exception("IDIV word -32768",run({0xf7,0xff,0,0},2,0x8000,0xffff,0,1),
                  0x8000,2))return 8;
    return 0;
}
