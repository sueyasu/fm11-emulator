#include "FloppyDevice.h"
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>
#include <vector>

// Exercise real DMA register paths (not just FDC storage helpers) with
// two consecutive sectors and one FDC command, in both timing modes.
static void disk(FloppyDevice& f,unsigned ch) {
    const unsigned sectors=ch?26:16,tracks=ch?77:40;
    std::vector<BYTE> raw(tracks*2*sectors*256,0xe5);
    for(unsigned j=0;j<256;++j) {
        raw[j]=0x31;
        raw[256+j]=0x62;
    }
    if(ch)f.load2hd(raw);
    else f.load2d(raw);
}
static int check6809(unsigned ch,bool timed,bool writing) {
    Machine m;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto f=m.systemBoard().floppyDevice();
    disk(*f,ch);
    m.reset();
    f->enableSeekTiming(timed);
    const WORD cmd=ch?0xfd30:0xfd18;
    const WORD irq=ch?0xfd37:0xfd1f;
    const WORD bank=ch?0xfd95:0xfd94;
    const WORD ctl=ch?0xfd51:0xfd50;
    const WORD addr=ch?0xfd44:0xfd40;
    const WORD cnt=ch?0xfd46:0xfd42;
    cpu->writeCpu(ch?0xfd35:0xfd1d,0);
    cpu->writeCpu(ch?0xfd31:0xfd19,0);
    cpu->writeCpu(ch?0xfd34:0xfd1c,0);
    cpu->writeCpu(ch?0xfd32:0xfd1a,1);
    cpu->writeCpu(bank,0);
    cpu->writeCpu(addr,0x40);
    cpu->writeCpu(WORD(addr+1),0);
    cpu->writeCpu(cnt,0x02);
    cpu->writeCpu(WORD(cnt+1),0);
    for(unsigned j=0;j<512;++j)
        cpu->writePhysical(0x4000+j,writing?BYTE(j<256?0x73:0x84):0);
    cpu->writeCpu(ctl,writing?0x05:0x04);
    cpu->writeCpu(cmd,writing?0xb0:0x90);
    if(timed)m.advanceTimeNanoseconds(500000000);
    if((cpu->readCpu(irq)&0x40)==0)return 1;
    if(cpu->readCpu(cnt)!=0 || cpu->readCpu(WORD(cnt+1))!=0)return 2;
    if(cpu->readCpu(ch?0xfd32:0xfd1a)!=2)return 5;
    for(unsigned j=0;j<512;++j) {
        const BYTE expect=writing?BYTE(j<256?0x73:0x84):
                                  BYTE(j<256?0x31:0x62);
        if(writing) {
            const auto& raw=ch?f->raw2hd():f->raw2d();
            if(raw[j]!=expect)return 3;
        } else if(cpu->readPhysical(0x4000+j)!=expect)return 4;
    }
    return 0;
}
// A fast PIO multi-record command must cross sector boundaries without
// another command write, through the same data port as single-sector I/O.
static int check6809FastPio(unsigned ch,bool writing) {
    Machine m;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto f=m.systemBoard().floppyDevice();
    disk(*f,ch);
    m.reset();
    f->enableSeekTiming(false);
    const WORD cmd=ch?0xfd30:0xfd18;
    const WORD data=ch?0xfd33:0xfd1b;
    const WORD sector=ch?0xfd32:0xfd1a;
    cpu->writeCpu(ch?0xfd35:0xfd1d,0);
    cpu->writeCpu(ch?0xfd31:0xfd19,0);
    cpu->writeCpu(ch?0xfd34:0xfd1c,0);
    cpu->writeCpu(sector,1);
    cpu->writeCpu(ch?0xfd51:0xfd50,writing?1:0); // no DMA
    cpu->writeCpu(cmd,writing?0xb0:0x90);
    const unsigned count=f->sectorsPerTrack(ch,0);
    for(unsigned sectorNum=0;sectorNum<count;++sectorNum)
        for(unsigned j=0;j<256;++j) {
            const BYTE expected=BYTE(sectorNum==0?0x31:
                                     sectorNum==1?0x62:0xe5);
            if(writing)
                cpu->writeCpu(data,BYTE(sectorNum==0?0x73:
                                        sectorNum==1?0x84:0x95));
            else if(cpu->readCpu(data)!=expected)return 1;
        }
    if(cpu->readCpu(sector)!=count)return 2;
    if(writing) {
        const auto& raw=ch?f->raw2hd():f->raw2d();
        for(unsigned n=0;n<count;++n)
            if(raw[n*256]!=BYTE(n==0?0x73:n==1?0x84:0x95))return 3;
    }
    return 0;
}
static int check8088(unsigned ch,bool timed,bool writing) {
    Machine m;
    auto board=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=board.get();
    m.addBoard(std::move(board));
    auto f=m.systemBoard().floppyDevice();
    disk(*f,ch);
    m.reset();
    f->enableSeekTiming(timed);
    auto& bus=cpu->bus();
    const WORD cmd=ch?0xfd30:0xfd18;
    const WORD irq=ch?0xfd37:0xfd1f;
    constexpr WORD addr=0x4000;
    bus.writeIo(ch?0xfd35:0xfd1d,0);
    bus.writeIo(ch?0xfd31:0xfd19,0);
    bus.writeIo(ch?0xfd34:0xfd1c,0);
    bus.writeIo(ch?0xfd32:0xfd1a,1);
    for(unsigned j=0;j<512;++j)
        bus.writeMemory(addr+j,writing?BYTE(j<256?0x73:0x84):0);
    bus.writeIo(0x1c,0); // clear 8237 address/count flip-flop
    bus.writeIo(WORD(0x10+ch*2),BYTE(addr));
    bus.writeIo(WORD(0x10+ch*2),BYTE(addr>>8));
    bus.writeIo(WORD(0x11+ch*2),0xff); // 512 bytes, N-1
    bus.writeIo(WORD(0x11+ch*2),0x01);
    bus.writeIo(WORD(0x20+ch),0);
    bus.writeIo(0x1b,BYTE((writing?0x48:0x44)+ch));
    bus.writeIo(0x1a,BYTE(ch));
    bus.writeIo(cmd,writing?0xb0:0x90);
    if(timed)m.advanceTimeNanoseconds(500000000);
    if((bus.readIo(irq)&0x40)==0)return 1;
    if(bus.dmaChannel(ch).count!=0xffff)return 2;
    for(unsigned j=0;j<512;++j) {
        const BYTE expect=writing?BYTE(j<256?0x73:0x84):
                                  BYTE(j<256?0x31:0x62);
        if(writing) {
            const auto& raw=ch?f->raw2hd():f->raw2d();
            if(raw[j]!=expect)return 3;
        } else if(bus.readRam(addr+j)!=expect)return 4;
    }
    return 0;
}
int main() {
    for(unsigned ch=0;ch<2;++ch)
        for(unsigned timed=0;timed<2;++timed)
            for(unsigned writing=0;writing<2;++writing) {
                if(int r=check6809(ch,timed!=0,writing!=0))
                    return 10+ch*40+timed*20+writing*10+r;
                if(int r=check8088(ch,timed!=0,writing!=0))
                    return 100+ch*40+timed*20+writing*10+r;
                if(!timed)
                    if(int r=check6809FastPio(ch,writing!=0))
                        return 200+ch*20+writing*10+r;
            }
    return 0;
}
