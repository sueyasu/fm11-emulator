#include "Machine.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include <iostream>
#include <memory>
#include <string>

int main(int argc,char** argv) {
    if(argc!=5)return 2;
    try {
        const std::string mode=argv[4];
        if(mode!="top" && mode!="scroll")return 3;
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
        if(machine.displaySharedByte(0)!=0x66 || machine.mainDisplayControl()!=3)
            return 10;
        const auto& display=machine.displaySubsystem();
        if(display.crtcRegister(1)!=40 || display.crtcRegister(6)!=25 ||
           display.crtcRegister(13)!=(mode=="top"?0:80))return 11;
        for(const auto& [line,word]:std::initializer_list<std::pair<unsigned,std::string>>{
                {0,"FIRST LINE"},{1,"SECOND LINE"}})
            for(unsigned i=0;i<word.size();++i) {
                const auto pos=(line*80+i)*2;
                if(display.textByte(pos)!=static_cast<BYTE>(word[i]) ||
                   display.textByte(pos+1)!=7)return 12;
            }
        for(bool high:{false,true}) {
            auto frame=machine.captureDisplayFrame(high);
            DisplayBitmap::compositeText(frame,display);
            const bool shifted=mode=="scroll";
            const auto white=0xffc0c0c0u, black=0xff000000u;
            // F has bit 0 set, S has bit 1 set. The same RAM is visible
            // at different screen coordinates solely from CRTC R13.
            if(frame.argb[0]!=(shifted?black:white) ||
               frame.argb[1]!=(shifted?white:black))return 13;
            if(!shifted && frame.argb[(high?16u:8u)*640+1]!=white)return 14;
            if(shifted && frame.argb[(high?16u:8u)*640]!=black)return 15;
        }
        std::cout<<"PASS: CRTC display start "<<mode
                 <<" with unchanged CPU-written text RAM (200/400)\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 20;
    }
}
