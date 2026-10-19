#include "Machine.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include <iostream>
#include <memory>
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    try {
        Machine m;
        m.loadDisplayRomFile(argv[2]);
        m.loadAnkRomFile(argv[3]);
        auto cpu=std::make_unique<Main6809Board>(CpuType::MC6809);
        auto* main=cpu.get();
        cpu->loadBootRomFile(argv[1]);
        cpu->setCpuClockHz(2000000);
        m.addBoard(std::move(cpu));
        m.setDisplayCpuClockHz(2000000);
        m.reset();
        if(main->readCpu(0xfd05)!=0x80 || m.displaySharedByte(1)!=0 ||
           m.mainDisplayControl()!=0)return 10;
        for(int i=0;i<200000 && m.displaySharedByte(4)!=0x77;++i)
            m.executeOne();
        if(m.displaySharedByte(4)!=0x77) {
            std::cerr<<"stage8 stalled: mainPC=$"<<std::hex<<main->cpu().pc()
                     <<" subPC=$"<<m.displayCpuPc()
                     <<" FD05=$"<<unsigned(main->readCpu(0xfd05))
                     <<" AFE0=$"<<unsigned(m.displaySubsystem().readMemory(0xafe0))
                     <<" FC81=$"<<unsigned(m.displaySharedByte(1))
                     <<" FC82=$"<<unsigned(m.displaySharedByte(2))
                     <<" FC83=$"<<unsigned(m.displaySharedByte(3))
                     <<" FDA0=$"<<unsigned(m.mainDisplayControl())
                     <<" text0=$"<<unsigned(m.displaySubsystem().textByte(0))
                     <<std::dec<<"\\n";
            return 11;
        }
        if(m.displaySharedByte(1)!=0 || m.displaySharedByte(2)!='M' ||
           m.displaySharedByte(3)!=0x66)return 12;
        // Original receive subroutine lowers READY while consuming data.
        if(m.displayBusyStatus()!=0x80 || main->readCpu(0xfd05)!=0x80)
            return 13;
        if(m.mainDisplayControl()!=3)return 14;
        const auto& d=m.displaySubsystem();
        if(d.crtcRegister(1)!=40 || d.crtcRegister(6)!=25 ||
           d.textByte(0)!='M' || d.textByte(1)!=7)return 15;
        for(bool high:{false,true}) {
            auto f=m.captureDisplayFrame(high);
            DisplayBitmap::compositeText(f,d);
            if(f.argb[0]!=0xffc0c0c0u || f.argb[1]!=0xff000000u)
                return 16;
        }
        std::cout<<"PASS: original SubRom CAA0 AFE0/FC81/FC82 handshake and text\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 20;
    }
}
