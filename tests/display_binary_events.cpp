#include "Machine.h"
#include "Main6809Board.h"
#include <iostream>
#include <memory>

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        Machine machine;
        auto cpu=std::make_unique<Main6809Board>(CpuType::MC6809);
        cpu->loadBootRomFile(argv[1]);
        cpu->setCpuClockHz(2000000);
        machine.addBoard(std::move(cpu));
        machine.reset();
        // Exercise both independent main-board events simultaneously.
        machine.injectDisplayAttention();
        machine.injectBreakEvent();
        for(int i=0;i<100;++i)machine.executeOne();
        if(machine.displaySharedByte(0)!=0x03 ||
           machine.displaySharedByte(1)!=0x00) {
            std::cerr<<"FD04 combined event latch / clear mismatch\n";
            return 10;
        }
        // A second edge must be accepted after the read-to-clear ACK.
        machine.injectDisplayAttention();
        auto* main=dynamic_cast<Main6809Board*>(&machine.board(0));
        if(!main || main->readCpu(0xfd04)!=1 ||
           main->readCpu(0xfd04)!=0)return 11;
        std::cout<<"PASS: ATTENTION+BREAK -> FD04=03 -> clear -> rearm\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
