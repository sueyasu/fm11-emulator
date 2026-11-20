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
        auto cpu=std::make_unique<Main6809Board>(CpuType::MC6809);
        cpu->loadBootRomFile(argv[1]);
        cpu->setCpuClockHz(2000000);
        machine.addBoard(std::move(cpu));
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        for(int i=0;i<200000 && machine.displaySharedByte(0)!=0x66;++i)
            machine.executeOne();
        if(machine.displaySharedByte(0)!=0x66 || machine.mainDisplayControl()!=3)
            return 10;
        const auto& display=machine.displaySubsystem();
        const BYTE attrs[]={2,4,1,0x0a,0x27,7};
        for(unsigned i=0;i<6;++i)
            if(display.textByte(2*i+1)!=attrs[i])return 11;
        for(bool high:{false,true}) {
            auto frame=machine.captureDisplayFrame(high);
            DisplayBitmap::compositeText(frame,display);
            const std::uint32_t colors[]={
                0xffc00000u,0xff00c000u,0xff0000c0u,
                0xffc00000u,0xffffffffu,0xffc0c0c0u
            };
            for(unsigned i=0;i<6;++i) {
                const unsigned x=i*8;
                const bool reverse=i==3;
                // In 200 lines both glyph bits are lit; reverse video
                // inverts both. In 400 lines row 0 contains only bit 0.
                const auto foreground=reverse?0xff000000u:colors[i];
                const auto background=reverse?colors[i]:0xff000000u;
                if(frame.argb[x]!=foreground ||
                   frame.argb[x+1]!=(high?background:foreground) ||
                   frame.argb[x+2]!=background)
                    return 12;
                if(high && (frame.argb[640+x]!=background ||
                            frame.argb[640+x+1]!=foreground))
                    return 12;
            }
        }
        std::cout<<"PASS: 6809 text RGB, reverse video, bright and normal at 200/400 lines\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<error.what()<<"\n";
        return 13;
    }
}
