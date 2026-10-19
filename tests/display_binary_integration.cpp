#include "Machine.h"
#include "Main6809Board.h"
#include <iostream>
#include <memory>
#include <string>

int main(int argc,char** argv) {
    if(argc!=3 && argc!=4)return 2;
    const bool handshake=argc==4 && std::string(argv[3])=="--handshake";
    if(argc==4 && !handshake)return 2;
    try {
        Machine machine;
        machine.loadDisplayRomFile(argv[2]);
        auto mainCpu=std::make_unique<Main6809Board>(CpuType::MC6809);
        mainCpu->loadBootRomFile(argv[1]);
        mainCpu->setCpuClockHz(2000000);
        machine.addBoard(std::move(mainCpu));
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        // Both CPUs run their own machine-code programs; no host-side
        // writes to shared RAM or VRAM are used to produce the result.
        for(int i=0;i<5000;++i)machine.executeOne();
        if(handshake) {
            if(machine.displaySharedByte(0)!=0x81 ||
               machine.displaySharedByte(1)!=0x55 ||
               machine.displaySharedByte(2)!=0x66) {
                std::cerr<<"bidirectional command/ack did not complete\n";
                return 20;
            }
            if(machine.displayPhysicalVramByte(0)!=0x80 ||
               machine.displayPhysicalVramByte(0x10000)!=0x80 ||
               machine.displayPhysicalVramByte(0x20000)!=0x80 ||
               machine.captureDisplayFrame().argb[0]!=0xffffffffu) {
                std::cerr<<"command did not produce a rendered white pixel\n";
                return 21;
            }
            std::cout<<"PASS: main command -> shared RAM -> sub VRAM -> ACK -> main confirmation\n";
            return 0;
        }
        if(machine.displaySharedByte(0)!=0x80 ||
           machine.displaySharedByte(1)!=0x01) {
            std::cerr<<"main CPU did not write shared RAM\n";
            return 10;
        }
        if(machine.displayPhysicalVramByte(0)!=0x80 ||
           machine.displayPhysicalVramByte(0x10000)!=0x80 ||
           machine.displayPhysicalVramByte(0x20000)!=0x80) {
            std::cerr<<"display CPU did not write 3-plane VRAM\n";
            return 11;
        }
        auto frame=machine.captureDisplayFrame(false);
        if(frame.argb[0]!=0xffffffffu || frame.argb[1]!=0xff000000u) {
            std::cerr<<"rendered frame did not reflect sub CPU write\n";
            return 12;
        }
        std::cout<<"PASS: main ROM -> FC80 -> 9F80 -> display ROM -> VRAM -> RGB\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
