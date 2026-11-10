#include "Machine.h"
#include "Main6809Board.h"
#include "MainZ80Board.h"
#include <memory>

int main()
{
    Machine machine;
    auto main=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu09=main.get();
    Main6809Board::BootRomImage rom{};
    rom[0xffe]=0x12;rom[0xfff]=0x34;
    main->loadBootRom(rom);
    const BYTE m6809[]={0x86,0x01,0xb7,0xfd,0x05,0x12};
    for(unsigned i=0;i<sizeof(m6809);++i)
        main->writePhysical(0x1234+i,m6809[i]);
    auto z80=std::make_unique<MainZ80Board>();
    auto* cpuZ80=z80.get();
    machine.addBoard(std::move(main));
    machine.addBoard(std::move(z80));
    // LD A,0 ; LD (FD05),A ; NOP
    const BYTE code[]={0x3e,0x00,0x32,0x05,0xfd,0x00};
    for(unsigned i=0;i<sizeof(code);++i)cpuZ80->writeMemory(i,code[i]);
    machine.enableFd05Selection(true);
    machine.reset();
    if(machine.activeBoard()!=cpu09)return 1;
    machine.executeOne();
    machine.executeOne();
    if(machine.activeBoard()!=cpuZ80 || cpu09->cpu().pc()!=0x1239)return 2;
    machine.executeOne();
    machine.executeOne();
    if(machine.activeBoard()!=cpu09 || cpuZ80->cpu().pc()!=5)return 3;
    machine.executeOne();
    if(cpu09->cpu().pc()!=0x123a || cpuZ80->cpu().pc()!=5)return 4;

    // Both CPUs must see the same physical bank-0 and banked RAM.
    cpu09->writePhysical(0x0123,0x5a);
    if(cpuZ80->readMemory(0x0123)!=0x5a)return 5;
    cpuZ80->writeMemory(0x2345,0xa5);
    if(cpu09->readPhysical(0x2345)!=0xa5)return 6;
    cpuZ80->writeMemory(0xfd2f,2);
    cpuZ80->writeMemory(0x4567,0x77);
    if(cpu09->readPhysical(0x24567)!=0x77)return 7;
    cpu09->writePhysical(0x24567,0x33);
    if(cpuZ80->readMemory(0x4567)!=0x33)return 8;
    cpuZ80->writeMemory(0xc123,0x42);
    if(cpu09->readPhysical(0xc123)!=0x42)return 9;
    if(cpuZ80->readMemory(0xfd2f)!=2)return 10;
    machine.reset();
    if(cpuZ80->bank()!=0 || machine.activeBoard()!=cpu09)return 11;

    // Reversed board insertion must also share RAM and arbitrate FD05.
    Machine reversed;
    auto opt=std::make_unique<MainZ80Board>();
    auto* optPtr=opt.get();
    reversed.addBoard(std::move(opt));
    auto primary=std::make_unique<Main6809Board>();
    auto* primaryPtr=primary.get();
    reversed.addBoard(std::move(primary));
    primaryPtr->writePhysical(0x1111,0x6b);
    if(optPtr->readMemory(0x1111)!=0x6b)return 12;
    reversed.enableFd05Selection(true);
    if(reversed.activeBoard()!=primaryPtr)return 13;
    primaryPtr->writeCpu(0xfd05,1);
    if(reversed.activeBoard()!=optPtr)return 14;
    return 0;
}
