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

    // With non-comparison string instructions, V20 64h/65h repeat
    // unconditionally until CX=0, regardless of CF. A single
    // executeOne() must return to the prefix when CX remains nonzero.
    for(const BYTE prefix:{BYTE(0x64),BYTE(0x65)})
        for(const bool carry:{false,true}) {
            const auto result=execute(CpuType::NECV20,
                {prefix,0xa4,0,0},2,0,0,0,2,0x0200,0x0200,
                carry?setCarry:clearCarry);
        // Both initial CF states must continue for non-comparisons.
            if(!check("REPC/REPNC MOVSB",
                result.cx==1 && result.si==0x0201 && result.di==0x0201 &&
                result.dst==0x5a && result.ip==0,
                result))return 1;
        }

    // 64h/65h also repeat I/O strings to CX=0, ignoring carry.
    for(const BYTE prefix:{BYTE(0x64),BYTE(0x65)})
        for(const bool carry:{false,true}) {
            const auto result=execute(CpuType::NECV20,
                {prefix,0x6c,0,0},2,0,0,0,2,0x0200,0x0200,
                carry?setCarry:clearCarry);
        // Both initial CF states must continue for non-comparisons.
            if(!check("REPC/REPNC INSB",
                result.cx==1 && result.di==0x0201 &&
                result.dst==0xff && result.ip==0,
                result))return 2;
        }

    // OUTSB shares the same unconditional repeat rules as INSB.
    // The conformance bus ignores output writes, so verify SI/CX/IP.
    for(const BYTE prefix:{BYTE(0x64),BYTE(0x65)})
        for(const bool carry:{false,true}) {
            const auto result=execute(CpuType::NECV20,
                {prefix,0x6e,0,0},2,0,0,0,2,0x0200,0x0200,
                carry?setCarry:clearCarry);
        // Both initial CF states must continue for non-comparisons.
            if(!check("REPC/REPNC OUTSB",
                result.cx==1 && result.si==0x0201 &&
                result.di==0x0200 && result.ip==0,
                result))return 6;
        }

    // On V20, REP does not negate the quotient for IDIV.
    // Intel 8088 alone has the undocumented quotient negation.
    for(const BYTE prefix:{BYTE(0xf2),BYTE(0xf3)}) {
        const auto byte=execute(CpuType::NECV20,
            {prefix,0xf6,0xfb,0},3,10,2,0,0,0,0,clearCarry);
        if(!check("REP IDIV byte",byte.ax==0x0005 &&
            byte.cx==0 && byte.ip==3 && byte.flags==clearCarry,byte))
            return 3;
        const auto word=execute(CpuType::NECV20,
            {prefix,0xf7,0xfb,0},3,10,2,0,0,0,0,clearCarry);
        if(!check("REP IDIV word",word.ax==0x0005 &&
            word.dx==0 && word.ip==3 && word.flags==clearCarry,word))
            return 4;
    }

    // The 8088 signed DIV alias must still negate the quotient.
    for(const BYTE prefix:{BYTE(0xf2),BYTE(0xf3)}) {
        const auto intel=execute(CpuType::I8088,
            {prefix,0xf6,0xfb,0},3,10,2,0,0,0,0,clearCarry);
        if(!check("8088 REP IDIV",intel.ax==0x00fb &&
                  intel.ip==3,intel))return 7;
    }

    // Keep the unprefixed V20 signed divide unchanged.
    const auto normal=execute(CpuType::NECV20,
        {0xf6,0xfb,0,0},2,10,2,0,0,0,0,clearCarry);
    if(!check("normal IDIV",normal.ax==5 && normal.ip==2,normal))
        return 5;
    return 0;
}
