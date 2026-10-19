#include "Machine.h"
#include "Main6809Board.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Full ROM boot without SDL: this intentionally uses the same Machine,
// display CPU scheduler and shared floppy device as the SDL frontend.
// The historical fm11-headless --floppy instead jumps straight to the
// disk IPL at $0400, bypassing both hardware ROMs.
namespace {
std::vector<BYTE> readFile(const std::string& path) {
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("cannot open: "+path);
    return std::vector<BYTE>(std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>());
}
std::uint64_t numeric(const std::string& value) {
    std::size_t used=0;
    const auto n=std::stoull(value,&used,0);
    if(used!=value.size())throw std::invalid_argument("invalid number: "+value);
    return n;
}
void usage() {
    std::cerr<<"usage: fm11-romboot --floppy-d88 FILE [--main-rom FILE]"
       <<" [--sub-rom FILE] [--instructions N] [--report-every N]\\n"
       <<"ROM defaults: external/roms/boot6809.bin and display6809.bin\\n"
       <<"Full 6809 ROM boot, no SDL; serial TX appears on stdout.\\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string mainPath="external/roms/boot6809.bin";
        std::string subPath="external/roms/display6809.bin";
        std::string floppyPath;
        std::uint64_t limit=20000000, reportEvery=2000000;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--help"){usage();return 0;}
            if(i+1>=argc){usage();return 2;}
            const std::string value=argv[++i];
            if(arg=="--floppy-d88")floppyPath=value;
            else if(arg=="--main-rom")mainPath=value;
            else if(arg=="--sub-rom")subPath=value;
            else if(arg=="--instructions")limit=numeric(value);
            else if(arg=="--report-every")reportEvery=numeric(value);
            else {usage();return 2;}
        }
        if(floppyPath.empty()){usage();return 2;}
        Machine machine;
        machine.loadDisplayRomFile(subPath);
        auto cpu=std::make_unique<Main6809Board>(CpuType::MC6809);
        cpu->loadBootRomFile(mainPath);
        cpu->setCpuClockHz(2000000);
        auto* main=cpu.get();
        machine.addBoard(std::move(cpu));
        // Must be after addBoard(): it replaces the CPU-local FDC with
        // the machine's shared FloppyDevice.
        const auto disk=readFile(floppyPath);
        if(disk.size()==327680)main->loadFloppy2d(disk);
        else main->loadFloppyD88(disk);
        machine.setDisplayCpuClockHz(2000000);
        machine.reset();
        if(!main->floppyImage().size())
            throw std::runtime_error("floppy was not mounted");
        std::cerr<<"ROM boot: disk bytes="<<disk.size()
                 <<" image bytes="<<main->floppyImage().size()
                 <<" mainPC=$"<<std::hex<<main->cpu().pc()
                 <<" subPC=$"<<machine.displayCpuPc()<<std::dec<<"\\n";
        // Optional FDC access trace is at register level; unlike PC-only
        // logging it records read commands and whether the FDC reports DRQ.
        // No CPU bus reads are performed by diagnostics.
        std::uint64_t serialBytes=0;
        for(std::uint64_t i=0;i<limit;++i) {
            machine.executeOne();
            BYTE ch=0;
            while(main->takeSerialOutput(ch)) {
                std::cout.put(static_cast<char>(ch));
                ++serialBytes;
            }
            if(reportEvery && (i+1)%reportEvery==0) {
                std::cerr<<"steps="<<(i+1)
                         <<" mainPC=$"<<std::hex<<main->cpu().pc()
                         <<" subPC=$"<<machine.displayCpuPc()
                         <<std::dec<<" serial="<<serialBytes
                         <<" busy="<<unsigned(machine.displayBusyStatus())
                         <<" displayWrites="<<machine.displaySharedWriteRevision()
                         <<" halted="<<machine.displayHalted()
                         <<" fdcReads="<<main->fdcReadCommands()
                         <<" fdcOK="<<main->fdcSuccessfulReads()
                         <<" fdcFail="<<main->fdcFailedReads()
                         <<" fdcStatus="<<unsigned(main->fdcStatus())
                         <<" CHS="<<unsigned(main->fdcTrack())<<":"<<unsigned(main->fdcSector())
                         <<" drive="<<unsigned(main->fdcDrive())<<"\\n";
            }
        }
        std::cout.flush();
        std::cerr<<"FDxx accesses (nonzero, register:read/write[last]):";
        for(unsigned reg=0;reg<256;++reg) {
            const auto reads=main->ioReadCount(reg);
            const auto writes=main->ioWriteCount(reg);
            if(!reads && !writes)continue;
            std::cerr<<" "<<std::hex<<std::setw(2)<<std::setfill('0')
                     <<reg<<std::dec<<":"<<reads<<"/"<<writes;
            if(writes)std::cerr<<"[$"<<std::hex<<std::setw(2)
                               <<unsigned(main->lastIoWrite(reg))<<std::dec<<"]";
        }
        std::cerr<<"\\n";
        std::cerr<<"ROM boot instruction limit reached: "<<limit
                 <<" mainPC=$"<<std::hex<<main->cpu().pc()
                 <<" subPC=$"<<machine.displayCpuPc()<<std::dec<<"\\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"fm11-romboot: "<<e.what()<<"\\n";
        return 1;
    }
}
