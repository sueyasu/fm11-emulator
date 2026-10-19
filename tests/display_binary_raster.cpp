#include "Machine.h"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        Machine machine;
        machine.loadDisplayRomFile(argv[1]);
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        if((machine.displayMemory(0xafe6)&0x80)!=0)return 10;
        // 60 Hz nominal frame: first ~15.67 ms low, 1 ms high.
        for(int i=0;i<50;++i)machine.advanceTimeNanoseconds(250000);
        if(machine.displayMemory(0x9f80)!=0)return 11;
        for(int i=0;i<40;++i)machine.advanceTimeNanoseconds(250000);
        if(machine.displayMemory(0x9f80)!=0x66) {
            std::cerr<<"6809 did not exit AFE6 high/low loops\n";
            return 12;
        }
        std::cout<<"PASS: display 6809 AFE6 low -> high -> low\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
