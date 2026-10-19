#include "Machine.h"
#include "Main8088Board.h"
#include <memory>

int main() {
    Machine m;
    auto card=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* b=card.get();
    Main8088Bus::Rom rom{};
    rom.fill(0x90); // NOP
    rom[0xff1]=0x26; // ES override prefix on next NOP
    rom[0xff2]=0x90;
    rom[0xff3]=0xf4; // HLT: timing not characterized
    b->bus().loadBootRom(rom);
    m.addBoard(std::move(card));
    m.reset();
    m.executeOne();
    if(m.lastExecutionCycles()!=3)return 1;
    m.executeOne();
    if(m.lastExecutionCycles()!=0)return 2;
    m.executeOne();
    if(m.lastExecutionCycles()!=0)return 3;
    m.executeOne(); // halted: no elapsed instruction cycles
    if(m.lastExecutionCycles()!=0)return 4;
    Machine v;
    auto c=std::make_unique<Main8088Board>(CpuType::NECV20);
    c->bus().loadBootRom(rom);
    v.addBoard(std::move(c));
    v.reset();
    v.executeOne();
    if(v.lastExecutionCycles()!=0)return 5;
    // 8088 flag-only instructions each take two clocks.
    Machine flags;
    auto f=std::make_unique<Main8088Board>(CpuType::I8088);
    Main8088Bus::Rom flagRom{};
    flagRom.fill(0x90);
    const BYTE ops[]={0xf8,0xf9,0xf5,0xfa,0xfb,0xfc,0xfd};
    for(unsigned i=0;i<sizeof(ops);++i)flagRom[0xff0+i]=ops[i];
    f->bus().loadBootRom(flagRom);
    flags.addBoard(std::move(f));
    flags.reset();
    for(unsigned i=0;i<sizeof(ops);++i) {
        flags.executeOne();
        if(flags.lastExecutionCycles()!=2)return int(6+i);
    }
    return 0;
}
