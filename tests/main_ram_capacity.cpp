#include "Main6809Board.h"
#include "Main8088Bus.h"
#include "MainZ80Board.h"
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

static void programDma(Main8088Bus& bus,unsigned channel,WORD address,WORD count)
{
    bus.writeIo(0x1c,0);
    bus.writeIo(WORD(0x10+2*channel),BYTE(address));
    bus.writeIo(WORD(0x10+2*channel),BYTE(address>>8));
    bus.writeIo(WORD(0x11+2*channel),BYTE(count));
    bus.writeIo(WORD(0x11+2*channel),BYTE(count>>8));
}

int main()
{
    constexpr std::array<unsigned,8> sizes{128,256,384,512,640,768,896,1024};
    for(const auto kib:sizes) {
        const auto bytes=std::uint32_t(kib)*1024;
        Main6809Board cpu(CpuType::MC6809);
        cpu.setRamKiB(kib);
        if(cpu.ramKiB()!=kib)return 1;

        // The last installed byte must be unique and the first missing
        // page must alias the corresponding page near physical address 0.
        cpu.writePhysical(0x100,0x23);
        cpu.writePhysical(bytes-1,0x45);
        if(cpu.readPhysical(0x100)!=0x23 ||
           cpu.readPhysical(bytes-1)!=0x45)return 2;
        if(bytes < 0x100000) {
            if(cpu.readPhysical(bytes+0x100)!=0x23)return 3;
            cpu.writePhysical(bytes+0x100,0x5a);
            if(cpu.readPhysical(0x100)!=0x5a)return 4;
        }

        // MMR enabled: logical $0000 maps to hardware physical page
        // at the first unpopulated address. This must alias page 0.
        // The page cache must also react to MMR register writes.
        if(bytes < 0x100000) {
            cpu.writeCpu(0xfd80,BYTE(bytes>>12));
            cpu.writeCpu(0xfd93,0x81);
            cpu.writeCpu(0x100,0xa6);
            if(cpu.readPhysical(0x100)!=0xa6)return 5;
            cpu.writeCpu(0xfd80,0x00);
            if(cpu.readCpu(0x100)!=0xa6)return 6;
        }
        cpu.reset();
        cpu.writeCpu(0x100,0x77);
        if(cpu.readPhysical(0x100)!=0x77)return 7;

        // FM-11 6844 DMA0 FDD -> RAM must obey the same decoder.
        if(bytes < 0x100000) {
            std::vector<BYTE> floppy(40u*2*16*256,0);
            floppy[0]=0x88;
            cpu.loadFloppy2d(floppy);
            cpu.writeCpu(0xfd19,0);
            cpu.writeCpu(0xfd1a,1);
            cpu.writeCpu(0xfd1c,0);
            cpu.writeCpu(0xfd1d,0);
            cpu.writeCpu(0xfd94,BYTE(bytes>>16));
            cpu.writeCpu(0xfd40,0x01);
            cpu.writeCpu(0xfd41,0x00);
            cpu.writeCpu(0xfd42,0x01);
            cpu.writeCpu(0xfd43,0x00);
            cpu.writeCpu(0xfd50,0x04);
            cpu.writeCpu(0xfd18,0x80);
            if(cpu.readPhysical(0x100)!=0x88)return 21;
        }

        // The optional Z80 shares the 6809 backing store. Its banked
        // accesses should see the same mirror rather than a full 1 MiB.
        MainZ80Board z80;
        z80.setPhysicalMemory(cpu.physicalMemory());
        z80.setRamKiB(kib);
        if(z80.ramKiB()!=kib)return 8;
        z80.writeMemory(0x4001,0xbc);
        if(cpu.readPhysical(0x4001)!=0xbc)return 9;
        // Bank selection is via FD2F; an absent bank aliases to RAM.
        if(bytes<=0x80000) {
            z80.writeIo(0xfd2f,BYTE(bytes>>16));
            z80.writeMemory(0x4001,0xcd);
            if(cpu.readPhysical(0x4001)!=0xcd)return 10;
        }

        Main8088Bus bus;
        bus.setRamKiB(kib);
        if(bus.ramKiB()!=kib)return 11;
        bus.writeMemory(0x100,0x36);
        bus.writeMemory(bytes-1,0x79);
        if(bus.readRam(0x100)!=0x36 || bus.readRam(bytes-1)!=0x79)return 12;
        if(bytes < 0x100000) {
            if(bus.readMemory(bytes+0x100)!=0x36)return 13;
            bus.writeMemory(bytes+0x100,0x62);
            if(bus.readRam(0x100)!=0x62)return 14;
        }
        // The 4 KiB reset ROM takes precedence on CPU reads even when
        // its underlying physical RAM aliases the low end of memory.
        Main8088Bus::Rom rom{};
        rom[0]=0xe9;
        bus.loadBootRom(rom);
        bus.writeMemory(0xff000,0x12);
        if(bus.readMemory(0xff000)!=0xe9 ||
           bus.readRam(0xff000)!=0x12)return 15;

        // DMA2 I/O->RAM must honor the same decoder as CPU accesses.
        if(bytes<0x100000) {
            bus.reset();
            bus.setDmaDevice(2,[](){return BYTE(0x9e);},[](BYTE){});
            programDma(bus,2,0x0100,0);
            bus.writeIo(0x22,BYTE(bytes>>16));
            bus.writeIo(0x1b,0x46);
            bus.writeIo(0x1a,2);
            bus.writeIo(0x19,6); // software request, DMA channel 2
            if(!bus.serviceDma() || bus.readRam(0x100)!=0x9e)return 16;
        }
    }

    Main6809Board cpu(CpuType::MC6809);
    Main8088Bus bus;
    MainZ80Board z80;
    if(cpu.ramKiB()!=1024 || bus.ramKiB()!=1024 || z80.ramKiB()!=1024)return 17;
    for(const auto invalid:{0u,64u,192u,1152u}) {
        bool rejected=false;
        try {cpu.setRamKiB(invalid);} catch(const std::invalid_argument&) {rejected=true;}
        if(!rejected)return 18;
        rejected=false;
        try {bus.setRamKiB(invalid);} catch(const std::invalid_argument&) {rejected=true;}
        if(!rejected)return 19;
        rejected=false;
        try {z80.setRamKiB(invalid);} catch(const std::invalid_argument&) {rejected=true;}
        if(!rejected)return 20;
    }
    return 0;
}
