#include "Machine.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include <iostream>
#include <memory>

int main(int argc,char** argv) {
    if(argc!=4)return 2;
    try {
        Machine machine;
        machine.loadDisplayRomFile(argv[2]);
        machine.loadAnkRomFile(argv[3]);
        auto main=std::make_unique<Main6809Board>(CpuType::MC6809);
        main->loadBootRomFile(argv[1]);
        main->setCpuClockHz(2000000);
        machine.addBoard(std::move(main));
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        for(int i=0;i<200000 && machine.displaySharedByte(0)!=0x66;++i)
            machine.executeOne();
        if(machine.displaySharedByte(0)!=0x66)return 10;
        if(machine.mainDisplayControl()!=3)return 11;
        const auto& display=machine.displaySubsystem();
        if(display.crtcRegister(1)!=40 || display.crtcRegister(6)!=25)return 12;
        constexpr char expected[]="HELLO FM-11";
        for(unsigned i=0;i<sizeof(expected)-1;++i)
            if(display.textByte(i*2)!=static_cast<BYTE>(expected[i]) ||
               display.textByte(i*2+1)!=7)return 13;
        auto frame=machine.captureDisplayFrame(true);
        DisplayBitmap::compositeText(frame,display);
        if(frame.width!=640 || frame.height!=400)return 14;
        for(unsigned i=0;i<sizeof(expected)-1;++i) {
            const unsigned x=i*8;
            if(frame.argb[x]!=0xffc0c0c0u ||
               frame.argb[x+1]!=0xff000000u ||
               frame.argb[640+x+1]!=0xffc0c0c0u)return 15;
        }
        if(frame.argb[24*640]!=0xff000000u)return 16;
        std::cout<<"PASS: both 6809 ROMs -> FDA0 -> CRTC -> text RAM -> ANK -> framebuffer\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
