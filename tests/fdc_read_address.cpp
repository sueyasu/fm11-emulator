#include "FloppyDevice.h"
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>
#include <vector>

// A read-address result is six bytes (CHRN and CRC16), not a sector.
// Test the shared FIFO, both adapters, both FDC channels, and fast/timed.
static void load(FloppyDevice& f,unsigned ch) {
    if(ch)f.load2hd(std::vector<BYTE>(77u*2*26*256,0xe5));
    else f.load2d(std::vector<BYTE>(40u*2*16*256,0xe5));
}
static int shared(unsigned ch) {
    FloppyDevice f;
    load(f,ch);
    if(!f.beginReadAddress(ch,0,0,0))return 1;
    const BYTE id[]={0,0,1,1};
    for(unsigned i=0;i<4;++i)
        if(f.transferRead()!=id[i])return 2;
    f.transferRead();f.transferRead();
    if(f.transferStatus()!=0 || f.transferLength()!=6 ||
       f.registerValue(ch?0x32:0x1a)!=1)return 3;
    f.enableSeekTiming(true);
    if(!f.beginTypeII(ch,0,0,0,0,0xc0))return 4;
    f.advanceSeekNanoseconds(f.nextTypeIIEventNanoseconds());
    BYTE cmd=0;bool done=true;
    if(!f.takeTypeIIEvent(ch,cmd,done) || done || cmd!=0xc0)return 5;
    if(!f.beginReadAddress(ch,0,0,0))return 6;
    if(f.transferLength()!=6 || f.transferRead()!=0)return 7;
    return 0;
}
static int cpu6809(unsigned ch,bool timed) {
    Machine m;
    auto card=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=card.get();
    m.addBoard(std::move(card));
    auto f=m.systemBoard().floppyDevice();
    load(*f,ch);
    m.reset();f->enableSeekTiming(timed);
    const WORD cmd=ch?0xfd30:0xfd18;
    const WORD data=ch?0xfd33:0xfd1b;
    cpu->writeCpu(ch?0xfd35:0xfd1d,0);
    cpu->writeCpu(ch?0xfd31:0xfd19,0);
    cpu->writeCpu(ch?0xfd34:0xfd1c,0);
    // Both channels use PIO and must release DRQ at exactly six bytes.
    cpu->writeCpu(ch?0xfd51:0xfd50,0);
    cpu->writeCpu(cmd,0xc0);
    if(timed) {
        if((cpu->readCpu(cmd)&1)==0)return 1;
        m.advanceTimeNanoseconds(20000000);
    }
    const BYTE id[]={0,0,1,1};
    for(unsigned n=0;n<4;++n) {
        if(timed)m.advanceTimeNanoseconds(32000);
        const BYTE got=cpu->readCpu(data);
        if(got!=id[n])return 2;
    }
    if(timed)m.advanceTimeNanoseconds(32000);
    cpu->readCpu(data);
    if(timed)m.advanceTimeNanoseconds(32000);
    cpu->readCpu(data);
    if(!timed && (cpu->readCpu(cmd)&2))return 3;
    return 0;
}
static int cpu8088(unsigned ch,bool timed) {
    Machine m;
    auto card=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=card.get();
    m.addBoard(std::move(card));
    auto f=m.systemBoard().floppyDevice();
    load(*f,ch);
    m.reset();f->enableSeekTiming(timed);
    auto& bus=cpu->bus();
    const WORD cmd=ch?0xfd30:0xfd18;
    constexpr WORD addr=0x2400;
    bus.writeIo(ch?0xfd35:0xfd1d,0);
    bus.writeIo(ch?0xfd31:0xfd19,0);
    bus.writeIo(ch?0xfd34:0xfd1c,0);
    bus.writeIo(0x1c,0);
    bus.writeIo(WORD(0x10+ch*2),BYTE(addr));
    bus.writeIo(WORD(0x10+ch*2),BYTE(addr>>8));
    bus.writeIo(WORD(0x11+ch*2),5);
    bus.writeIo(WORD(0x11+ch*2),0);
    bus.writeIo(WORD(0x20+ch),0);
    bus.writeIo(0x1b,BYTE(0x44+ch));
    bus.writeIo(0x1a,BYTE(ch));
    bus.writeIo(cmd,0xc0);
    if(timed)m.advanceTimeNanoseconds(100000000);
    const BYTE id[]={0,0,1,1};
    for(unsigned n=0;n<4;++n)
        if(bus.readRam(addr+n)!=id[n])return 1;
    if(bus.dmaChannel(ch).count!=0xffff)return 2;
    if((bus.readIo(ch?0xfd37:0xfd1f)&0x40)==0)return 3;
    return 0;
}
int main() {
    for(unsigned ch=0;ch<2;++ch) {
        if(int r=shared(ch))return 10+ch*10+r;
        for(int timed=0;timed<2;++timed) {
            if(int r=cpu6809(ch,timed!=0))return 40+ch*20+timed*10+r;
            if(int r=cpu8088(ch,timed!=0))return 100+ch*20+timed*10+r;
        }
    }
    return 0;
}
