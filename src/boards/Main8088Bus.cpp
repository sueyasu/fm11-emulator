#include "Main8088Bus.h"
#include <utility>
#include <stdexcept>
#include <algorithm>

Main8088Bus::Main8088Bus():memory_(kMemorySize,0) {}

void Main8088Bus::reset()
{
    cardIoRam_.fill(0);
    picMask_=0;picRequest_=0;picInService_=0;
    systemIrqPrevious_=0;
    picVectorBase_=8;picReadIsr_=0;picInitStep_=0;
    picNeedsIcw4_=false;picSingle_=false;picAutoEoi_=false;
    resetDma();
    floppy_->finishTransfer();
}

void Main8088Bus::loadFloppy2d(const std::vector<BYTE>& image)
{
    floppy_->load2d(image);
    setDmaDevice(0,[this](){return floppyReadData();},
                   [this](BYTE value){floppyWriteData(value);});
}

void Main8088Bus::loadFloppy2hd(const std::vector<BYTE>& image)
{
    floppy_->load2hd(image);
    setDmaDevice(1,[this](){return floppyReadData();},
                   [this](BYTE value){floppyWriteData(value);});
}

BYTE Main8088Bus::floppyReadData()
{
    return floppy_->transferRead();
}

void Main8088Bus::floppyWriteData(BYTE value)
{
    floppy_->transferWrite(value);
}

void Main8088Bus::floppyCommand(BYTE command,unsigned channel)
{
    const auto decoded=FloppyDevice::decodeCommand(command);
    const bool writing=decoded==FloppyDevice::Command::WriteSector;
    if(decoded==FloppyDevice::Command::Other) {
        floppy_->finishTransfer();
        return;
    }
    const unsigned base=channel==0?0x18:0x30;
    if(!floppy_->beginSector(channel,floppy_->registerValue(base+1),
            floppy_->registerValue(channel==0?0x1c:0x34),
            floppy_->registerValue(base+2),
            floppy_->registerValue(channel==0?0x1d:0x35),writing))return;

    // The 8237 remains the bus master; the system FDC only supplies data.
    setDmaRequest(channel,true);
    for(unsigned i=0;i<floppy_->transferLength();++i) {
        if(!serviceDma() || floppy_->transferPosition()==floppy_->transferLength())
            break;
    }
    setDmaRequest(channel,false);
    if(floppy_->transferPosition()!=floppy_->transferLength())
        floppy_->transferError();
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
    const_cast<Main8088Bus*>(this)->refreshSystemIrq();
    if(port>=0x10 && port<=0x23)return readDma(port);
    if(port==0x00)return picReadIsr_?picInService_:picRequest_;
    if(port==0x01)return picMask_;
    if(port>=0xfc00 && port<=0xfcff) return cardIoRam_[port&0xff];
    if(((port==0xfd04 || port==0xfd06 || port==0xfd07) ||
        (port>=0xfd38 && port<=0xfd3f)) && mainBoard_) {
        BYTE value=0;
        if(mainBoard_->readIo(port,value))return value;
    }
    if(port>=0xfd00 && port<=0xfdff && systemReader_) return systemReader_(port);
    if(((port>=0xfd18 && port<=0xfd1d && floppy_->has2d()) ||
        (port>=0xfd30 && port<=0xfd35 && floppy_->has2hd()))) {
        if(port==0xfd18 || port==0xfd30)return floppy_->transferStatus();
        if(port==0xfd1b || port==0xfd33)
            return const_cast<Main8088Bus*>(this)->floppyReadData();
        BYTE busValue=0;
        if(mainBoard_ && mainBoard_->readIo(port,busValue))return busValue;
        return floppy_->registerValue(port&0xff);
    }
    // 8259 PIC and 8237 DMA belong to the future Sim86 adapter.
    return 0xff;
}

void Main8088Bus::writeIo(WORD port,BYTE value)
{
    refreshSystemIrq();
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
    else if(((port==0xfd04 || port==0xfd06 || port==0xfd07) ||
             (port>=0xfd38 && port<=0xfd3f)) && mainBoard_) {
        mainBoard_->writeIo(port,value);
        refreshSystemIrq();
    }
    else if(port>=0xfd00 && port<=0xfdff && systemWriter_) systemWriter_(port,value);
    else if(((port>=0xfd18 && port<=0xfd1d && floppy_->has2d()) ||
             (port>=0xfd30 && port<=0xfd35 && floppy_->has2hd()))) {
        if(port==0xfd18 || port==0xfd30)floppyCommand(value,port==0xfd18?0:1);
        else if(port==0xfd1b || port==0xfd33)floppyWriteData(value);
        else if(!mainBoard_ || !mainBoard_->writeIo(port,value))
            floppy_->writeRegister(port&0xff,value);
    }
}

void Main8088Bus::refreshSystemIrq()
{
    if(!mainBoard_)return;
    const BYTE lines=mainBoard_->irqLines();
    const BYTE rising=static_cast<BYTE>(lines & ~systemIrqPrevious_);
    // Main-board IRQ wiring is converted to 8259 edge requests on this card.
    // A continuously asserted source must not retrigger on PIC EOI.
    for(unsigned line=0;line<8;++line)
        if(rising & (1u<<line))requestIrq(line);
    systemIrqPrevious_=lines;
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
