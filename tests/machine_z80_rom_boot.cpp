#include "Machine.h"
#include "Main6809Board.h"
#include "MainZ80Board.h"
#include <memory>

// Run the actual 6809 ROM reset vector, then transfer to the optional
// Z80 card and back. Neither CPU is started by manually assigning its PC.
int main() {
    Machine machine;
    auto main=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* m=main.get();
    Main6809Board::BootRomImage rom{};
    rom[0]=0x86;rom[1]=0x01;              // LDA #1
    rom[2]=0xb7;rom[3]=0xfd;rom[4]=0x05;  // STA $FD05
    rom[5]=0x12;                          // NOP on resume
    rom[0xffe]=0x50;rom[0xfff]=0x00;      // reset to ROM $5000
    main->loadBootRom(rom);
    auto z80=std::make_unique<MainZ80Board>();
    auto* z=z80.get();
    machine.addBoard(std::move(main));
    machine.addBoard(std::move(z80));
    const BYTE program[]={0x3e,0x00,0x32,0x05,0xfd,0x00};
    for(unsigned i=0;i<sizeof(program);++i)
        z->writeMemory(static_cast<WORD>(i),program[i]);
    machine.enableFd05Selection(true);
    machine.reset();
    if(machine.activeBoard()!=m || m->cpu().pc()!=0x5000)return 1;
    machine.executeOne(); // LDA #1
    machine.executeOne(); // STA FD05
    if(machine.activeBoard()!=z || m->cpu().pc()!=0x5005)return 2;
    machine.executeOne(); // Z80 LD A,0
    machine.executeOne(); // Z80 LD (FD05),A
    if(machine.activeBoard()!=m || z->cpu().pc()!=0x0005)return 3;
    machine.executeOne();
    if(m->cpu().pc()!=0x5006)return 4;
    // The resident 6809 service's DMA into bank zero must be visible
    // to Z80 before and after the CPU owner changes.
    m->writePhysical(0xdc00,0xc3);
    m->writePhysical(0xdc01,0x00);
    m->writePhysical(0xdc02,0xf2);
    if(z->readMemory(0xdc00)!=0xc3 ||
       z->readMemory(0xdc01)!=0 ||
       z->readMemory(0xdc02)!=0xf2)return 5;
    return 0;
}
