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
        machine.injectDisplayAttention();
        for(int i=0;i<100;++i)machine.executeOne();
        if(machine.displaySharedByte(0)!=1 || machine.displaySharedByte(1)!=0) {
            std::cerr<<"FD04 ATTENTION latch/ack failed\n";
            return 10;
        }
        std::cout<<"PASS: injected ATTENTION -> main FD04 read -> ACK cleared\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
