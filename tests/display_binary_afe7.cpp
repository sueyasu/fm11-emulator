#include "Machine.h"
#include "Main6809Board.h"
#include <iostream>
#include <memory>

int main(int argc,char** argv) {
    if(argc!=3)return 2;
    try {
        Machine machine;
        machine.loadDisplayRomFile(argv[2]);
        auto main=std::make_unique<Main6809Board>(CpuType::MC6809);
        main->loadBootRomFile(argv[1]);
        main->setCpuClockHz(2000000);
        machine.addBoard(std::move(main));
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        // No host-generated notification. Real display MC6809 code
        // must read $AFE7 to assert the event on the main CPU.
        for(int i=0;i<5000;++i)machine.executeOne();
        if(machine.displaySharedByte(0)!=1 ||
           machine.displaySharedByte(1)!=0) {
            std::cerr<<"display AFE7 read -> main FD04 ACK failed\n";
            return 10;
        }
        std::cout<<"PASS: display CPU reads AFE7 -> main FD04 ATTENTION -> ACK\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
