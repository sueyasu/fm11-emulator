#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <cstdio>

// Intel 8088: C6/C7 ModR/M.reg bits are ignored. Exercise all /0../7
// encodings for both register and memory forms and both operand widths.
namespace {
bool check(BYTE opcode,unsigned regField,bool memoryForm) {
    Main8088Board board(CpuType::I8088);
    board.bus().enableConformanceRam();
    constexpr WORD offset=0x2100;
    constexpr WORD ds=0x3000;
    constexpr std::uint32_t memoryAddress=0x32100;
    const BYTE modrm=BYTE((memoryForm?0x06:0xc0)|(regField<<3));
    auto& bus=board.bus();
    bus.writeMemory(0x10000,opcode);
    bus.writeMemory(0x10001,modrm);
    unsigned pos=2;
    if(memoryForm) {
        bus.writeMemory(0x10000+pos++,BYTE(offset));
        bus.writeMemory(0x10000+pos++,BYTE(offset>>8));
    }
    const WORD immediate=opcode==0xc6?WORD(0x00ab):WORD(0x92ab);
    bus.writeMemory(0x10000+pos++,BYTE(immediate));
    if(opcode==0xc7)bus.writeMemory(0x10000+pos++,BYTE(immediate>>8));
    if(memoryForm) {
        bus.writeMemory(memoryAddress,0x55);
        bus.writeMemory(memoryAddress+1,0x66);
    }
    std::array<WORD,8> regs{};
    std::array<WORD,4> seg{};
    regs[0]=0x3456;
    regs[4]=0x8000;
    seg[1]=0x1000; seg[3]=ds;
    constexpr WORD flags=0x0297;
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,flags);
    cpu.executeOne();
    WORD actual=0;
    if(memoryForm) {
        actual=WORD(bus.readRam(memoryAddress) |
                    (WORD(bus.readRam(memoryAddress+1))<<8));
    } else actual=cpu.reg(0);
    const WORD expected=opcode==0xc6
        ? WORD(memoryForm?0x66ab:0x34ab)
        : immediate;
    if(actual==expected && cpu.ip()==pos && cpu.flags()==flags &&
       cpu.cs()==0x1000)return true;
    std::fprintf(stderr,"opcode=%02X /%u %s: value=%04X expected=%04X "
                        "IP=%04X expected=%04X FLAGS=%04X\n",
                 unsigned(opcode),regField,memoryForm?"mem":"reg",
                 unsigned(actual),unsigned(expected),unsigned(cpu.ip()),
                 pos,unsigned(cpu.flags()));
    return false;
}
}
int main() {
    for(const BYTE opcode:{BYTE(0xc6),BYTE(0xc7)})
        for(unsigned field=0;field<8;++field)
            for(bool memoryForm:{false,true})
                if(!check(opcode,field,memoryForm))return 1;
    return 0;
}
