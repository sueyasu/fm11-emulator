#include "Machine.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include <algorithm>
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
        if(machine.displaySharedByte(0)!=0x66 ||
           machine.mainDisplayControl()!=0x0f)return 10;
        const auto& display=machine.displaySubsystem();
        if(display.textByte(0)!='T' || display.textByte(1)!=6 ||
           display.textByte(2)!='R' || display.textByte(3)!=0x0a)
            return 11;
        for(bool high:{false,true}) {
            auto frame=machine.captureDisplayFrame(high);
            for(auto& pixel:frame.argb) {
                unsigned index=0;
                if(pixel&0x00ff0000u)index|=1;
                if(pixel&0x0000ff00u)index|=2;
                if(pixel&0x000000ffu)index|=4;
                pixel=DisplayBitmap::referenceColor(machine.mainDisplayPalette(index));
            }
            // Verify that graphics is present before text is composited.
            if(frame.argb[0]!=0xff00c000u && frame.argb[0]!=0xff0000c0u)return 12;
            if(frame.argb[8]!=0xff0000c0u)return 13;
            DisplayBitmap::compositeText(frame,display);
            if(frame.argb[0]!=0xffc0c000u || // yellow glyph replaces blue
               frame.argb[1]!=0xff000000u || // transparent glyph background
               frame.argb[8]!=0xff0000c0u || // reverse glyph hole: blue visible
               frame.argb[9]!=0xffc00000u || // reverse background: red
               frame.argb[16]!=0xff000000u)return 14;
        }
        std::cout<<"PASS: both 6809 ROMs: blue VRAM + yellow text + reversed red, 200/400\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 20;
    }
}
