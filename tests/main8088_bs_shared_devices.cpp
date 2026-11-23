#include "Machine.h"
#include "Main8088Board.h"
#include <cstdio>
#include <memory>
#include <vector>

int main() {
    Machine machine;
    const auto cpuIndex=machine.addBoard(std::make_unique<Main8088Board>(CpuType::NECV20));
    machine.setActiveBoard(cpuIndex);
    if(machine.boardCount()!=1 || machine.activeCpuType()!=CpuType::NECV20) {
        std::fputs("FAIL: 8088/V20-only machine CPU selection\n",stderr);
        return 1;
    }
    // The original 8088 boot ROM reads FD23 and branches on (~DIP & F8h).
    // FFh means no boot devices, so a missing shared decode prevents
    // the ROM from ever attempting its two-sector disk IPL read.
    machine.systemBoard().setBootDip(0x9f);
    if(cpuIndex>=machine.boardCount() ||
       dynamic_cast<Main8088Board*>(&machine.board(cpuIndex))->bus().readIo(0xfd23)!=0x9f) {
        std::fputs("FAIL: 8088-only FM-11 cannot read boot-device DIP FD23\n",stderr);
        return 5;
    }
    auto floppy=machine.systemBoard().floppyDevice();
    std::vector<BYTE> image(40u*2*16*256,0xe5);
    image[0]=0x12;
    image[1]=0x34;
    floppy->load2d(image);
    floppy->setMountedDrive(0);
    auto* cpu=dynamic_cast<Main8088Board*>(&machine.board(cpuIndex));
    if(!cpu || cpu->bus().floppyImage().size()!=image.size() ||
       cpu->bus().floppyImage()[0]!=0x12) {
        std::fputs("FAIL: 8088 CPU does not see shared FDC media\n",stderr);
        return 2;
    }
    // Standard 8088 ROM: motor on/drive 0, RESTORE then SEEK,
    // finally read the first 2D sector by channel-0 8237 DMA.
    cpu->bus().writeIo(0xfd1d,0x80);
    cpu->bus().writeIo(0xfd18,0x0a); // RESTORE
    if((cpu->bus().readIo(0xfd1f)&0x40)==0 ||
       cpu->bus().readIo(0xfd19)!=0) {
        std::fputs("FAIL: Mini-FDC RESTORE/IRQ completion\n",stderr);
        return 9;
    }
    cpu->bus().writeIo(0xfd1b,2);
    cpu->bus().writeIo(0xfd18,0x1a); // SEEK to track 2
    if(cpu->bus().readIo(0xfd19)!=2 ||
       (cpu->bus().readIo(0xfd1f)&0x40)==0) {
        std::fputs("FAIL: Mini-FDC SEEK target/IRQ completion\n",stderr);
        return 10;
    }
    cpu->bus().writeIo(0xfd18,0x0a); // back to track 0
    cpu->bus().writeIo(0x1a,4); // mask DMA channel 0
    cpu->bus().writeIo(0x1c,0); // clear 8237 address flip-flop
    cpu->bus().writeIo(0x10,0x00);
    cpu->bus().writeIo(0x10,0x40); // address 4000h
    cpu->bus().writeIo(0x11,0xff);
    cpu->bus().writeIo(0x11,0x00); // 256 bytes
    cpu->bus().writeIo(0x20,0x00); // DMA bank 0
    cpu->bus().writeIo(0x1b,0x44); // CH0, I/O to memory
    cpu->bus().writeIo(0xfd1a,1); // sector 1
    cpu->bus().writeIo(0xfd1c,0); // head 0
    cpu->bus().writeIo(0x1a,0); // unmask CH0
    cpu->bus().writeIo(0xfd18,0x80); // read sector
    if(cpu->bus().readIo(0xfd18)!=0 ||
       (cpu->bus().readIo(0xfd1f)&0x40)==0 ||
       cpu->bus().readRam(0x4000)!=0x12 ||
       cpu->bus().readRam(0x4001)!=0x34) {
        std::fputs("FAIL: standard ROM 2D sector READ with drive control bits\n",stderr);
        return 11;
    }
    // Original ROM initializes display palette via FD98..FD9F.
    cpu->bus().writeIo(0xfd98,0x07);
    cpu->bus().writeIo(0xfd9f,0x03);
    if(machine.systemBoard().displayPalette(0)!=7 ||
       machine.systemBoard().displayPalette(7)!=3) {
        std::fputs("FAIL: 8088 boot ROM cannot initialize display palette\n",stderr);
        return 7;
    }
    // The original 8088 ROM issues display commands through FC81-FC84.
    // They must reach the shared display mailbox, not private card I/O RAM.
    cpu->bus().writeIo(0xfc81,0x15);
    cpu->bus().writeIo(0xfc82,0x03);
    cpu->bus().writeIo(0xfc83,0x01);
    cpu->bus().writeIo(0xfc84,'M');
    if(machine.systemBoard().readDisplayShared(1)!=0x15 ||
       machine.systemBoard().readDisplayShared(2)!=0x03 ||
       machine.systemBoard().readDisplayShared(3)!=0x01 ||
       machine.systemBoard().readDisplayShared(4)!='M' ||
       cpu->bus().readIo(0xfc84)!='M') {
        std::fputs("FAIL: 8088 boot ROM display mailbox is not shared\n",stderr);
        return 8;
    }
    // Initialize the shared 8251A before receiving from the host.
    cpu->bus().writeIo(0xfd07,0x4e); // async x16, 8N1
    cpu->bus().writeIo(0xfd07,0x05); // RxEN and TxEN
    machine.systemBoard().queueSerialInput('A');
    if((cpu->bus().readIo(0xfd07)&0x02)==0 ||
       cpu->bus().readIo(0xfd06)!='A') {
        std::fputs("FAIL: 8088 CPU does not see shared USART\n",stderr);
        return 3;
    }
    // Both CPU adapters share the same FDC sector writer:
    // the 6809 DMA path uses writeSector and the 8088 DMA path
    // completes through transferWrite.
    unsigned completed=0;
    std::size_t lastOffset=9999;
    floppy->setSectorWriter([&](unsigned channel,unsigned drive,std::size_t offset,
                                 const BYTE* data,std::size_t length) {
        if(channel!=0 || drive!=0 || length!=256 || data[0]!=0x5a)return false;
        ++completed;
        lastOffset=offset;
        return true;
    });
    std::vector<BYTE> sector(256,0);
    sector[0]=0x5a;
    if(!floppy->writeSector(0,256,sector.data()) ||
       completed!=1 || lastOffset!=256 || floppy->read(0,256)!=0x5a) {
        std::fputs("FAIL: 6809 DMA shared sector commit\n",stderr);
        return 12;
    }
    if(!floppy->beginSector(0,0,0,3,0,true))return 13;
    for(BYTE value:sector)floppy->transferWrite(value);
    if(completed!=2 || lastOffset!=512 || floppy->transferStatus()!=0 ||
       floppy->read(0,512)!=0x5a) {
        std::fputs("FAIL: 8088 DMA shared sector commit\n",stderr);
        return 14;
    }
    floppy->setSectorWriter([](unsigned,unsigned,std::size_t,const BYTE*,std::size_t) {
        return false;
    });
    sector[1]=0x7e;
    if(floppy->writeSector(0,256,sector.data()) ||
       floppy->read(0,257)==0x7e || floppy->transferStatus()!=0x10) {
        std::fputs("FAIL: failed host image write was acknowledged\n",stderr);
        return 15;
    }
    floppy->setSectorWriter({});
    machine.reset();
    if(cpu->bus().readIo(0xfd23)!=0x9f) {
        std::fputs("FAIL: FD23 boot-device DIP lost on reset\n",stderr);
        return 6;
    }
    if(cpu->bus().floppyImage().size()!=image.size() ||
       cpu->bus().floppyImage()[1]!=0x34) {
        std::fputs("FAIL: shared FDC media lost on reset\n",stderr);
        return 4;
    }
    std::puts("PASS: FM-11 BS V20-only CPU with shared FDC and USART");
    return 0;
}
