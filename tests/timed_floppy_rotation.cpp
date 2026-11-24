#include "FloppyDevice.h"
#include "Machine.h"
#include "Main6809Board.h"
#include <memory>
#include <vector>

int main() {
    FloppyDevice f;
    f.load2d(std::vector<BYTE>(40u*2*16*256,0x5a));
    f.load2hd(std::vector<BYTE>(77u*2*26*256,0xa5));
    f.enableSeekTiming(true);
    if(!f.indexPulse(0,0) || !f.indexPulse(1,0))return 1;
    f.advanceSeekNanoseconds(3000000);
    if(f.indexPulse(0,0) || f.indexPulse(1,0))return 2;
    f.advanceSeekNanoseconds(197000000);
    if(!f.indexPulse(0,0) || f.indexPulse(1,0))return 3;
    f.advanceSeekNanoseconds(133333334);
    if(!f.indexPulse(1,0))return 4;
    if(!f.beginTypeII(0,0,0,0,1,0x80))return 5;
    if(!f.typeIIBusy(0,0) || f.typeIIReady())return 6;
    f.advanceSeekNanoseconds(1000000);
    if(f.typeIIReady())return 7;
    f.advanceSeekNanoseconds(200000000);
    if(!f.typeIIReady())return 8;
    BYTE command=0;
    if(!f.takeTypeIIReady(0,command) || command!=0x80 || f.typeIIBusy(0,0))return 9;
    if(!f.beginTypeII(1,0,0,0,27,0x80))return 10;
    f.advanceSeekNanoseconds(4*f.revolutionNanoseconds(1));
    if(f.typeIIReady())return 11;
    f.advanceSeekNanoseconds(f.revolutionNanoseconds(1));
    if(!f.takeTypeIIReady(1,command))return 12;
    if(!f.beginTypeII(0,0,0,0,1,0xa0))return 13;
    f.cancelTypeII(0);
    if(f.typeIIReady() || f.typeIIBusy(0,0))return 14;
    f.enableSeekTiming(false);
    if(f.beginTypeII(0,0,0,0,1,0x80) || f.indexPulse(0,0))return 15;

    Machine machine;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    machine.addBoard(std::move(board));
    machine.systemBoard().floppyDevice()->load2hd(
        std::vector<BYTE>(77u*2*26*256,0x6c));
    machine.reset();
    machine.systemBoard().floppyDevice()->enableSeekTiming(true);
    cpu->writeCpu(0xfd35,0);
    cpu->writeCpu(0xfd34,0);
    cpu->writeCpu(0xfd31,0);
    cpu->writeCpu(0xfd32,1);
    cpu->writeCpu(0xfd51,0); // PIO
    cpu->writeCpu(0xfd30,0x80);
    if((cpu->readCpu(0xfd30)&1)==0)return 16;
    machine.advanceTimeNanoseconds(1000000);
    if((cpu->readCpu(0xfd30)&1)==0)return 17;
    machine.advanceTimeNanoseconds(200000000);
    if((cpu->readCpu(0xfd30)&2)==0)return 18;
    if(cpu->readCpu(0xfd33)!=0x6c)return 19;
    return 0;
}
