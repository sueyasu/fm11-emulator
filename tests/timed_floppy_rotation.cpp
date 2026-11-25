#include "FloppyDevice.h"
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
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
    bool completed=false;
    if(!f.takeTypeIIEvent(0,command,completed) || command!=0x80 ||
       completed || !f.typeIITransferring(0,0))return 9;
    if(!f.takeTypeIIEvent(0,command,completed) || !completed ||
       f.typeIIBusy(0,0))return 9;
    if(!f.beginTypeII(1,0,0,0,27,0x80))return 10;
    f.advanceSeekNanoseconds(4*f.revolutionNanoseconds(1));
    if(f.typeIIReady())return 11;
    f.advanceSeekNanoseconds(f.revolutionNanoseconds(1));
    if(!f.takeTypeIIEvent(1,command,completed) || !completed ||
       f.typeIIBusy(1,0) || f.typeIIResult(1)!=0x10)return 12;
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
    // At 500 kb/s each successive 2HD byte gets its own 16 us DRQ.
    // Poll and consume every byte, as the boot ROM must do.
    unsigned pioBytes=0;
    for(unsigned i=0;i<40000 && pioBytes<256;++i) {
        machine.advanceTimeNanoseconds(8000);
        if(cpu->readCpu(0xfd30)&0x02) {
            if(cpu->readCpu(0xfd33)!=0x6c)return 19;
            ++pioBytes;
        }
    }
    if(pioBytes!=256)return 18;
    machine.advanceTimeNanoseconds(20000);
    if(cpu->readCpu(0xfd30)!=0)return 27;
    // An absent ID does not return RNF before five revolutions.
    cpu->writeCpu(0xfd32,27);
    cpu->writeCpu(0xfd30,0x80);
    machine.advanceTimeNanoseconds(4*FloppyDevice::revolutionNanoseconds(1));
    if((cpu->readCpu(0xfd30)&1)==0)return 22;
    machine.advanceTimeNanoseconds(FloppyDevice::revolutionNanoseconds(1));
    if((cpu->readCpu(0xfd30)&0x10)==0)return 23;
    // Force Interrupt aborts a pending rotational wait.
    cpu->writeCpu(0xfd32,1);
    cpu->writeCpu(0xfd30,0x80);
    cpu->writeCpu(0xfd30,0xd0);
    machine.advanceTimeNanoseconds(200000000);
    if(cpu->readCpu(0xfd30)&1)return 24;

    // The same scheduler must defer 8088 8237 DMA transfers.
    Machine machine86;
    auto board86=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu86=board86.get();
    machine86.addBoard(std::move(board86));
    std::vector<BYTE> image(40u*2*16*256,0);
    image[0]=0x73;
    machine86.systemBoard().floppyDevice()->load2d(image);
    machine86.reset();
    machine86.systemBoard().floppyDevice()->enableSeekTiming(true);
    auto& bus=cpu86->bus();
    bus.writeIo(0xfd19,0);
    bus.writeIo(0xfd1a,1);
    bus.writeIo(0xfd1c,0);
    bus.writeIo(0xfd1d,0);
    bus.writeIo(0x1c,0); // reset 8237 address/count flip-flop
    bus.writeIo(0x10,0);
    bus.writeIo(0x10,0x20);
    bus.writeIo(0x11,0xff);
    bus.writeIo(0x11,0);
    bus.writeIo(0x1b,0x44);
    bus.writeIo(0x1a,0);
    bus.writeIo(0xfd18,0x80);
    if((bus.readIo(0xfd18)&1)==0 || bus.readRam(0x2000)==0x73)return 20;
    machine86.advanceTimeNanoseconds(3000000);
    if(bus.readRam(0x2000)==0x73 || (bus.readIo(0xfd18)&1)==0)return 28;
    for(unsigned i=0;i<1000 && bus.readRam(0x2000)!=0x73;++i)
        machine86.advanceTimeNanoseconds(8000);
    if(bus.readRam(0x2000)!=0x73 || (bus.readIo(0xfd18)&1)==0)return 29;
    for(unsigned i=0;i<30000;++i)machine86.advanceTimeNanoseconds(8000);
    if(bus.readRam(0x2000)!=0x73 || bus.readIo(0xfd18)!=0)return 21;
    // WRITE SECTOR must not modify backing media until timed completion.
    for(unsigned i=0;i<256;++i)bus.writeMemory(0x2100+i,0x42);
    bus.writeIo(0xfd1a,2);
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0);
    bus.writeIo(0x10,0x21);
    bus.writeIo(0x11,0xff);
    bus.writeIo(0x11,0);
    bus.writeIo(0x1b,0x48);
    bus.writeIo(0x1a,0);
    bus.writeIo(0xfd18,0xa0);
    if(machine86.systemBoard().floppyDevice()->raw2d()[256]!=0)return 25;
    for(unsigned i=0;i<30000;++i)machine86.advanceTimeNanoseconds(8000);
    if(machine86.systemBoard().floppyDevice()->raw2d()[256]!=0x42)return 26;
    return 0;
}
