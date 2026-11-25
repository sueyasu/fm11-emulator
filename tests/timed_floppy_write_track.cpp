#include "FloppyDevice.h"
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <cstdint>
#include <memory>
#include <vector>

static std::vector<BYTE> formatStream(unsigned track,unsigned side,
                                      unsigned sectors,unsigned length) {
    std::vector<BYTE> bytes;
    for(unsigned id=1;id<=sectors;++id) {
        bytes.insert(bytes.end(),5,0);
        bytes.insert(bytes.end(),{0xfe,BYTE(track),BYTE(side),BYTE(id),1,0xf7});
        bytes.insert(bytes.end(),5,0);
        bytes.push_back(0xfb);
        bytes.insert(bytes.end(),256,BYTE(id));
        bytes.push_back(0xf7);
    }
    bytes.resize(length,0x4e);
    return bytes;
}

static int check6809(unsigned ch) {
    Machine machine;
    auto card=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=card.get();
    machine.addBoard(std::move(card));
    auto fdc=machine.systemBoard().floppyDevice();
    if(ch)fdc->load2hd(std::vector<BYTE>(77u*2*26*256,0xe5));
    else fdc->load2d(std::vector<BYTE>(40u*2*16*256,0xe5));
    machine.reset();
    fdc->enableSeekTiming(true);
    const unsigned length=ch?10900:6400;
    const WORD addr=0x2000;
    const unsigned side=ch?1:0;
    const auto bytes=formatStream(0,side,ch?26:16,length);
    for(unsigned i=0;i<length;++i)cpu->writePhysical(addr+i,bytes[i]);
    cpu->writeCpu(WORD(ch?0xfd44:0xfd40),BYTE(addr>>8));
    cpu->writeCpu(WORD(ch?0xfd45:0xfd41),BYTE(addr));
    cpu->writeCpu(WORD(ch?0xfd46:0xfd42),BYTE(length>>8));
    cpu->writeCpu(WORD(ch?0xfd47:0xfd43),BYTE(length));
    cpu->writeCpu(WORD(ch?0xfd95:0xfd94),0);
    cpu->writeCpu(WORD(ch?0xfd51:0xfd50),0x05);
    cpu->writeCpu(WORD(ch?0xfd31:0xfd19),0);
    cpu->writeCpu(WORD(ch?0xfd34:0xfd1c),BYTE(side));
    cpu->writeCpu(WORD(ch?0xfd35:0xfd1d),0);
    cpu->writeCpu(WORD(ch?0xfd30:0xfd18),0xf0);
    if((cpu->readCpu(WORD(ch?0xfd30:0xfd18))&1)==0)return 1;
    machine.advanceTimeNanoseconds(30000000);
    const unsigned rem=(unsigned(cpu->readCpu(WORD(ch?0xfd46:0xfd42))<<8))|
                         cpu->readCpu(WORD(ch?0xfd47:0xfd43));
    if(rem==0 || rem>=length ||
       (cpu->readCpu(WORD(ch?0xfd51:0xfd50))&0xc0) ||
       (cpu->readCpu(WORD(ch?0xfd30:0xfd18))&1)==0 ||
       (ch?fdc->raw2hd()[side*26*256]:fdc->raw2d()[0])!=0xe5)
        return 2;
    machine.advanceTimeNanoseconds(200000000);
    if(cpu->readCpu(WORD(ch?0xfd30:0xfd18))!=0 ||
       (cpu->readCpu(WORD(ch?0xfd51:0xfd50))&0x80)==0 ||
       cpu->readCpu(WORD(ch?0xfd46:0xfd42))!=0 ||
       cpu->readCpu(WORD(ch?0xfd47:0xfd43))!=0 ||
       (ch?fdc->raw2hd()[side*26*256]:fdc->raw2d()[0])!=1)
        return 3;
    return 0;
}

static int check8088(unsigned ch,bool masked) {
    Machine machine;
    auto card=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=card.get();
    machine.addBoard(std::move(card));
    auto fdc=machine.systemBoard().floppyDevice();
    if(ch)fdc->load2hd(std::vector<BYTE>(77u*2*26*256,0xe5));
    else fdc->load2d(std::vector<BYTE>(40u*2*16*256,0xe5));
    machine.reset();
    fdc->enableSeekTiming(true);
    auto& bus=cpu->bus();
    const unsigned length=ch?10900:6400;
    const unsigned side=ch?1:0;
    const auto bytes=formatStream(0,side,ch?26:16,length);
    const WORD addr=0x2000;
    for(unsigned i=0;i<length;++i)bus.writeMemory(addr+i,bytes[i]);
    bus.writeIo(0x1c,0);
    bus.writeIo(WORD(0x10+ch*2),BYTE(addr));
    bus.writeIo(WORD(0x10+ch*2),BYTE(addr>>8));
    bus.writeIo(WORD(0x11+ch*2),BYTE(length-1));
    bus.writeIo(WORD(0x11+ch*2),BYTE((length-1)>>8));
    bus.writeIo(WORD(0x20+ch),0);
    bus.writeIo(0x1b,BYTE(0x48+ch));
    bus.writeIo(0x1a,BYTE(masked?4+ch:ch));
    bus.writeIo(WORD(ch?0xfd31:0xfd19),0);
    bus.writeIo(WORD(ch?0xfd34:0xfd1c),BYTE(side));
    bus.writeIo(WORD(ch?0xfd35:0xfd1d),0);
    bus.writeIo(WORD(ch?0xfd30:0xfd18),0xf0);
    if((bus.readIo(WORD(ch?0xfd30:0xfd18))&1)==0)return 4;
    machine.advanceTimeNanoseconds(30000000);
    if((bus.readIo(WORD(ch?0xfd30:0xfd18))&1)==0 ||
       (ch?fdc->raw2hd()[side*26*256]:fdc->raw2d()[0])!=0xe5)
        return 5;
    if(!masked && (bus.dmaChannel(ch).count==WORD(length-1) ||
                   bus.dmaChannel(ch).count==0xffff))
        return 6;
    machine.advanceTimeNanoseconds(200000000);
    if((bus.readIo(WORD(ch?0xfd30:0xfd18))&1)!=0 ||
       (bus.readIo(WORD(ch?0xfd1f:0xfd1f))&0x40)==0)
        return 7;
    if(masked) {
        if((bus.readIo(WORD(ch?0xfd30:0xfd18))&0x04)==0 ||
           bus.dmaChannel(ch).count!=WORD(length-1) ||
           (ch?fdc->raw2hd()[side*26*256]:fdc->raw2d()[0])!=0xe5)
            return 8;
    } else {
        if(bus.dmaChannel(ch).count!=0xffff ||
           (ch?fdc->raw2hd()[side*26*256]:fdc->raw2d()[0])!=1)
            return 9;
    }
    return 0;
}
int main() {
    for(unsigned ch=0;ch<2;++ch) {
        if(int r=check6809(ch))return 10+ch*10+r;
        if(int r=check8088(ch,false))return 30+ch*10+r;
        if(int r=check8088(ch,true))return 50+ch*10+r;
    }
    return 0;
}
