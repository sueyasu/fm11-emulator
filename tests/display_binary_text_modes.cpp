#include "Machine.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include <cstdlib>
#include <iostream>
#include <memory>

int main(int argc,char** argv) {
    if(argc!=5)return 2;
    try {
        const unsigned columns=static_cast<unsigned>(std::stoul(argv[4]));
        if(columns!=40 && columns!=80)return 3;
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
        if(machine.displaySharedByte(0)!=0x66 ||
           machine.mainDisplayControl()!=3)return 10;
        const auto& d=machine.displaySubsystem();
        if(d.crtcRegister(1)!=columns/2 || d.crtcRegister(6)!=25)return 11;
        const char text[]="40/80 COL TEST";
        for(unsigned i=0;i<sizeof(text)-1;++i)
            if(d.textByte(i*2)!=static_cast<BYTE>(text[i]) ||
               d.textByte(i*2+1)!=7)return 12;
        for(bool high:{false,true}) {
            auto f=machine.captureDisplayFrame(high);
            DisplayBitmap::compositeText(f,d);
            const unsigned width=640/columns;
            const unsigned height=high?400:200;
            if(f.width!=640 || f.height!=height)return 13;
            // Test font has bit 0 in row 0: 1 pixel wide in 80-column,
            // 2 pixels wide in 40-column, in both 200/400-line scanout.
            for(unsigned i=0;i<sizeof(text)-1;++i) {
                const unsigned x=i*width;
                if(text[i]==' ') {
                    if(f.argb[x]!=0xff000000u)return 14;
                    continue;
                }
                if(f.argb[x]!=0xffc0c0c0u ||
                   f.argb[x+width-1]!=0xff000000u)return 15;
                if(columns==40 && f.argb[x+1]!=0xffc0c0c0u)return 16;
                if(columns==80 && f.argb[x+1]!=0xff000000u)return 17;
            }
            if(f.argb[640*(height-1)]!=0xff000000u)return 18;
        }
        std::cout<<"PASS: "<<columns<<" columns, 200 and 400 lines, actual 6809 ROMs\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 19;
    }
}
