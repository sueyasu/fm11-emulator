#include "Machine.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include <cstdint>
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
        // The main CPU cannot emit a command until the sub CPU
        // has initialized CRTC and announced READY via AFE0.
        if(m.displayBusyStatus()!=0x80 || main->readCpu(0xfd05)!=0x80 ||
           m.mainDisplayControl()!=0 || m.displaySharedByte(0)!=0)return 10;
        for(int i=0;i<200000 && m.displaySharedByte(3)!=0x77;++i)
            m.executeOne();
        if(m.displaySharedByte(3)!=0x77)return 11;
        if(m.displayBusyStatus()!=0 || main->readCpu(0xfd05)!=0)return 12;
        if(m.displaySharedByte(0)!=0xa5 ||
           m.displaySharedByte(1)!='M' ||
           m.displaySharedByte(2)!=0x66)return 13;
        if(m.mainDisplayControl()!=3)return 14;
        const auto& d=m.displaySubsystem();
        if(d.crtcRegister(1)!=40 || d.crtcRegister(6)!=25 ||
           d.textByte(0)!='M' || d.textByte(1)!=7)return 15;
        for(bool high:{false,true}) {
            auto f=m.captureDisplayFrame(high);
            DisplayBitmap::compositeText(f,d);
            if(f.argb[0]!=0xffc0c0c0u ||
               f.argb[1]!=0xff000000u ||
               f.argb[640+1]!=0xffc0c0c0u)return 16;
        }
        std::cout<<"PASS: 6809 main FD05 READY -> shared request -> sub text -> ACK -> FDA0\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 20;
    }
}
