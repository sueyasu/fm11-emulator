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
        for(int i=0;i<200000;++i) {
            machine.executeOne();
            if(machine.displaySharedByte(2)==0x66)break;
        }
        if(machine.displaySharedByte(0)!=0x81 ||
           machine.displaySharedByte(1)!=0x55 ||
           machine.displaySharedByte(2)!=0x66) {
            std::cerr<<"full-frame binary command/ACK did not finish\n";
            return 10;
        }
        const auto frame=machine.captureDisplayFrame(false);
        if(frame.width!=640 || frame.height!=200 ||
           frame.argb.size()!=640*200)return 11;
        for(std::size_t y=0;y<200;++y)
            for(std::size_t x=0;x<640;++x) {
                const auto expected=(x%2==0)?0xffffffffu:0xff000000u;
                if(frame.argb[y*640+x]!=expected) {
                    std::cerr<<"pixel mismatch at "<<x<<","<<y<<"\n";
                    return 12;
                }
            }
        std::cout<<"PASS: main command -> display full 640x200 VRAM -> RGB -> ACK\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
