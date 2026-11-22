#include "Main8088Board.h"
#include <array>
#include <iostream>

// MOV r/m16,Sreg (8C) and MOV Sreg,r/m16 (8E), including
// undocumented /4..7 segment-selector aliases on the 8088.
// Keep address-selection and transfer tests independent of the
// timing implementation: EA[BX]=5, EA[BP+disp]=9.
namespace {
constexpr WORD kOriginalSeg[4]={0x0400,0x1000,0x0200,0x0300};
struct Case {
    BYTE opcode;
    unsigned ext;
    unsigned addressMode; // 0=[BX], 1=[BP+disp8], 2=[BP+disp16], 3=BX
    bool overrideEs;
};
bool check(const Case& t) {
    Main8088Board board(CpuType::I8088);
    const bool memory=t.addressMode<3;
    const unsigned rm=t.addressMode==0?7:t.addressMode==3?3:6;
    const unsigned mod=t.addressMode;
    const BYTE modrm=BYTE((mod<<6)|(t.ext<<3)|rm);
    const unsigned baseLength=t.addressMode==0||t.addressMode==3?2u:
        t.addressMode==1?3u:4u;
    const unsigned length=baseLength+(t.overrideEs?1u:0u);
    unsigned p=0x10000;
    if(t.overrideEs)board.bus().writeMemory(p++,0x26);
    board.bus().writeMemory(p++,t.opcode);
    board.bus().writeMemory(p++,modrm);
    if(t.addressMode==1)board.bus().writeMemory(p++,0x10);
    if(t.addressMode==2) {
        board.bus().writeMemory(p++,0x10);
        board.bus().writeMemory(p++,0x00);
    }
    std::array<WORD,8> regs{};
    regs[3]=0x0100; // BX
    regs[4]=0x8000; // SP
    regs[5]=0x0110; // BP
    const std::array<WORD,4> segments{
        kOriginalSeg[0],kOriginalSeg[1],
        kOriginalSeg[2],kOriginalSeg[3]};
    const unsigned addressOffset=t.addressMode==0?0x0100u:0x0120u;
    const unsigned addressSegment=t.overrideEs?0x0400u:
        t.addressMode==0?0x0300u:0x0200u;
    const unsigned address=(addressSegment<<4)+addressOffset;
    // Memory source is distinctive; memory destination is initialized
    // to a different value to expose direction mistakes.
    if(memory) {
        board.bus().writeMemory(address,0x78);
        board.bus().writeMemory(address+1,0x56);
    }
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    cpu.executeOne();

    const unsigned ea=t.addressMode==0?5u:
        t.addressMode==3?0u:9u;
    const unsigned expectedCycles=(memory
        ?(t.opcode==0x8c?13u:12u)+ea : 2u)
        +(t.overrideEs?2u:0u);
    if(cpu.lastExecutionCycles()!=expectedCycles || cpu.ip()!=length) {
        std::cerr<<"MOV segment opcode "<<std::hex<<unsigned(t.opcode)
                 <<" modrm "<<unsigned(modrm)<<std::dec
                 <<" cycles "<<cpu.lastExecutionCycles()
                 <<" expected "<<expectedCycles
                 <<" IP "<<cpu.ip()<<" expected "<<length<<"\n";
        return false;
    }

    const WORD source=t.opcode==0x8c
        ?kOriginalSeg[t.ext&3]:WORD(memory?0x5678:0x0100);
    if(t.opcode==0x8c) {
        if(memory) {
            if(board.bus().readMemory(address)!=BYTE(source) ||
               board.bus().readMemory(address+1)!=BYTE(source>>8))
                return false;
        } else if(cpu.reg(3)!=source)return false;
    } else {
        if(cpu.segment(t.ext&3)!=source)return false;
    }
    for(unsigned segment=0;segment<4;++segment)
        if((t.opcode==0x8c || segment!=(t.ext&3)) &&
            cpu.segment(segment)!=kOriginalSeg[segment])return false;

    return true;
}
}
int main() {
    for(BYTE opcode:{BYTE(0x8c),BYTE(0x8e)}) {
        for(unsigned ext=0;ext<8;++ext) {
            for(unsigned mode=0;mode<4;++mode) {
                for(unsigned pref=0;pref<2;++pref) {
                    if(!check({opcode,ext,mode,pref!=0})) {
                        std::cerr<<"segment MOV matrix failed: opcode "
                            <<std::hex<<unsigned(opcode)<<std::dec
                            <<" selector "<<ext<<" mode "<<mode
                            <<" ES-override "<<pref<<"\n";
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
