#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include "MdcHddDevice.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

static void issue(MdcHddDevice& d,BYTE op,std::initializer_list<BYTE> args) {
    d.write(0xc0,op);
    for(BYTE v:args)d.write(0xc1,v);
}
static std::array<BYTE,10> result(MdcHddDevice& d) {
    std::array<BYTE,10> bytes{};
    for(auto& b:bytes)b=d.read(0xc1);
    return bytes;
}
static std::vector<BYTE> medium() {
    return std::vector<BYTE>(157u*4*32*256,0x31);
}
static int addresses(bool timed) {
    MdcHddDevice d;
    d.load(medium());d.enableTiming(timed);
    unsigned reads=0;
    d.setDmaTransfer([&](unsigned,bool writing,std::vector<BYTE>& bytes) {
        if(writing)return false;
        ++reads;
        return bytes.size()==256;
    });
    // Invalid CHR values must be rejected before the DMA callback.
    for(const auto& invalid:std::array<std::array<BYTE,3>,3>{{
        {{157,0,0}},{{0,4,0}},{{0,0,32}}
    }}) {
        issue(d,0x22,{0,0,0,invalid[0],invalid[1],invalid[2],1,0});
        if(d.busy() || !d.irqPending())return 1;
        const auto r=result(d);
        if(r[0]!=0x80 || r[1]!=0x02 || r[2]!=0x80)return 2;
    }
    // Binary last-record edge: 0x4e7f is valid; 0x4e80 and
    // a two-record transfer crossing the boundary are not.
    issue(d,0x11,{0x70,0});
    if(result(d)[0]!=0x10)return 3;
    issue(d,0x22,{0,0,0,0,0x4e,0x80,1,0});
    if(result(d)[2]!=0x80)return 4;
    issue(d,0x22,{0,0,0,0,0x4e,0x7f,2,0});
    if(result(d)[2]!=0x80)return 5;
    if(reads!=0)return 6;
    issue(d,0x22,{0,0,0,0,0x4e,0x7f,1,0});
    if(timed) {
        if(!d.busy() || d.irqPending())return 7;
        d.advanceNanoseconds(200000000);
    }
    if(d.busy() || result(d)[0]!=0x10 || reads!=1)return 8;
    return 0;
}
static int writeFailure() {
    MdcHddDevice d;
    d.load(medium());d.enableTiming(true);
    unsigned dma=0,writes=0;
    d.setDmaTransfer([&](unsigned,bool writing,std::vector<BYTE>& bytes){
        if(!writing || bytes.size()!=256)return false;
        ++dma;
        std::fill(bytes.begin(),bytes.end(),BYTE(0xa5));
        return true;
    });
    d.setHostWriter([&](std::size_t offset,const BYTE* data,std::size_t n){
        ++writes;
        return offset==0 && n==256 && data[0]==0xa5;
    });
    issue(d,0x23,{0,0,0,0,0,0,2,0});
    d.advanceNanoseconds(50000000);
    const auto r=result(d);
    if(r[0]!=0x90 || r[1]!=0x40 || r[2]!=0x01 ||
       dma!=2 || writes!=2 || d.busy() || d.irqPending())return 1;
    if(d.image()[0]!=0xa5 || d.image()[255]!=0xa5 ||
       d.image()[256]!=0x31 || d.image()[511]!=0x31)return 2;
    // The next command must run after failure, not inherit old DMA.
    issue(d,0x10,{0,0});
    d.advanceNanoseconds(3000000);
    if(result(d)[0]!=0x10)return 3;
    return 0;
}
static int aborts() {
    MdcHddDevice d;
    d.load(medium());d.enableTiming(true);
    unsigned writes=0;
    d.setDmaTransfer([&](unsigned,bool writing,std::vector<BYTE>& bytes){
        if(!writing)return false;
        std::fill(bytes.begin(),bytes.end(),BYTE(0xa5));
        return true;
    });
    d.setHostWriter([&](std::size_t,const BYTE*,std::size_t){
        ++writes;return true;
    });
    issue(d,0x23,{0,0,0,0,0,0,2,0});
    d.advanceNanoseconds(9500000);
    if(d.image()[0]!=0xa5 || d.image()[256]!=0x31 || writes!=1)
        return 1;
    // Controller reset cancels the second record.
    d.write(0xc2,0x08);
    d.advanceNanoseconds(100000000);
    if(d.busy() || d.irqPending() || d.read(0xc0)!=0x80 ||
       d.image()[256]!=0x31 || writes!=1)return 2;
    d.write(0xc2,0);
    // A newer command replaces the former timed WRITE.
    issue(d,0x23,{0,0,0,0,0,1,1,0});
    if(!d.busy())return 3;
    issue(d,0x14,{0,0});
    if(d.busy() || !d.irqPending() || result(d)[0]!=0x10)return 4;
    d.advanceNanoseconds(100000000);
    if(d.image()[256]!=0x31 || writes!=1)return 5;
    // Changing to fast during a pending timed command must release BUSY.
    issue(d,0x22,{0,0,0,0,0,0,1,0});
    if(!d.busy())return 6;
    d.enableTiming(false);
    if(d.busy() || d.irqPending() || d.read(0xc0)!=0x80)return 7;
    issue(d,0x14,{0,0});
    if(result(d)[0]!=0x10)return 8;
    return 0;
}
static int short6809(bool writing) {
    Machine m;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto h=m.systemBoard().hddDevice();
    h->load(medium());
    m.reset();h->enableTiming(true);
    unsigned writes=0;
    h->setHostWriter([&](std::size_t,const BYTE*,std::size_t){
        ++writes;return true;
    });
    cpu->writeCpu(0xfd48,0x30);cpu->writeCpu(0xfd49,0);
    cpu->writeCpu(0xfd4a,0);cpu->writeCpu(0xfd4b,128);
    cpu->writeCpu(0xfd96,0);
    for(unsigned i=0;i<256;++i)cpu->writePhysical(0x3000+i,0xa5);
    cpu->writeCpu(0xfd52,writing?0x05:0x04);
    cpu->writeCpu(0xfdc2,0x20);
    cpu->writeCpu(0xfdc0,writing?0x23:0x22);
    for(BYTE v:{BYTE(0),BYTE(0),BYTE(0),BYTE(0),
                BYTE(0),BYTE(0),BYTE(1),BYTE(0)})cpu->writeCpu(0xfdc1,v);
    m.advanceTimeNanoseconds(30000000);
    if(!h->irqPending() || h->busy())return 1;
    const BYTE code=cpu->readCpu(0xfdc1);
    for(unsigned i=1;i<10;++i)cpu->readCpu(0xfdc1);
    if(code!=0x90 || h->irqPending())return 2;
    if(cpu->readCpu(0xfd4b)!=128 || cpu->readCpu(0xfd4a)!=0 ||
       h->image()[0]!=0x31 || writes)return 3;
    // HD6844 rejects the too-short block before transferring.
    if(cpu->readPhysical(0x3000)!=0xa5)return 4;
    return 0;
}
static int short8088(bool writing) {
    Machine m;
    auto board=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto h=m.systemBoard().hddDevice();
    h->load(medium());
    m.reset();h->enableTiming(true);
    auto& bus=cpu->bus();
    unsigned writes=0;
    h->setHostWriter([&](std::size_t,const BYTE*,std::size_t){
        ++writes;return true;
    });
    for(unsigned i=0;i<256;++i)bus.writeMemory(0x3000+i,0xa5);
    bus.writeIo(0x1c,0);
    bus.writeIo(0x14,0);bus.writeIo(0x14,0x30);
    bus.writeIo(0x15,127);bus.writeIo(0x15,0); // only 128 bytes
    bus.writeIo(0x22,0);
    bus.writeIo(0x1b,writing?0x4a:0x46);
    bus.writeIo(0x1a,2);
    bus.writeIo(0xfdc2,0x20);
    bus.writeIo(0xfdc0,writing?0x23:0x22);
    for(BYTE v:{BYTE(0),BYTE(0),BYTE(0),BYTE(0),
                BYTE(0),BYTE(0),BYTE(1),BYTE(0)})bus.writeIo(0xfdc1,v);
    m.advanceTimeNanoseconds(30000000);
    if(!h->irqPending() || h->busy())return 1;
    const BYTE code=bus.readIo(0xfdc1);
    for(unsigned i=1;i<10;++i)bus.readIo(0xfdc1);
    if(code!=0x90 || h->irqPending())return 2;
    if(bus.dmaChannel(2).count!=0xffff || h->image()[0]!=0x31 ||
       writes)return 3;
    // The 8237 can consume a partial READ block, but MUST NOT commit
    // a partial WRITE record when the MDC reports DMA failure.
    if(!writing && (bus.readRam(0x3000)!=0x31 ||
                    bus.readRam(0x3080)!=0xa5))return 4;
    return 0;
}
static int irqGate6809() {
    Machine m;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto h=m.systemBoard().hddDevice();
    h->load(medium());
    m.reset();h->enableTiming(true);
    cpu->writeCpu(0xfdc2,0x20); // HDD IRQ gate disabled
    cpu->writeCpu(0xfdc0,0x20); // SEEK C0/H0/R0
    for(unsigned i=0;i<8;++i)cpu->writeCpu(0xfdc1,0);
    m.advanceTimeNanoseconds(3000000);
    if(!h->irqPending() || (cpu->readCpu(0xfd2c)&4))return 1;
    cpu->writeCpu(0xfdc2,0xa0); // enable HDD IRQ gate
    if((cpu->readCpu(0xfd2c)&4)==0)return 2;
    for(unsigned i=0;i<10;++i)cpu->readCpu(0xfdc1);
    if(h->irqPending() || (cpu->readCpu(0xfd2c)&4))return 3;
    return 0;
}
int main() {
    for(unsigned timed=0;timed<2;++timed)
        if(int r=addresses(timed!=0))return 10+timed*10+r;
    if(int r=writeFailure())return 40+r;
    if(int r=aborts())return 50+r;
    for(unsigned write=0;write<2;++write) {
        if(int r=short6809(write!=0))return 60+write*10+r;
        if(int r=short8088(write!=0))return 80+write*10+r;
    }
    if(int r=irqGate6809())return 100+r;
    return 0;
}
