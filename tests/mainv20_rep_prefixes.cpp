#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <cstdio>

namespace {
struct CpuResult {
    WORD ax,dx,cx,si,di,ip,flags;
    BYTE dst;
};
CpuResult execute(CpuType variant,const std::array<BYTE,4>& code,
                  unsigned size,WORD ax,WORD bx,WORD dx,WORD cx,
                  WORD si,WORD di,WORD flags) {
    Main8088Board board(variant);
    board.bus().enableConformanceRam();
    auto& bus=board.bus();
    for(unsigned i=0;i<size;++i)bus.writeMemory(0x10000+i,code[i]);
    bus.writeMemory(0x30200,0x5a);
    bus.writeMemory(0x40200,0);
    std::array<WORD,8> regs{};
    std::array<WORD,4> seg{};
    regs[0]=ax;regs[1]=cx;regs[2]=dx;regs[3]=bx;
    regs[4]=0x8000;regs[6]=si;regs[7]=di;
    seg[0]=0x4000;seg[1]=0x1000;seg[2]=0x5000;seg[3]=0x3000;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,flags);
    cpu.executeOne();
    return {cpu.reg(0),cpu.reg(2),cpu.reg(1),cpu.reg(6),
            cpu.reg(7),cpu.ip(),cpu.flags(),bus.readRam(0x40200)};
}
bool check(const char* label,bool pass,const CpuResult& r) {
    if(pass)return true;
    std::fprintf(stderr,"%s AX=%04X DX=%04X CX=%04X "
        "SI=%04X DI=%04X IP=%04X FLAGS=%04X DST=%02X\n",
        label,unsigned(r.ax),unsigned(r.dx),unsigned(r.cx),
        unsigned(r.si),unsigned(r.di),unsigned(r.ip),
        unsigned(r.flags),unsigned(r.dst));
    return false;
}
}

int main() {
    constexpr WORD clearCarry=0x0002, setCarry=0x0003;

    // REPC (65h) continues MOVSB while carry remains set;
    // REPNC (64h) continues while carry is clear. In either case
    // the first iteration executes and CX is decremented.
    for(const BYTE prefix:{BYTE(0x64),BYTE(0x65)})
        for(const bool carry:{false,true}) {
            const auto result=execute(CpuType::NECV20,
                {prefix,0xa4,0,0},2,0,0,0,2,0x0200,0x0200,
                carry?setCarry:clearCarry);
            const bool again=carry==(prefix==0x65);
            if(!check("REPC/REPNC MOVSB",
                result.cx==1 && result.si==0x0201 && result.di==0x0201 &&
                result.dst==0x5a && result.ip==(again?0:2),
                result))return 1;
        }

    // The same carry-controlled repeat prefix must be accepted with
    // INS/OUTS (6Ch-6Fh) in addition to the ordinary string family.
    for(const BYTE prefix:{BYTE(0x64),BYTE(0x65)})
        for(const bool carry:{false,true}) {
            const auto result=execute(CpuType::NECV20,
                {prefix,0x6c,0,0},2,0,0,0,2,0x0200,0x0200,
                carry?setCarry:clearCarry);
            const bool again=carry==(prefix==0x65);
            if(!check("REPC/REPNC INSB",
                result.cx==1 && result.di==0x0201 &&
                result.dst==0xff && result.ip==(again?0:2),
                result))return 2;
        }

    // OUTSB shares the same carry termination rules as INSB.
    // The conformance bus ignores output writes, so verify SI/CX/IP.
    for(const BYTE prefix:{BYTE(0x64),BYTE(0x65)})
        for(const bool carry:{false,true}) {
            const auto result=execute(CpuType::NECV20,
                {prefix,0x6e,0,0},2,0,0,0,2,0x0200,0x0200,
                carry?setCarry:clearCarry);
            const bool again=carry==(prefix==0x65);
            if(!check("REPC/REPNC OUTSB",
                result.cx==1 && result.si==0x0201 &&
                result.di==0x0200 && result.ip==(again?0:2),
                result))return 6;
        }

    // REP on V20 IDIV negates the quotient, but does not repeat.
    // 10 / 2 yields -5, remainder 0, regardless of F2 or F3.
    for(const BYTE prefix:{BYTE(0xf2),BYTE(0xf3)}) {
        const auto byte=execute(CpuType::NECV20,
            {prefix,0xf6,0xfb,0},3,10,2,0,0,0,0,clearCarry);
        if(!check("REP IDIV byte",byte.ax==0x00fb &&
            byte.cx==0 && byte.ip==3 && byte.flags==clearCarry,byte))
            return 3;
        const auto word=execute(CpuType::NECV20,
            {prefix,0xf7,0xfb,0},3,10,2,0,0,0,0,clearCarry);
        if(!check("REP IDIV word",word.ax==0xfffb &&
            word.dx==0 && word.ip==3 && word.flags==clearCarry,word))
            return 4;
    }

    // Keep the unprefixed V20 signed divide unchanged.
    const auto normal=execute(CpuType::NECV20,
        {0xf6,0xfb,0,0},2,10,2,0,0,0,0,clearCarry);
    if(!check("normal IDIV",normal.ax==5 && normal.ip==2,normal))
        return 5;
    return 0;
}
