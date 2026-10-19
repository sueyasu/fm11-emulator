#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>

int main()
{
    Machine machine;
    auto main=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu09=main.get();
    Main6809Board::BootRomImage rom09{};
    rom09[0xffe]=0x12;
    rom09[0xfff]=0x34;
    main->loadBootRom(rom09);
    // LDA #1; STA $FD05; NOP -- the NOP must wait for 8088 handoff.
    const BYTE program09[]={0x86,0x01,0xb7,0xfd,0x05,0x12};
    for(unsigned i=0;i<sizeof(program09);++i)
        main->writePhysical(0x1234+i,program09[i]);

    auto optional=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu86=optional.get();
    Main8088Bus::Rom rom86{};
    rom86.fill(0x90);
    // MOV DX,FD05h; MOV AL,0; OUT DX,AL; NOP.
    // OUT DX,AL, unlike OUT imm8, can reach the system FD05 port.
    const BYTE program86[]={0xba,0x05,0xfd,0xb0,0x00,0xee,0x90};
    for(unsigned i=0;i<sizeof(program86);++i)
        rom86[0xff0+i]=program86[i];
    optional->bus().loadBootRom(rom86);

    machine.addBoard(std::move(main));
    machine.addBoard(std::move(optional));
    machine.enableFd05Selection(true);
    machine.reset();
    if(machine.activeBoard()!=cpu09)return 1;

    machine.executeOne(); // 6809 LDA #1
    if(machine.activeBoard()!=cpu09 || cpu09->cpu().pc()!=0x1236)return 2;
    machine.executeOne(); // 6809 STA $FD05
    if(machine.activeBoard()!=cpu86 || cpu09->cpu().pc()!=0x1239)return 3;

    // The main-board PTM keeps running while the optional CPU owns the
    // bus. Configure it only after the 6809 has yielded, so this test
    // measures time charged to 8088 instructions, not prior 6809 work.
    machine.set8088ClockHz(9700); // one approximate CPU cycle per PTM tick
    cpu09->writeCpu(0xfd39,0x01);
    cpu09->writeCpu(0xfd38,0x50);
    cpu09->writeCpu(0xfd3a,0);
    cpu09->writeCpu(0xfd3b,1);
    if(cpu09->readCpu(0xfd39)!=0)return 9;

    machine.executeOne(); // 8088 MOV DX,FD05h
    if(machine.activeBoard()!=cpu86 || cpu86->cpu().pc()!=0xffff3)return 4;
    // A shared device must advance even with the 6809 CPU suspended.
    if((cpu09->readCpu(0xfd39)&0x81)!=0x81)return 10;
    machine.executeOne(); // 8088 MOV AL,0
    if(machine.activeBoard()!=cpu86 || cpu86->cpu().pc()!=0xffff5)return 5;
    machine.executeOne(); // 8088 OUT DX,AL
    if(machine.activeBoard()!=cpu09 || cpu86->cpu().pc()!=0xffff6)return 6;

    machine.executeOne(); // resumed 6809 NOP
    if(machine.activeBoard()!=cpu09 || cpu09->cpu().pc()!=0x123a)return 7;
    if(cpu86->cpu().pc()!=0xffff6)return 8;
    return 0;
}
