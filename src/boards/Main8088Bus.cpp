#include "Main8088Bus.h"
#include <utility>
#include <stdexcept>
#include <algorithm>

Main8088Bus::Main8088Bus():memory_(kMemorySize,0) {}

void Main8088Bus::reset()
{
    cardIoRam_.fill(0);
    picMask_=0;picRequest_=0;picInService_=0;
    picVectorBase_=8;picReadIsr_=0;picInitStep_=0;
    picNeedsIcw4_=false;picSingle_=false;picAutoEoi_=false;
    resetDma();
    floppyRegisters_.fill(0);
    floppyStatus_=0;
    floppyPosition_=256;
    floppyWriting_=false;
}

void Main8088Bus::loadFloppy2d(const std::vector<BYTE>& image)
{
    if(image.size()!=40u*2*16*256)
        throw std::invalid_argument("FM-11 2D raw image must be 327680 bytes");
    floppy2d_=image;
    floppyDirty_=false;
    floppyPosition_=256;
    floppyStatus_=0;
    floppyWriting_=false;
    // FM-11 FDD uses the main CPU DMAC. CH0 is the current 2D adapter
    // wiring assumption; keep the 8237 byte transfers on its usual path.
    setDmaDevice(0,[this](){return floppyReadData();},
                   [this](BYTE value){floppyWriteData(value);});
}

bool Main8088Bus::floppySectorOffset(std::size_t& offset)
{
    const unsigned track=floppyRegisters_[0x19];
    const unsigned sector=floppyRegisters_[0x1a];
    const unsigned side=floppyRegisters_[0x1c]&1;
    const unsigned drive=floppyRegisters_[0x1d]&3;
    if(drive!=0 || track>=40 || sector<1 || sector>16 || floppy2d_.empty()) {
        floppyStatus_=0x10;
        return false;
    }
    offset=((track*2+side)*16+(sector-1))*256;
    return true;
}

BYTE Main8088Bus::floppyReadData()
{
    if(floppyPosition_>=256)return 0xff;
    const BYTE value=floppyBuffer_[floppyPosition_++];
    if(floppyPosition_==256)floppyStatus_=0;
    return value;
}

void Main8088Bus::floppyWriteData(BYTE value)
{
    if(!floppyWriting_ || floppyPosition_>=256)return;
    floppyBuffer_[floppyPosition_++]=value;
    if(floppyPosition_==256) {
        std::copy(floppyBuffer_.begin(),floppyBuffer_.end(),
                  floppy2d_.begin()+floppySectorStart_);
        floppyDirty_=true;
        floppyWriting_=false;
        floppyStatus_=0;
    }
}

void Main8088Bus::floppyCommand(BYTE command)
{
    floppyStatus_=0;
    floppyPosition_=256;
    floppyWriting_=false;
    const bool read=(command&0xf0)==0x80;
    const bool write=(command&0xf0)==0xa0;
    if(!read && !write)return;
    std::size_t offset=0;
    if(!floppySectorOffset(offset))return;
    floppySectorStart_=offset;
    floppyPosition_=0;
    floppyWriting_=write;
    if(read)std::copy_n(floppy2d_.begin()+offset,256,floppyBuffer_.begin());
    floppyStatus_=0x02;
    // Complete one sector through the existing 8237 request/arbitration path,
    // never by writing directly to the 8088 address space.
    setDmaRequest(0,true);
    unsigned transferred=0;
    for(;transferred<256;++transferred) {
        if(!serviceDma())break;
        if(floppyPosition_==256)break;
    }
    setDmaRequest(0,false);
    // A short or unavailable DMA transfer remains an error, not success.
    if(floppyPosition_!=256 && transferred<256)floppyStatus_=0x10;
}

void Main8088Bus::loadBootRom(const Rom& rom)
{
    bootRom_=rom;
}

BYTE Main8088Bus::readMemory(std::uint32_t address) const
{
    address &= 0xfffff;
    return address >= kRomBase ? bootRom_[address-kRomBase] : memory_[address];
}

void Main8088Bus::writeMemory(std::uint32_t address, BYTE value)
{
    memory_[address&0xfffff]=value;
}

BYTE Main8088Bus::readRam(std::uint32_t address) const
{
    return memory_[address&0xfffff];
}

void Main8088Bus::setSystemIo(IoReader reader,IoWriter writer)
{
    systemReader_=std::move(reader);
    systemWriter_=std::move(writer);
}

BYTE Main8088Bus::readIo(WORD port) const
{
    if(port>=0x10 && port<=0x23)return readDma(port);
    if(port==0x00)return picReadIsr_?picInService_:picRequest_;
    if(port==0x01)return picMask_;
    if(port>=0xfc00 && port<=0xfcff) return cardIoRam_[port&0xff];
    if(port>=0xfd00 && port<=0xfdff && systemReader_) return systemReader_(port);
    if(port>=0xfd18 && port<=0xfd1d && !floppy2d_.empty()) {
        if(port==0xfd18)return floppyStatus_;
        if(port==0xfd1b)return const_cast<Main8088Bus*>(this)->floppyReadData();
        return floppyRegisters_[port&0xff];
    }
    // 8259 PIC and 8237 DMA belong to the future Sim86 adapter.
    return 0xff;
}

void Main8088Bus::writeIo(WORD port,BYTE value)
{
    if(port>=0x10 && port<=0x23){writeDma(port,value);return;}
    if(port==0x00) {
        if(value&0x10) {
            picInitStep_=1;
            picNeedsIcw4_=(value&1)!=0;
            picAutoEoi_=false;
            picSingle_=(value&2)!=0;
            picMask_=0;picRequest_=0;picInService_=0;picReadIsr_=0;
        } else if((value&0x18)==0x08) {
            if(value&2)picReadIsr_=BYTE(value&1);
        } else if((value&0xe0)==0x20) { // Non-specific EOI
            for(unsigned i=0;i<8;++i) if(picInService_&(1u<<i)) {
                picInService_=BYTE(picInService_&~(1u<<i));break;
            }
        } else if((value&0xe0)==0x60) { // Specific EOI
            picInService_=BYTE(picInService_&~(1u<<(value&7)));
        }
        return;
    }
    if(port==0x01) {
        if(picInitStep_==1) {
            picVectorBase_=BYTE(value&0xf8);
            picInitStep_=picSingle_?(picNeedsIcw4_?3:0):2;
        } else if(picInitStep_==2) {
            picInitStep_=picNeedsIcw4_?3:0;
        } else if(picInitStep_==3) {
            picAutoEoi_=(value&0x02)!=0; // ICW4 automatic EOI
            picInitStep_=0;
        } else picMask_=value;
        return;
    }
    if(port>=0xfc00 && port<=0xfcff) cardIoRam_[port&0xff]=value;
    else if(port>=0xfd00 && port<=0xfdff && systemWriter_) systemWriter_(port,value);
    else if(port>=0xfd18 && port<=0xfd1d && !floppy2d_.empty()) {
        if(port==0xfd18)floppyCommand(value);
        else if(port==0xfd1b)floppyWriteData(value);
        else floppyRegisters_[port&0xff]=value;
    }
}

void Main8088Bus::requestIrq(unsigned line)
{
    if(line<8)picRequest_=BYTE(picRequest_|(1u<<line));
}
bool Main8088Bus::interruptPending() const
{
    // Fixed priority: IRQ0 is highest. An IRQ already in service blocks
    // its own priority and all lower-priority requests until EOI.
    BYTE eligible=BYTE(picRequest_&BYTE(~picMask_));
    for(unsigned line=0;line<8;++line)if(picInService_&(1u<<line)) {
        eligible=BYTE(eligible&((1u<<line)-1u));
        break;
    }
    return eligible!=0;
}
BYTE Main8088Bus::acknowledgeInterrupt()
{
    BYTE eligible=BYTE(picRequest_&BYTE(~picMask_));
    for(unsigned line=0;line<8;++line)if(picInService_&(1u<<line)) {
        eligible=BYTE(eligible&((1u<<line)-1u));
        break;
    }
    for(unsigned line=0;line<8;++line)if(eligible&(1u<<line)) {
        picRequest_=BYTE(picRequest_&~(1u<<line));
        if(!picAutoEoi_)picInService_=BYTE(picInService_|(1u<<line));
        return BYTE(picVectorBase_+line);
    }
    return BYTE(picVectorBase_+7); // Spurious IRQ7 when none is pending
}

void Main8088Bus::resetDma()
{
    dmaChannels_.fill(DmaChannel{});
    dmaCommand_=dmaMask_=dmaStatus_=dmaFlip_=dmaRequest_=0;
    dmaPriorityBase_=dmaTemporary_=0;
    dmaExternalRequest_=dmaExternalDriven_=dmaBlockActive_=0;
}
BYTE Main8088Bus::readDma(WORD port) const
{
    // The 8237 first/last byte latch is shared by all channels.
    // The read path is logically mutable; update only the flip-flop.
    auto* self=const_cast<Main8088Bus*>(this);
    if(port>=0x10 && port<=0x17) {
        const unsigned channel=(port-0x10)/2;
        const WORD word=(port&1)?dmaChannels_[channel].count:dmaChannels_[channel].address;
        const BYTE value=self->dmaFlip_?BYTE(word>>8):BYTE(word);
        self->dmaFlip_^=1;
        return value;
    }
    if(port==0x18) {
        const BYTE status=dmaStatus_;
        self->dmaStatus_&=0xf0;
        return status;
    }
    if(port==0x1d)return dmaTemporary_; // last memory-to-memory byte
    if(port>=0x20 && port<=0x23)return dmaChannels_[port-0x20].bank;
    return 0xff;
}
void Main8088Bus::writeDma(WORD port,BYTE value)
{
    if(port>=0x10 && port<=0x17) {
        DmaChannel& ch=dmaChannels_[(port-0x10)/2];
        WORD& field=(port&1)?ch.count:ch.address;
        if(dmaFlip_==0)field=WORD((field&0xff00)|value);
        else field=WORD((field&0x00ff)|(WORD(value)<<8));
        if(port&1)ch.baseCount=ch.count;
        else ch.baseAddress=ch.address;
        dmaFlip_^=1;
        return;
    }
    if(port>=0x20 && port<=0x23) {
        dmaChannels_[port-0x20].bank=value;
        return;
    }
    switch(port) {
    case 0x18:
        dmaCommand_=value;
        dmaStatus_=BYTE((dmaStatus_&0x0f)|(activeDmaRequests()<<4));
        break;
    case 0x19:
        if(value&4)dmaRequest_|=BYTE(1u<<(value&3));
        else dmaRequest_&=BYTE(~(1u<<(value&3)));
        dmaStatus_=BYTE((dmaStatus_&0x0f)|(activeDmaRequests()<<4));
        break;
    case 0x1a:
        if(value&4)dmaMask_|=BYTE(1u<<(value&3));
        else dmaMask_&=BYTE(~(1u<<(value&3)));
        break;
    case 0x1b:
        dmaChannels_[value&3].mode=value;
        dmaBlockActive_=BYTE(dmaBlockActive_&~(1u<<(value&3)));
        break;
    case 0x1c:dmaFlip_=0;break;
    case 0x1d:
        resetDma();
        // 8237 master clear masks all four channels. This differs from
        // the card's power-on reset defaults.
        dmaMask_=0x0f;
        break;
    case 0x1e:dmaMask_=0;break;
    case 0x1f:dmaMask_=BYTE(value&0x0f);break;
    }
}

void Main8088Bus::setDmaDevice(unsigned channel,DmaRead reader,DmaWrite writer)
{
    if(channel>=dmaChannels_.size())throw std::out_of_range("DMA channel");
    dmaReaders_[channel]=std::move(reader);
    dmaWriters_[channel]=std::move(writer);
}
BYTE Main8088Bus::activeDmaRequests() const
{
    // Undriven inputs never assert a request when the polarity changes.
    const BYTE external=BYTE(dmaExternalRequest_^((dmaCommand_&0x40)?0x0f:0));
    return BYTE(dmaRequest_|(external&dmaExternalDriven_));
}

void Main8088Bus::setDmaRequest(unsigned channel,bool levelHigh)
{
    if(channel>=dmaChannels_.size())throw std::out_of_range("DMA channel");
    const BYTE bit=BYTE(1u<<channel);
    dmaExternalDriven_=BYTE(dmaExternalDriven_|bit);
    if(levelHigh)dmaExternalRequest_=BYTE(dmaExternalRequest_|bit);
    else dmaExternalRequest_=BYTE(dmaExternalRequest_&~bit);
    dmaStatus_=BYTE((dmaStatus_&0x0f)|(activeDmaRequests()<<4));
}

bool Main8088Bus::serviceDma()
{
    // 8237 transfer modes: 01=I/O to memory, 10=memory to I/O.
    // Requests are explicitly serviced by the device scheduler.
    if(dmaCommand_&0x04)return false; // controller disabled
    // Memory-to-memory uses CH0 as the request/source channel and CH1
    // as the destination/count channel. A Block transfer continues after
    // its initiating DREQ is released, until CH1 reaches terminal count.
    const bool memoryPairRequested=(activeDmaRequests()&0x01)!=0;
    const bool memoryPairBlock=(dmaChannels_[1].mode&0xc0u)==0x80u;
    if((dmaCommand_&0x01) &&
       (dmaMask_&0x03)==0 &&
       (memoryPairRequested ||
        (memoryPairBlock && (dmaBlockActive_&0x01)))) { // CH0 -> CH1
        auto& source=dmaChannels_[0];
        auto& dest=dmaChannels_[1];
        const std::uint32_t sourceAddress=
            (std::uint32_t(source.bank&0x0f)<<16)|source.address;
        const std::uint32_t destAddress=
            (std::uint32_t(dest.bank&0x0f)<<16)|dest.address;
        if(memoryPairBlock)dmaBlockActive_=BYTE(dmaBlockActive_|0x01);
        dmaTemporary_=readMemory(sourceAddress);
        writeMemory(destAddress,dmaTemporary_);
        if(!(dmaCommand_&0x02))
            source.address=WORD(source.address+((source.mode&0x20)?-1:1));
        dest.address=WORD(dest.address+((dest.mode&0x20)?-1:1));
        const bool terminal=dest.count==0;
        dest.count=WORD(dest.count-1);
        if(terminal) {
            dmaStatus_=BYTE(dmaStatus_|0x02); // CH1 terminal count
            dmaRequest_=BYTE(dmaRequest_&~0x01);
            dmaBlockActive_=BYTE(dmaBlockActive_&~0x01);
            dmaStatus_=BYTE((dmaStatus_&0x0f)|(activeDmaRequests()<<4));
            if(dest.mode&0x10) {
                dest.address=dest.baseAddress;
                dest.count=dest.baseCount;
            } else dmaMask_=BYTE(dmaMask_|0x03);
        }
        return true;
    }
    for(unsigned rank=0;rank<4;++rank) {
        const unsigned channel=(rank+((dmaCommand_&0x10)?dmaPriorityBase_:0u))&3u;
        const BYTE bit=BYTE(1u<<channel);
        // In memory-to-memory mode channels 0/1 are reserved as a pair.
        // Other channels remain usable when the pair is idle.
        if((dmaCommand_&0x01) && channel<2)continue;
        if(dmaMask_&bit)continue;
        auto& ch=dmaChannels_[channel];
        const unsigned transferMode=(ch.mode>>6)&3u;
        const bool requested=(activeDmaRequests()&bit)!=0;
        // In Block mode a DREQ pulse starts a transfer which runs to TC;
        // Demand and Single modes require an asserted request per service.
        if(!requested && !(transferMode==2 && (dmaBlockActive_&bit)))
            continue;
        // Cascade mode delegates bus ownership to a secondary DMA
        // controller; it must not perform an ordinary byte transfer.
        if((ch.mode&0xc0u)==0xc0u)continue;
        const unsigned type=(ch.mode>>2)&3;
        if((type==1 && !dmaReaders_[channel]) ||
           (type==2 && !dmaWriters_[channel]) ||
           type==3)continue;
        if(transferMode==2)dmaBlockActive_=BYTE(dmaBlockActive_|bit);
        // The DMA bank byte supplies address bits 16-19 on a 20-bit bus.
        const std::uint32_t address=
            ((std::uint32_t(ch.bank&0x0f)<<16)|ch.address)&0xfffff;
        if(type==1)writeMemory(address,dmaReaders_[channel]());
        else if(type==2)dmaWriters_[channel](readRam(address));
        // type==0 is verify: advance without transferring data.
        ch.address=WORD(ch.address+((ch.mode&0x20)?-1:1));
        const bool terminal=(ch.count==0);
        ch.count=WORD(ch.count-1);
        if(dmaCommand_&0x10)dmaPriorityBase_=BYTE((channel+1u)&3u);
        if(terminal) {
            dmaStatus_=BYTE(dmaStatus_|bit);
            dmaRequest_=BYTE(dmaRequest_&~bit);
            dmaBlockActive_=BYTE(dmaBlockActive_&~bit);
            dmaStatus_=BYTE((dmaStatus_&0x0f)|(activeDmaRequests()<<4));
            if(ch.mode&0x10) {
                // Auto-initialize restores the base address as well as count.
                ch.address=ch.baseAddress;
                ch.count=ch.baseCount;
            } else dmaMask_=BYTE(dmaMask_|bit);
        }
        return true;
    }
    return false;
}
