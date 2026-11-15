#include "Main8088Board.h"
#include <array>
#include <cstdio>

// The hardware-reference instruction tests have no FM-11 I/O peripherals.
// Every IN must see the un-driven FFh data bus even at addresses normally
// decoded by the FM-11 8259/8237; ordinary bus behavior is unchanged.
namespace {
bool check(BYTE opcode,BYTE port8,WORD dx,WORD startAx) {
    Main8088Board board(CpuType::I8088);
    board.bus().enableConformanceRam();
    constexpr std::uint32_t codeBase=0x10000;
    board.bus().writeMemory(codeBase,opcode);
    board.bus().writeMemory(codeBase+1,port8);
    std::array<WORD,8> regs{};
    std::array<WORD,4> segments{};
    regs[0]=startAx;
    regs[2]=dx; // DX in internal AX,CX,DX,BX order
    regs[4]=0x8000;
    segments[1]=0x1000;
    constexpr WORD flags=0x0202;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,flags);
    cpu.executeOne();
    const WORD expected=(opcode==0xe4 || opcode==0xec)
        ? WORD((startAx&0xff00)|0x00ff) : WORD(0xffff);
    const WORD length=(opcode==0xe4 || opcode==0xe5)?2:1;
    if(cpu.reg(0)==expected && cpu.ip()==length && cpu.flags()==flags)return true;
    std::fprintf(stderr,"opcode=%02X port=%04X AX=%04X expected=%04X "
                 "IP=%04X expected IP=%04X\n",
                 unsigned(opcode),
                 unsigned(opcode==0xe4 || opcode==0xe5 ? port8:dx),
                 unsigned(cpu.reg(0)),unsigned(expected),
                 unsigned(cpu.ip()),unsigned(length));
    return false;
}
}
int main() {
    // PIC status/mask, DMA addresses/counts, and an unassigned port.
    for(const BYTE port:{BYTE(0x00),BYTE(0x01),BYTE(0x10),
                         BYTE(0x11),BYTE(0x18),BYTE(0x23),BYTE(0xff)}) {
        if(!check(0xe4,port,0,0x5a00))return 1;
        if(!check(0xe5,port,0,0x1234))return 2;
        if(!check(0xec,0,port,0x5a00))return 3;
        if(!check(0xed,0,port,0x1234))return 4;
    }
    // The DX-word port read wraps FFFFh -> 0000h.
    if(!check(0xed,0,0xffff,0x1234))return 5;
    return 0;
}
