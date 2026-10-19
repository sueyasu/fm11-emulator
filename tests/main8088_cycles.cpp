#include "Machine.h"
#include "Main8088Board.h"
#include <memory>

int main() {
    Machine m;
    auto card=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* b=card.get();
    Main8088Bus::Rom rom{};
    rom.fill(0x90); // NOP
    rom[1]=0x26; // ES override prefix on next NOP
    rom[2]=0x90;
    rom[3]=0xf4; // HLT: timing not characterized
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
    return 0;
}
