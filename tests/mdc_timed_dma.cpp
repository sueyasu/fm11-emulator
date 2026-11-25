#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>
#include <vector>
#include <initializer_list>

static int run6809(bool writing) {
    Machine m;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto h=m.systemBoard().hddDevice();
    h->load(std::vector<BYTE>(157u*4*32*256,0x31));
    m.reset();h->enableTiming(true);
    unsigned persisted=0;
    h->setHostWriter([&](std::size_t,const BYTE*,std::size_t length){
        if(length!=256)return false;
        ++persisted;return true;
    });
    cpu->writeCpu(0xfd48,0x30);cpu->writeCpu(0xfd49,0);
    cpu->writeCpu(0xfd4a,2);cpu->writeCpu(0xfd4b,0);
    cpu->writeCpu(0xfd96,0);
    for(unsigned j=0;j<512;++j)
        cpu->writePhysical(0x3000+j,writing?BYTE(j<256?0xa5:0xb6):0x00);
    cpu->writeCpu(0xfd52,writing?0x05:0x04);
    cpu->writeCpu(0xfdc2,0x20); // physical drive 0 / DMA channel 2
    cpu->writeCpu(0xfdc0,writing?0x23:0x22);
    for(BYTE p:{BYTE(0),BYTE(0),BYTE(0),BYTE(0),
                BYTE(0),BYTE(0),BYTE(2),BYTE(0)})
        cpu->writeCpu(0xfdc1,p);
    if(!h->busy() || h->irqPending() || h->read(0xc0)!=0x01)return 1;
    m.advanceTimeNanoseconds(8000000);
    if(cpu->readCpu(0xfd4a)!=2 || cpu->readCpu(0xfd4b)!=0 ||
       h->irqPending())return 2;
    m.advanceTimeNanoseconds(1500000); // first record at 9.333 ms
    if(cpu->readCpu(0xfd4a)!=1 || cpu->readCpu(0xfd4b)!=0 ||
       !h->busy() || h->irqPending())return 3;
    if(writing) {
        if(h->image()[0]!=0xa5 || h->image()[256]!=0x31 ||
           persisted!=1)return 4;
    } else if(cpu->readPhysical(0x3000)!=0x31 ||
              cpu->readPhysical(0x3100)!=0)return 4;

    if(writing) {
        cpu->writeCpu(0xfdc2,0x28); // reset during second record
        m.advanceTimeNanoseconds(100000000);
        if(h->busy() || h->irqPending() || h->image()[256]!=0x31 ||
           persisted!=1)return 5;
    } else {
        m.advanceTimeNanoseconds(1000000);
        if(h->busy() || !h->irqPending() || cpu->readCpu(0xfd4a)!=0 ||
           cpu->readCpu(0xfd4b)!=0)return 6;
        if(cpu->readPhysical(0x3100)!=0x31)return 7;
        if(cpu->readCpu(0xfdc1)!=0x10)return 8;
        for(unsigned j=1;j<10;++j)cpu->readCpu(0xfdc1);
        if(h->irqPending() || h->read(0xc0)!=0x80)return 9;
    }
    return 0;
}
static int run8088(bool writing) {
    Machine m;
    auto board=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto h=m.systemBoard().hddDevice();
    h->load(std::vector<BYTE>(157u*4*32*256,0x31));
    m.reset();h->enableTiming(true);
    auto& bus=cpu->bus();
    unsigned persisted=0;
    h->setHostWriter([&](std::size_t,const BYTE*,std::size_t len){
        if(len!=256)return false;
        ++persisted;return true;
    });
    for(unsigned j=0;j<512;++j)
        bus.writeMemory(0x3000+j,writing?BYTE(j<256?0xa5:0xb6):0);
    bus.writeIo(0x1c,0);
    bus.writeIo(0x14,0);bus.writeIo(0x14,0x30);
    bus.writeIo(0x15,0xff);bus.writeIo(0x15,0x01);
    bus.writeIo(0x22,0);
    bus.writeIo(0x1b,writing?0x4a:0x46);
    bus.writeIo(0x1a,2);
    bus.writeIo(0xfdc2,0x20);
    bus.writeIo(0xfdc0,writing?0x23:0x22);
    for(BYTE p:{BYTE(0),BYTE(0),BYTE(0),BYTE(0),
                BYTE(0),BYTE(0),BYTE(2),BYTE(0)})
        bus.writeIo(0xfdc1,p);
    if(!h->busy() || h->irqPending())return 1;
    m.advanceTimeNanoseconds(8000000);
    if(bus.dmaChannel(2).count!=0x01ff || h->irqPending())return 2;
    m.advanceTimeNanoseconds(1500000);
    if(bus.dmaChannel(2).count!=0x00ff || h->irqPending())return 3;
    if(writing) {
        if(h->image()[0]!=0xa5 || h->image()[256]!=0x31 ||
           persisted!=1)return 4;
        bus.writeIo(0xfdc2,0x28);
        m.advanceTimeNanoseconds(100000000);
        if(h->busy() || h->irqPending() || h->image()[256]!=0x31 ||
           persisted!=1)return 5;
    } else {
        if(bus.readRam(0x3000)!=0x31 || bus.readRam(0x3100)!=0)return 4;
        m.advanceTimeNanoseconds(1000000);
        if(h->busy() || !h->irqPending() ||
           bus.dmaChannel(2).count!=0xffff ||
           bus.readRam(0x3100)!=0x31)return 6;
        if(bus.readIo(0xfdc1)!=0x10)return 7;
        for(unsigned j=1;j<10;++j)bus.readIo(0xfdc1);
        if(h->irqPending())return 8;
    }
    return 0;
}
int main() {
    if(int r=run6809(false))return 10+r;
    if(int r=run6809(true))return 20+r;
    if(int r=run8088(false))return 30+r;
    if(int r=run8088(true))return 40+r;
    return 0;
}
