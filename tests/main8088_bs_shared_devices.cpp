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
    machine.systemBoard().queueSerialInput('A');
    if((cpu->bus().readIo(0xfd07)&0x02)==0 ||
       cpu->bus().readIo(0xfd06)!='A') {
        std::fputs("FAIL: 8088 CPU does not see shared USART\n",stderr);
        return 3;
    }
    machine.reset();
    if(cpu->bus().floppyImage().size()!=image.size() ||
       cpu->bus().floppyImage()[1]!=0x34) {
        std::fputs("FAIL: shared FDC media lost on reset\n",stderr);
        return 4;
    }
    std::puts("PASS: FM-11 BS V20-only CPU with shared FDC and USART");
    return 0;
}
