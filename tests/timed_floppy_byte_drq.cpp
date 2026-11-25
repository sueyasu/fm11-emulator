#include "FloppyDevice.h"
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>
#include <vector>

// Stage 2-B2: DRQ is a byte-time pulse, rather than a whole-sector
// permission. A stalled interface must raise WD179x LOST DATA (bit 2).
int main() {
    FloppyDevice f;
    f.load2d(std::vector<BYTE>(40u*2*16*256,0x5a));
    f.enableSeekTiming(true);
    if(!f.beginTypeII(0,0,0,0,1,0x80))return 1;
    f.advanceSeekNanoseconds(3125000); // C0/H0/S1 ID
    BYTE cmd=0;bool done=false;
    if(!f.takeTypeIIEvent(0,cmd,done) || done)return 2;
    if(!f.beginSector(0,0,0,1,0,false))return 3;
    f.startTypeIIBytePacing(0);
    if(!f.typeIIByteReady(0) || f.transferRead()!=0x5a ||
       !f.acceptTypeIIByte(0))return 4;
    f.advanceSeekNanoseconds(31999);
    if(f.typeIIByteReady(0))return 5;
    f.advanceSeekNanoseconds(1);
    if(!f.typeIIByteReady(0) || f.typeIILostData(0))return 6;
    f.advanceSeekNanoseconds(32000); // missed previous DRQ
    if(!f.typeIILostData(0))return 7;
    f.advanceSeekNanoseconds(10000000);
    if(!f.takeTypeIIEvent(0,cmd,done) || !done ||
       f.typeIIResult(0)!=0x04)return 8;

    // One large CPU instruction can span many byte times. The machine's
    // FDC scheduler must still service hardware DMA at each DRQ deadline.
    Machine machine;
    auto card=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=card.get();
    machine.addBoard(std::move(card));
    std::vector<BYTE> image(40u*2*16*256,0);
    for(unsigned i=0;i<256;++i)image[i]=BYTE(i^0x5a);
    machine.systemBoard().floppyDevice()->load2d(image);
    machine.reset();
    machine.systemBoard().floppyDevice()->enableSeekTiming(true);
    auto& bus=cpu->bus();
    bus.writeIo(0xfd19,0);bus.writeIo(0xfd1a,1);
    bus.writeIo(0xfd1c,0);bus.writeIo(0xfd1d,0);
    auto setup=[&](WORD dst,bool masked) {
        bus.writeIo(0x1c,0);
        bus.writeIo(0x10,BYTE(dst));bus.writeIo(0x10,BYTE(dst>>8));
        bus.writeIo(0x11,0xff);bus.writeIo(0x11,0);
        bus.writeIo(0x1b,0x44);
        bus.writeIo(0x1a,masked?4:0);
    };
    setup(0x2000,false);
    bus.writeIo(0xfd18,0x80);
    machine.advanceTimeNanoseconds(200000000);
    if(bus.readIo(0xfd18)!=0 || (bus.readIo(0x18)&1)==0)return 9;
    for(unsigned i=0;i<256;++i)
        if(bus.readRam(0x2000+i)!=BYTE(i^0x5a))return 10;

    // An 8237 channel left masked misses each DRQ and reports LOST DATA.
    setup(0x2100,true);
    bus.writeIo(0xfd18,0x80);
    machine.advanceTimeNanoseconds(200000000);
    if((bus.readIo(0xfd18)&0x04)==0 ||
       bus.readRam(0x2100)!=0 || bus.dmaChannel(0).count!=0x00ff)
        return 11;

    // The 6809 HD6844 is independently coupled to the same byte clock.
    Machine machine09;
    auto card09=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* c09=card09.get();
    machine09.addBoard(std::move(card09));
    std::vector<BYTE> disk09(40u*2*16*256,0);
    disk09[0]=0x7e;
    machine09.systemBoard().floppyDevice()->load2d(disk09);
    machine09.reset();
    machine09.systemBoard().floppyDevice()->enableSeekTiming(true);
    c09->writeCpu(0xfd19,0);
    c09->writeCpu(0xfd1a,1);
    c09->writeCpu(0xfd1c,0);
    c09->writeCpu(0xfd1d,0);
    c09->writeCpu(0xfd94,0);
    c09->writeCpu(0xfd40,0x20);
    c09->writeCpu(0xfd41,0);
    c09->writeCpu(0xfd42,1);
    c09->writeCpu(0xfd43,0);
    c09->writeCpu(0xfd50,0x04); // DMA on, FDC -> RAM
    c09->writeCpu(0xfd18,0x80);
    machine09.advanceTimeNanoseconds(200000000);
    if(c09->readCpu(0xfd18)!=0 ||
       (*c09->physicalMemory())[0x2000]!=0x7e ||
       c09->readCpu(0xfd42)!=0 || c09->readCpu(0xfd43)!=0)
        return 12;
    // The native 2HD ROM boot track is FM (26 x 128), not the
    // normal 500-kbps MFM data track. PIO gets 32 us per byte.
    std::vector<BYTE> hdD88(0x2b0,0);
    hdD88[0x1b]=0x20;
    auto put32=[&](std::size_t off,unsigned v) {
        for(unsigned i=0;i<4;++i)hdD88[off+i]=BYTE(v>>(i*8));
    };
    for(unsigned tr=0;tr<154;++tr) {
        put32(0x20+4*tr,unsigned(hdD88.size()));
        const unsigned len=tr==0?128:256;
        for(unsigned id=1;id<=26;++id) {
            const auto start=hdD88.size();
            hdD88.resize(start+16+len,0);
            hdD88[start]=BYTE(tr/2);
            hdD88[start+1]=BYTE(tr&1);
            hdD88[start+2]=BYTE(id);
            hdD88[start+3]=BYTE(tr==0?0:1);
            hdD88[start+4]=26;
            hdD88[start+6]=BYTE(tr==0?0x40:0);
            hdD88[start+14]=BYTE(len);
            hdD88[start+15]=BYTE(len>>8);
            for(unsigned i=0;i<len;++i)
                hdD88[start+16+i]=BYTE(id+i);
        }
    }
    put32(0x1c,unsigned(hdD88.size()));
    FloppyDevice bootHd;
    bootHd.loadD882hd(hdD88);
    bootHd.enableSeekTiming(true);
    if(!bootHd.beginTypeII(1,0,0,0,1,0x80))return 13;
    bootHd.advanceSeekNanoseconds(FloppyDevice::revolutionNanoseconds(1));
    if(!bootHd.takeTypeIIEvent(1,cmd,done) || done ||
       !bootHd.beginSector(1,0,0,1,0,false))return 14;
    bootHd.startTypeIIBytePacing(1);
    if(!bootHd.typeIIByteReady(1))return 15;
    if(bootHd.transferRead()!=1 || !bootHd.acceptTypeIIByte(1))return 16;
    bootHd.advanceSeekNanoseconds(16000);
    // DRQ cannot be asserted after 16 us on the FM boot track.
    if(bootHd.typeIIByteReady(1) || bootHd.typeIILostData(1))return 17;
    bootHd.advanceSeekNanoseconds(16000);
    if(!bootHd.typeIIByteReady(1) || bootHd.typeIILostData(1))return 18;
    if(bootHd.transferRead()!=2 || !bootHd.acceptTypeIIByte(1))return 19;
    // All 128 FM-sector bytes, at one-byte/32us, must complete cleanly.
    for(unsigned i=2;i<128;++i) {
        bootHd.advanceSeekNanoseconds(32000);
        if(!bootHd.typeIIByteReady(1) || bootHd.transferRead()!=BYTE(1+i) ||
           !bootHd.acceptTypeIIByte(1))return 20;
    }
    bootHd.advanceSeekNanoseconds(32000);
    if(!bootHd.takeTypeIIEvent(1,cmd,done) || !done ||
       bootHd.typeIIResult(1)!=0)return 21;

    // The adjacent C0/H1 MFM track retains 16 us per byte.
    bootHd.advanceSeekNanoseconds(100000000);
    if(!bootHd.beginTypeII(1,0,0,1,1,0x80))return 22;
    bootHd.advanceSeekNanoseconds(FloppyDevice::revolutionNanoseconds(1));
    if(!bootHd.takeTypeIIEvent(1,cmd,done) || done ||
       !bootHd.beginSector(1,0,1,1,0,false))return 23;
    bootHd.startTypeIIBytePacing(1);
    if(!bootHd.acceptTypeIIByte(1))return 24;
    bootHd.advanceSeekNanoseconds(16000);
    if(!bootHd.typeIIByteReady(1))return 25;
    return 0;
}
