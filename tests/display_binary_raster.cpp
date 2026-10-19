#include "Machine.h"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        Machine machine;
        machine.loadDisplayRomFile(argv[1]);
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        // Reference GetIoByte toggles bit 7 on successive CPU reads.
        // No synthetic 60Hz interval or SDL timing is required.
        machine.advanceTimeNanoseconds(100000);
        if(machine.displayMemory(0x9f80)!=0x66) {
            std::cerr<<"6809 did not exit AFE6 read-toggle loops\n";
            return 12;
        }
        std::cout<<"PASS: display 6809 AFE6 read-toggle high -> low\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
