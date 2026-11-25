#include "FloppyDevice.h"
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <cstdint>
#include <memory>
#include <vector>

static void mount(FloppyDevice& f,unsigned ch) {
    if(ch)f.load2hd(std::vector<BYTE>(77u*2*26*256,0x45));
    else f.load2d(std::vector<BYTE>(40u*2*16*256,0x45));
}
static int multiEdges(unsigned ch) {
    FloppyDevice f;
    mount(f,ch);
    f.enableSeekTiming(true);
    const unsigned last=f.sectorsPerTrack(ch,0);
    const auto rev=f.revolutionNanoseconds(ch);
    // A multi-record command starting at the final ID never wraps
    // into the next head, cylinder or a fabricated sector ID.
    if(!f.beginTypeII(ch,0,0,0,last,0x90))return 1;
    f.advanceSeekNanoseconds(f.nextTypeIIEventNanoseconds());
    BYTE cmd=0;bool done=true;
    if(!f.takeTypeIIEvent(ch,cmd,done) || done || cmd!=0x90)return 2;
    if(!f.beginSector(ch,0,0,last,0,false))return 3;
    for(unsigned j=0;j<256;++j)
        if(f.transferRead()!=0x45)return 4;
    f.advanceSeekNanoseconds(f.nextTypeIIEventNanoseconds());
    if(!f.takeTypeIIEvent(ch,cmd,done) || !done ||
       f.typeIIResult(ch)!=0 || f.continueMultiTypeII(ch,true))return 5;
    if(f.registerValue(ch?0x32:0x1a)==BYTE(last+1))return 6;

    // Missing sector IDs must not produce phantom data, and time out
    // after five revolutions.
    if(!f.beginTypeII(ch,0,0,0,last+1,0x90))return 7;
    f.advanceSeekNanoseconds(4*rev);
    if(f.typeIIReady())return 8;
    f.advanceSeekNanoseconds(rev);
    if(!f.takeTypeIIEvent(ch,cmd,done) || !done ||
       f.typeIIResult(ch)!=0x10 || f.continueMultiTypeII(ch,true))
        return 9;

    // An interrupted WRITE must not commit a partially filled sector.
    if(!f.beginTypeII(ch,0,0,0,1,0xb0))return 10;
    f.advanceSeekNanoseconds(f.nextTypeIIEventNanoseconds());
    if(!f.takeTypeIIEvent(ch,cmd,done) || done)return 11;
    if(!f.beginSector(ch,0,0,1,0,true))return 12;
    f.startTypeIIBytePacing(ch);
    for(unsigned j=0;j<8;++j) {
        if(j)f.advanceSeekNanoseconds(ch?16000:32000);
        if(!f.typeIIByteReady(ch))return 13;
        f.transferWrite(0xa5);
        if(!f.acceptTypeIIByte(ch))return 14;
    }
    if(f.forceInterrupt(ch,0,0xd0) ||
       f.typeIIBusy(ch,0) || f.transferStatus()!=0)return 15;
    f.advanceSeekNanoseconds(2*rev);
    if(f.forceInterruptPending(ch) || f.typeIIReady())return 16;
    for(unsigned j=0;j<8;++j)
        if((ch?f.raw2hd():f.raw2d())[j]!=0x45)return 17;

    // A DRQ that is left pending while the medium rotates is LOST
    // DATA. In particular, no incomplete WRITE may hit the image.
    if(!f.beginTypeII(ch,0,0,0,1,0xb0))return 18;
    f.advanceSeekNanoseconds(f.nextTypeIIEventNanoseconds());
    if(!f.takeTypeIIEvent(ch,cmd,done) || done)return 19;
    if(!f.beginSector(ch,0,0,1,0,true))return 20;
    f.startTypeIIBytePacing(ch);
    f.advanceSeekNanoseconds(10000000);
    if(!f.takeTypeIIEvent(ch,cmd,done) || !done ||
       f.typeIIResult(ch)!=0x04 || f.continueMultiTypeII(ch,true))
        return 21;
    if((ch?f.raw2hd():f.raw2d())[0]!=0x45)return 22;
    return 0;
}
// Inject a host write error on the second sector after the first
// completed sector has already been persisted. The multi-record
// command must not advance to a third sector or alter the failed one.
static int midWriteError(unsigned ch) {
    FloppyDevice f;
    mount(f,ch);
    f.enableSeekTiming(true);
    f.setSectorWriter([](unsigned,unsigned,std::size_t offset,
                         const BYTE*,std::size_t){return offset==0;});
    if(!f.beginTypeII(ch,0,0,0,1,0xb0))return 1;
    BYTE cmd=0;bool done=false;
    for(unsigned sector=1;sector<=2;++sector) {
        f.advanceSeekNanoseconds(f.nextTypeIIEventNanoseconds());
        if(!f.takeTypeIIEvent(ch,cmd,done) || done || cmd!=0xb0)
            return 2;
        if(!f.beginSector(ch,0,0,sector,0,true))return 3;
        for(unsigned j=0;j<256;++j)f.transferWrite(0x73);
        if(f.transferStatus()!=(sector==1?0:0x10))return 4;
        f.advanceSeekNanoseconds(f.nextTypeIIEventNanoseconds());
        if(!f.takeTypeIIEvent(ch,cmd,done) || !done)return 5;
        if(f.continueMultiTypeII(ch,true)!=(sector==1))return 6;
    }
    const auto& raw=ch?f.raw2hd():f.raw2d();
    for(unsigned j=0;j<256;++j) {
        if(raw[j]!=0x73 || raw[j+256]!=0x45)return 7;
    }
    if(f.registerValue(ch?0x32:0x1a)!=2)return 8;
    return 0;
}
static void put16(std::vector<BYTE>& a,std::size_t pos,unsigned v) {
    a[pos]=BYTE(v);a[pos+1]=BYTE(v>>8);
}
static void put32(std::vector<BYTE>& a,std::size_t pos,unsigned v) {
    put16(a,pos,v);put16(a,pos+2,v>>16);
}
static std::vector<BYTE> native2hdD88() {
    std::vector<BYTE> img(0x2b0,0);
    img[0x1b]=0x20;
    for(unsigned track=0;track<154;++track) {
        put32(img,0x20+track*4,unsigned(img.size()));
        const unsigned length=track==0?128:256;
        for(unsigned id=1;id<=26;++id) {
            const auto pos=img.size();
            img.resize(pos+16+length,0);
            img[pos]=BYTE(track/2);img[pos+1]=BYTE(track&1);
            img[pos+2]=BYTE(id);img[pos+3]=BYTE(track==0?0:1);
            put16(img,pos+4,26);
            img[pos+6]=BYTE(track==0?0x40:0);
            put16(img,pos+14,length);
        }
    }
    put32(img,0x1c,unsigned(img.size()));
    return img;
}
static int addressEdges() {
    FloppyDevice f;
    f.loadD882hd(native2hdD88());
    if(!f.beginReadAddress(1,0,0,0))return 1;
    const BYTE id0[]={0,0,1,0,0xd2,0xc3};
    for(const auto v:id0)if(f.transferRead()!=v)return 2;
    if(f.transferStatus()!=0)return 3;
    if(!f.beginReadAddress(1,0,1,0))return 4;
    const BYTE id1[]={0,1,1,1,0xf5,0xd2};
    for(const auto v:id1)if(f.transferRead()!=v)return 5;

    // Rotation changes the next ID, independent of the stale R latch.
    f.enableSeekTiming(true);
    const auto rev=f.revolutionNanoseconds(1);
    f.advanceSeekNanoseconds(rev/3);
    if(!f.beginTypeII(1,0,0,0,0,0xc0))return 6;
    f.advanceSeekNanoseconds(f.nextTypeIIEventNanoseconds());
    BYTE command=0;bool done=true;
    if(!f.takeTypeIIEvent(1,command,done) || done || command!=0xc0)
        return 7;
    if(!f.beginReadAddress(1,0,0,0))return 8;
    const BYTE c=f.transferRead(),h=f.transferRead(),r=f.transferRead();
    if(c!=0 || h!=0 || r!=10 || f.registerValue(0x32)!=r)return 9;
    return 0;
}
static int cpuAddressLatch() {
    auto f=std::make_shared<FloppyDevice>();
    f->loadD882hd(native2hdD88());
    Main6809Board b;
    b.setFloppyDevice(f);
    b.reset();
    b.writeCpu(0xfd35,0);b.writeCpu(0xfd31,0);
    b.writeCpu(0xfd34,0);b.writeCpu(0xfd32,25);
    b.writeCpu(0xfd51,0); // PIO
    b.writeCpu(0xfd30,0xc0);
    if(b.readCpu(0xfd32)!=1 || b.readCpu(0xfd33)!=0)return 1;
    return 0;
}
// A short DMA block must not spill into the next sector. Fast mode
// retains the original emulator's partial READ behavior; in timed mode,
// the FDC keeps clocking the sector and reports the unserviced DRQs as
// LOST DATA. Neither mode may modify bytes outside the 128-byte block.
static int shortDma6809(unsigned ch,bool timed) {
    Machine m;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto f=m.systemBoard().floppyDevice();
    mount(*f,ch);
    m.reset();f->enableSeekTiming(timed);
    const WORD cmd=ch?0xfd30:0xfd18,ctl=ch?0xfd51:0xfd50;
    const WORD addr=ch?0xfd44:0xfd40,cnt=ch?0xfd46:0xfd42;
    cpu->writeCpu(ch?0xfd35:0xfd1d,0);
    cpu->writeCpu(ch?0xfd31:0xfd19,0);
    cpu->writeCpu(ch?0xfd34:0xfd1c,0);
    cpu->writeCpu(ch?0xfd32:0xfd1a,1);
    cpu->writeCpu(ch?0xfd95:0xfd94,0);
    cpu->writeCpu(addr,0x30);cpu->writeCpu(WORD(addr+1),0);
    cpu->writeCpu(cnt,0);cpu->writeCpu(WORD(cnt+1),128);
    for(unsigned j=0;j<256;++j)cpu->writePhysical(0x3000+j,0xaa);
    cpu->writeCpu(ctl,0x04);
    cpu->writeCpu(cmd,0x90);
    if(timed)m.advanceTimeNanoseconds(300000000);
    if((cpu->readCpu(cmd)&0x04)!=BYTE(timed?0x04:0))return 1;
    if(cpu->readCpu(cnt)!=0 || cpu->readCpu(WORD(cnt+1))!=0)
        return 2;
    for(unsigned j=0;j<256;++j)
        if(cpu->readPhysical(0x3000+j)!=(j<128?0x45:0xaa))
            return 3;
    if(cpu->readCpu(ch?0xfd32:0xfd1a)!=1)return 4;
    return 0;
}
static int shortDma8088(unsigned ch,bool timed) {
    Machine m;
    auto board=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto f=m.systemBoard().floppyDevice();
    mount(*f,ch);
    m.reset();f->enableSeekTiming(timed);
    auto& bus=cpu->bus();
    const WORD cmd=ch?0xfd30:0xfd18;
    bus.writeIo(ch?0xfd35:0xfd1d,0);
    bus.writeIo(ch?0xfd31:0xfd19,0);
    bus.writeIo(ch?0xfd34:0xfd1c,0);
    bus.writeIo(ch?0xfd32:0xfd1a,1);
    for(unsigned j=0;j<256;++j)bus.writeMemory(0x3000+j,0xaa);
    bus.writeIo(0x1c,0);
    bus.writeIo(WORD(0x10+ch*2),0x00);
    bus.writeIo(WORD(0x10+ch*2),0x30);
    bus.writeIo(WORD(0x11+ch*2),127); // 128 bytes, 8237 N-1
    bus.writeIo(WORD(0x11+ch*2),0);
    bus.writeIo(WORD(0x20+ch),0);
    bus.writeIo(0x1b,BYTE(0x44+ch));
    bus.writeIo(0x1a,BYTE(ch));
    bus.writeIo(cmd,0x90);
    if(timed)m.advanceTimeNanoseconds(300000000);
    if((bus.readIo(cmd)&0x04)!=BYTE(timed?0x04:0))return 1;
    if(bus.dmaChannel(ch).count!=0xffff)return 2;
    for(unsigned j=0;j<256;++j)
        if(bus.readRam(0x3000+j)!=(j<128?0x45:0xaa))
            return 3;
    if(bus.readIo(ch?0xfd32:0xfd1a)!=1)return 4;
    return 0;
}
// A DMA block longer than the remaining side-track may transfer only
// the last physical sector. It must never wrap to the next side or
// overwrite the second half of the DMA destination.
static int endTrack6809(unsigned ch,bool timed) {
    Machine m;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto f=m.systemBoard().floppyDevice();
    mount(*f,ch);
    m.reset();f->enableSeekTiming(timed);
    const WORD cmd=ch?0xfd30:0xfd18,addr=ch?0xfd44:0xfd40;
    const WORD count=ch?0xfd46:0xfd42;
    const unsigned last=f->sectorsPerTrack(ch,0);
    cpu->writeCpu(ch?0xfd35:0xfd1d,0);
    cpu->writeCpu(ch?0xfd31:0xfd19,0);
    cpu->writeCpu(ch?0xfd34:0xfd1c,0);
    cpu->writeCpu(ch?0xfd32:0xfd1a,BYTE(last));
    cpu->writeCpu(ch?0xfd95:0xfd94,0);
    cpu->writeCpu(addr,0x50);cpu->writeCpu(WORD(addr+1),0);
    cpu->writeCpu(count,2);cpu->writeCpu(WORD(count+1),0);
    for(unsigned j=0;j<512;++j)cpu->writePhysical(0x5000+j,0xaa);
    cpu->writeCpu(ch?0xfd51:0xfd50,0x04);
    cpu->writeCpu(cmd,0x90);
    if(timed)m.advanceTimeNanoseconds(500000000);
    if(cpu->readCpu(count)!=1 || cpu->readCpu(WORD(count+1))!=0)
        return 1;
    if(cpu->readCpu(ch?0xfd32:0xfd1a)!=last)return 2;
    for(unsigned j=0;j<512;++j)
        if(cpu->readPhysical(0x5000+j)!=(j<256?0x45:0xaa))
            return 3;
    return 0;
}
static int endTrack8088(unsigned ch,bool timed) {
    Machine m;
    auto board=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto f=m.systemBoard().floppyDevice();
    mount(*f,ch);
    m.reset();f->enableSeekTiming(timed);
    auto& bus=cpu->bus();
    const unsigned last=f->sectorsPerTrack(ch,0);
    bus.writeIo(ch?0xfd35:0xfd1d,0);
    bus.writeIo(ch?0xfd31:0xfd19,0);
    bus.writeIo(ch?0xfd34:0xfd1c,0);
    bus.writeIo(ch?0xfd32:0xfd1a,BYTE(last));
    for(unsigned j=0;j<512;++j)bus.writeMemory(0x5000+j,0xaa);
    bus.writeIo(0x1c,0);
    bus.writeIo(WORD(0x10+ch*2),0x00);
    bus.writeIo(WORD(0x10+ch*2),0x50);
    bus.writeIo(WORD(0x11+ch*2),0xff);
    bus.writeIo(WORD(0x11+ch*2),0x01);
    bus.writeIo(WORD(0x20+ch),0);
    bus.writeIo(0x1b,BYTE(0x44+ch));
    bus.writeIo(0x1a,BYTE(ch));
    bus.writeIo(ch?0xfd30:0xfd18,0x90);
    if(timed)m.advanceTimeNanoseconds(500000000);
    if(bus.dmaChannel(ch).count!=0x00ff)return 1;
    if(bus.readIo(ch?0xfd32:0xfd1a)!=last)return 2;
    for(unsigned j=0;j<512;++j)
        if(bus.readRam(0x5000+j)!=(j<256?0x45:0xaa))
            return 3;
    return 0;
}
int main() {
    for(unsigned ch=0;ch<2;++ch) {
        if(int r=multiEdges(ch))return 10+ch*30+r;
        if(int r=midWriteError(ch))return 50+ch*10+r;
        for(unsigned timed=0;timed<2;++timed) {
            if(int r=shortDma6809(ch,timed!=0))
                return 100+ch*20+timed*10+r;
            if(int r=shortDma8088(ch,timed!=0))
                return 150+ch*20+timed*10+r;
            if(int r=endTrack6809(ch,timed!=0))
                return 200+ch*20+timed*10+r;
            if(int r=endTrack8088(ch,timed!=0))
                return 240+ch*6+timed*3+r;
        }
    }
    if(int r=addressEdges())return 80+r;
    if(int r=cpuAddressLatch())return 90+r;
    return 0;
}
