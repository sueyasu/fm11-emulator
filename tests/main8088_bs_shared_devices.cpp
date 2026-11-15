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
    machine.systemBoard().queueSerialInput('A');
    if((cpu->bus().readIo(0xfd07)&0x02)==0 ||
       cpu->bus().readIo(0xfd06)!='A') {
        std::fputs("FAIL: 8088 CPU does not see shared USART\n",stderr);
        return 3;
    }
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
