#include "Main8088Board.h"
#include <array>
#include <cstdio>

namespace {
bool check(const char* name,BYTE ext,BYTE modrm,bool immediate,
           WORD ax,WORD cx,WORD expectedAx,
           WORD expectedMemory) {
    Main8088Board board(CpuType::NECV20);
    board.bus().enableConformanceRam();
    auto& bus=board.bus();
    bus.writeMemory(0x10000,0x0f);
    bus.writeMemory(0x10001,ext);
    bus.writeMemory(0x10002,modrm);
    if(immediate)bus.writeMemory(0x10003,0);
    bus.writeMemory(0x40200,0);
    bus.writeMemory(0x40201,0);
    std::array<WORD,8> regs{};
    std::array<WORD,4> segments{};
    regs[0]=ax;
    regs[1]=cx;
    regs[4]=0x8000;
    regs[7]=0x0200;
    segments[0]=0x4000;
    segments[1]=0x1000;
    segments[2]=0x5000;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,0x0083);
    cpu.executeOne();
    const WORD memory=WORD(bus.readRam(0x40200)|
                          (WORD(bus.readRam(0x40201))<<8));
    const WORD expectedIp=immediate?4:3;
    const bool ok=cpu.reg(0)==expectedAx &&
                  memory==expectedMemory &&
                  cpu.ip()==expectedIp &&
                  cpu.reg(7)==0x0200 &&
                  cpu.flags()==0x0083;
    if(!ok)std::fprintf(stderr,
        "%s: AX=%04X MEM=%04X IP=%04X DI=%04X FLAGS=%04X; "
        "expected AX=%04X MEM=%04X IP=%04X\n",
        name,unsigned(cpu.reg(0)),unsigned(memory),unsigned(cpu.ip()),
        unsigned(cpu.reg(7)),unsigned(cpu.flags()),
        unsigned(expectedAx),unsigned(expectedMemory),unsigned(expectedIp));
    return ok;
}
}
int main() {
    // Immediate INS, with the bit-index register in AL: the updated
    // AL=1 is also the bit source, so ES:[DI] receives bit 0=1.
    if(!check("INS imm AL alias",0x39,0xc0,true,
              0x0000,0x0000,0x0001,0x0001))return 1;
    // Register-length INS with AL serving as both bit index and length.
    if(!check("INS reg AL alias",0x31,0xc0,false,
              0x0000,0x0000,0x0001,0x0001))return 2;
    // AH aliases the upper half of AX. A bitfield longer than eight
    // bits reads the updated AH; updating after the write would
    // incorrectly emit 0200h instead of 0600h into ES:[DI].
    if(!check("INS reg AH alias",0x31,0xcc,false,
              0x0100,0x0009,0x0b00,0x0600))return 3;
    return 0;
}
