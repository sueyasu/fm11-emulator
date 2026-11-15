#include "Main8088Board.h"
#include <array>
#include <cstdio>

namespace {
bool check(CpuType variant, const char* label) {
    Main8088Board board(variant);
    board.bus().enableConformanceRam();
    auto& bus=board.bus();
    auto put=[&](unsigned seg,unsigned off,const std::initializer_list<BYTE>& code) {
        unsigned physical=((seg<<4)+off)&0xfffff;
        for(BYTE byte:code)bus.writeMemory(physical++,byte);
    };
    // ROM -> 0400:2500, matching the FM-11 boot IPL far handoff.
    put(0xff00,0,{0xea,0x00,0x25,0x00,0x04});
    // CBIOS INIT: establish DS=SS=CS; temporarily use DS=0000 for
    // interrupt-vector writes, then restore DS and call SELDSK.
    put(0x0400,0x2500,{
        0x8c,0xc8,             // mov ax,cs
        0x8e,0xd8,             // mov ds,ax
        0x8e,0xd0,             // mov ss,ax
        0xbc,0x00,0x40,        // mov sp,4000h
        0x1e,                  // push ds
        0x31,0xc0,             // xor ax,ax
        0x8e,0xd8,             // mov ds,ax
        0x1f,                  // pop ds
        0x31,0xc9,             // xor cx,cx  (drive A:)
        0x31,0xd2,             // xor dx,dx
        0xe8,0xea,0x00,        // call 2600h (SELDSK)
        0xf4                   // hlt
    });
    // SELDSK's original DS-relative indirect DPH table read.
    put(0x0400,0x2600,{
        0x31,0xdb,             // xor bx,bx
        0x80,0xf9,0x04,        // cmp cl,4
        0x73,0x0a,             // jae absent
        0x30,0xed,             // xor ch,ch
        0x89,0xcb,             // mov bx,cx
        0xd1,0xe3,             // shl bx,1
        0x8b,0x9f,0x00,0x27,   // mov bx,[bx+2700h] (DS-relative)
        0xc3                   // ret
    });
    // Distinct contents for DS=0400 and DS=0000 ensure that
    // the test detects an incorrect segment after the IPL handoff.
    put(0x0400,0x2700,{0x2a,0x29}); // DPH0 address = 292Ah
    put(0x0000,0x2700,{0x00,0x00});

    std::array<WORD,8> regs{};
    std::array<WORD,4> segments{};
    segments[1]=0xff00;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,0x0002);
    for(unsigned step=0;step<40;++step) {
        if(cpu.cs()==0x0400 && cpu.ip()==0x2517)break;
        cpu.executeOne();
    }
    const bool success=cpu.cs()==0x0400 && cpu.ip()==0x2517 &&
                       cpu.segment(3)==0x0400 &&
                       cpu.segment(2)==0x0400 &&
                       cpu.reg(3)==0x292a &&
                       cpu.reg(4)==0x4000;
    if(!success)std::fprintf(stderr,
        "%s handoff: CS=%04X IP=%04X DS=%04X SS=%04X "
        "BX=%04X SP=%04X (expected 0400:2517 DS=SS=0400 "
        "BX=292A SP=4000)\n",label,unsigned(cpu.cs()),unsigned(cpu.ip()),
        unsigned(cpu.segment(3)),unsigned(cpu.segment(2)),
        unsigned(cpu.reg(3)),unsigned(cpu.reg(4)));
    return success;
}
}
int main() {
    if(!check(CpuType::I8088,"8088"))return 1;
    if(!check(CpuType::NECV20,"V20"))return 2;
    return 0;
}
