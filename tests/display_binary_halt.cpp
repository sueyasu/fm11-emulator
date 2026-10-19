#include "Machine.h"
#include "Main6809Board.h"
#include <iostream>
#include <memory>

int main(int argc,char** argv) {
    if(argc!=3)return 2;
    try {
        Machine machine;
        machine.loadDisplayRomFile(argv[2]);
        auto cpu=std::make_unique<Main6809Board>(CpuType::MC6809);
        auto* main=cpu.get();
        cpu->loadBootRomFile(argv[1]);
        cpu->setCpuClockHz(2000000);
        machine.addBoard(std::move(cpu));
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        machine.executeOne(); // LDA #$80; display may begin executing
        machine.executeOne(); // STA $FD05: HALT asserted
        if(main->readCpu(0xfd05)!=0x80)return 10;
        const auto haltedPc=machine.displayCpuPc();
        machine.executeOne(); // LDA #$11
        machine.executeOne(); // STA $FC80 while display CPU is halted
        machine.executeOne(); // LDA #$00
        if(machine.displayCpuPc()!=haltedPc ||
           machine.displaySharedByte(0)!=0x11)return 11;
        machine.executeOne(); // STA $FD05: release HALT
        if(main->readCpu(0xfd05)!=0)return 12;
        for(int i=0;i<1000;++i)machine.executeOne();
        if(machine.displayCpuPc()==haltedPc ||
           machine.displayBusyStatus()!=0x42 ||
           machine.displaySharedByte(2)!=0x33 ||
           machine.displaySharedByte(1)!=0x22) {
            std::cerr<<"release did not resume display BUSY/shared-RAM writes\n";
            return 13;
        }
        std::cout<<"PASS: main FD05 HALT -> sub PC frozen -> release -> sub AFF0 BUSY\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
