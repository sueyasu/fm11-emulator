#include "Main6809Board.h"

namespace {
bool run(unsigned md, unsigned kind)
{
    Main6809Board board(CpuType::HD6309);
    Main6809Board::BootRomImage rom{};
    rom[0xffe]=0x10;
    board.loadBootRom(rom);
    board.reset();
    board.writeCpu(0xfff6,0x11); board.writeCpu(0xfff7,0x00);
    board.writeCpu(0xfffc,0x11); board.writeCpu(0xfffd,0x00);
    board.writeCpu(0xfff8,0x11); board.writeCpu(0xfff9,0x00);
    board.writeCpu(0x1100,0x3b);
    dynamic_cast<CSim09&>(board.cpu()).regs.r.w.S=0x3000;
    dynamic_cast<CSim09&>(board.cpu()).regs.r.w.W=0x1234; // E/F
    dynamic_cast<CSim09&>(board.cpu()).regs.r.w.D=0x5678; // A/B
    dynamic_cast<CSim09&>(board.cpu()).regs.MD=static_cast<BYTE>(md);
    board.setCpuClockHz(9700);
    board.writeCpu(0xfd39,1); board.writeCpu(0xfd38,0x50);
    board.writeCpu(0xfd3a,0); board.writeCpu(0xfd3b,100);
    if (kind==0) board.cpu().firq();
    else if (kind==1) board.cpu().nmi();
    else { // IRQ accepted via instruction boundary
        board.writeCpu(0xfd02,0x80);
        board.setMainIrqSource(0x40,true);
        board.cpu().executeOne();
        board.setMainIrqSource(0x40,false);
    }
    if (board.cpu().pc()!=0x1100) return false;
    const bool full = kind!=0 || (md & 0x02)!=0;
    const unsigned frame = full ? ((md & 1)!=0 ? 14 : 12) : 3;
    if (dynamic_cast<CSim09&>(board.cpu()).regs.r.w.S != 0x3000-frame) return false;
    if (full && (md & 1)!=0) {
        if (board.readCpu(0x3000-frame+3)!=0x12 ||
            board.readCpu(0x3000-frame+4)!=0x34) return false;
    }
    // Alter E/F to demonstrate native RTI restores their stacked values.
    dynamic_cast<CSim09&>(board.cpu()).regs.r.w.W=0xabcd;
    board.cpu().executeOne(); // RTI
    if (board.cpu().pc()!=0x1000 || dynamic_cast<CSim09&>(board.cpu()).regs.r.w.S!=0x3000)
        return false;
    if (full && (md & 1)!=0 && dynamic_cast<CSim09&>(board.cpu()).regs.r.w.W!=0x1234)
        return false;
    const unsigned entry = full ? ((md & 1)!=0?21:19):10;
    const unsigned rti = full ? ((md & 1)!=0?17:15):6;
    return board.readCpu(0xfd3a)==0 &&
           board.readCpu(0xfd3b)==100-entry-rti;
}
}
int main()
{
    // 6309 emulation + native, FIRQ short / full, IRQ and NMI.
    for(unsigned md=0;md<4;++md)
        for(unsigned kind=0;kind<3;++kind)
            if(!run(md,kind)) return 1+md*3+kind;
    return 0;
}
