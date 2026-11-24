#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>
#include <vector>

// 2HD ROM-style RESTORE -> PIO READ -> SEEK flow with virtual elapsed time.
int main() {
    Machine machine;
    auto card=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=card.get();
    machine.addBoard(std::move(card));
    machine.systemBoard().floppyDevice()->load2hd(
        std::vector<BYTE>(77u*2*26*256,0x5a));
    machine.reset();
    auto fdc=machine.systemBoard().floppyDevice();
    fdc->enableSeekTiming(true);
    cpu->writeCpu(0xfd35,0);
    cpu->writeCpu(0xfd34,0);
    cpu->writeCpu(0xfd31,0);
    cpu->writeCpu(0xfd32,1);
    cpu->writeCpu(0xfd30,0x08); // Restore at the 2HD controller
    if((cpu->readCpu(0xfd30)&1)==0)return 1;
    machine.advanceTimeNanoseconds(20000000);
    if((cpu->readCpu(0xfd30)&1)==0)return 2;
    machine.advanceTimeNanoseconds(10000000);
    // Completion must clear BUSY (bit 0); IRQ appears on FD37 instead.
    if(cpu->readCpu(0xfd30)&1)return 3;
    if((cpu->readCpu(0xfd37)&0x40)==0)return 4;
    cpu->writeCpu(0xfd51,0); // PIO
    cpu->writeCpu(0xfd30,0x80); // READ sector at track 0
    if((cpu->readCpu(0xfd30)&1)==0)return 5;
    machine.advanceTimeNanoseconds(200000000);
    if((cpu->readCpu(0xfd30)&2)==0)return 5;
    for(unsigned i=0;i<256;++i)
        if(cpu->readCpu(0xfd33)!=0x5a)return 6;
    // Seek-clock progress must not erase a non-SEEK completion IRQ.
    machine.advanceTimeNanoseconds(1000000);
    if((cpu->readCpu(0xfd37)&0x40)==0)return 7;
    cpu->writeCpu(0xfd33,1); // SEEK target
    cpu->writeCpu(0xfd30,0x10);
    if((cpu->readCpu(0xfd30)&1)==0)return 8;
    machine.advanceTimeNanoseconds(50000000);
    if((cpu->readCpu(0xfd30)&1)!=0 ||
       cpu->readCpu(0xfd31)!=1 || fdc->headTrack(1,0)!=1)return 9;
    if((cpu->readCpu(0xfd37)&0x40)==0)return 10;
    // Writing the track register alone cannot teleport the head.
    cpu->writeCpu(0xfd31,0);
    cpu->writeCpu(0xfd30,0x80);
    if((cpu->readCpu(0xfd30)&0x10)==0)return 11;
    return 0;
}
