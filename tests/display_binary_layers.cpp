#include "Machine.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

int main(int argc,char** argv) {
    if(argc!=5)return 2;
    try {
        const std::string mode=argv[4];
        const BYTE expected=mode=="text"?3:mode=="graphics"?12:mode=="both"?15:255;
        if(expected==255)return 3;
        Machine m;
        m.loadDisplayRomFile(argv[2]);
        m.loadAnkRomFile(argv[3]);
        auto main=std::make_unique<Main6809Board>(CpuType::MC6809);
        main->loadBootRomFile(argv[1]);
        main->setCpuClockHz(2000000);
        m.addBoard(std::move(main));
        m.setDisplayCpuClockHz(2000000);
        m.reset();
        for(int i=0;i<200000 && m.displaySharedByte(0)!=0x66;++i)m.executeOne();
        if(m.displaySharedByte(0)!=0x66 || m.mainDisplayControl()!=expected ||
           m.mainDisplayPalette(4)!=1)return 10;
        if(m.displaySubsystem().textByte(0)!='L' ||
           m.displaySubsystem().textByte(1)!=6)return 11;
        for(bool high:{false,true}) {
            auto f=m.captureDisplayFrame(high);
            if(!(expected&0x0c))
                std::fill(f.argb.begin(),f.argb.end(),0xff000000u);
            else {
                for(auto& pixel:f.argb) {
                    unsigned index=0;
                    if(pixel&0x00ff0000u)index|=1;
                    if(pixel&0x0000ff00u)index|=2;
                    if(pixel&0x000000ffu)index|=4;
                    pixel=DisplayBitmap::referenceColor(m.mainDisplayPalette(index));
                }
            }
            if(expected&3)
                DisplayBitmap::compositeText(f,m.displaySubsystem());
            const std::uint32_t blue=0xff0000c0u, yellow=0xffc0c000u, black=0xff000000u;
            if(f.argb[0]!=(expected&3?yellow:blue))return 12;
            if(f.argb[1]!=(expected&12?black:black))return 13;
            // Well away from text: a broad stripe field is present only if
            // graphics is enabled. The alternating blue pixel at x=160
            // would be invisible in text-only mode.
            if(f.argb[160]!=(expected&12?blue:black) ||
               f.argb[161]!=black)return 14;
            if(f.argb[640*40+160]!=(expected&12?blue:black))return 15;
        }
        std::cout<<"PASS: FDA0 "<<mode<<" layer gating with 6809 text and full-frame VRAM\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<error.what()<<"\n";
        return 20;
    }
}
