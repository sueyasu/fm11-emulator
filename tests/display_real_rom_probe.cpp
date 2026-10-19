#include "Machine.h"
#include "Main6809Board.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

// Stage 10: actual, unchanged display firmware plus synthetic main ROM.
// This is a diagnostic probe, NOT a character-output regression pass.
// It identifies which original-ROM PCs write AFE0 and whether FD05 READY
// is ever reached before trying to guess an original text command.
int main(int argc,char** argv) {
    if(argc!=3 && argc!=4) {
        std::cerr<<"usage: display_real_rom_probe main.bin display6809.bin [steps]\n";
        return 2;
    }
    try {
        const unsigned limit=argc==4?static_cast<unsigned>(std::stoul(argv[3])):500000u;
        Machine m;
        m.loadDisplayRomFile(argv[2]);
        auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
        auto* main=board.get();
        board->loadBootRomFile(argv[1]);
        board->setCpuClockHz(2000000);
        m.addBoard(std::move(board));
        m.setDisplayCpuClockHz(2000000);
        std::array<unsigned,65536> pcCounts{};
        std::uint64_t afe0Writes=0,readyWrites=0,busyWrites=0;
        unsigned logged=0;
        m.displaySubsystem().setIoWriteObserver(
            [&](DisplaySubsystem::IoWriteTrace w) {
                if(!w.fromCpu)return;
                if(w.address==0xafe0) {
                    ++afe0Writes;
                    if(w.value&1)++readyWrites;else ++busyWrites;
                    if(logged++<32)
                        std::cout<<"AFE0 @ subPC=$"<<std::hex<<w.pc
                                 <<" data=$"<<unsigned(w.value)
                                 <<" mainFD05=$"<<unsigned(main->readCpu(0xfd05))
                                 <<std::dec<<"\n";
                }
            });
        m.reset();
        bool seenReady=false;
        for(unsigned i=0;i<limit;++i) {
            ++pcCounts[m.displayCpuPc()];
            m.executeOne();
            if(main->readCpu(0xfd05)==0)seenReady=true;
            if(m.displaySharedByte(4)==0x77)break;
        }
        std::cout<<"summary: mainPC=$"<<std::hex<<main->cpu().pc()
                 <<" subPC=$"<<m.displayCpuPc()
                 <<" FD05=$"<<unsigned(main->readCpu(0xfd05))
                 <<" AFE0=$"<<unsigned(m.displaySubsystem().readMemory(0xafe0))
                 <<" FC81=$"<<unsigned(m.displaySharedByte(1))
                 <<" FC82=$"<<unsigned(m.displaySharedByte(2))
                 <<" FC83=$"<<unsigned(m.displaySharedByte(3))
                 <<" FC84=$"<<unsigned(m.displaySharedByte(4))
                 <<" FDA0=$"<<unsigned(m.mainDisplayControl())
                 <<std::dec<<"\n";
        std::cout<<"AFE0 writes="<<afe0Writes<<" READY="<<readyWrites
                 <<" BUSY="<<busyWrites<<" READY observed="<<seenReady<<"\n";
        for(unsigned rank=0;rank<8;++rank) {
            unsigned pc=0,count=0;
            for(unsigned j=0;j<pcCounts.size();++j)
                if(pcCounts[j]>count) {count=pcCounts[j];pc=j;}
            if(!count)break;
            std::cout<<"hot subPC $"<<std::hex<<pc<<std::dec
                     <<" steps="<<count<<"\n";
            pcCounts[pc]=0;
        }
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<"\n";
        return 3;
    }
}
