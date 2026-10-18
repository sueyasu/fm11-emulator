#include "Main8088Bus.h"
#include <utility>

Main8088Bus::Main8088Bus():memory_(kMemorySize,0) {}

void Main8088Bus::reset()
{
    cardIoRam_.fill(0);
    picMask_=0;picRequest_=0;picInService_=0;
    picVectorBase_=8;picReadIsr_=0;picInitStep_=0;
    picNeedsIcw4_=false;picSingle_=false;
    resetDma();
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
            picInitStep_=0;
        } else picMask_=value;
        return;
    }
    if(port>=0xfc00 && port<=0xfcff) cardIoRam_[port&0xff]=value;
    else if(port>=0xfd00 && port<=0xfdff && systemWriter_) systemWriter_(port,value);
}

void Main8088Bus::requestIrq(unsigned line)
{
    if(line<8)picRequest_=BYTE(picRequest_|(1u<<line));
}
bool Main8088Bus::interruptPending() const
{
    return (picRequest_&BYTE(~picMask_)&BYTE(~picInService_))!=0;
}
BYTE Main8088Bus::acknowledgeInterrupt()
{
    const BYTE eligible=BYTE(picRequest_&BYTE(~picMask_)&BYTE(~picInService_));
    for(unsigned line=0;line<8;++line)if(eligible&(1u<<line)) {
        picRequest_=BYTE(picRequest_&~(1u<<line));
        picInService_=BYTE(picInService_|(1u<<line));
        return BYTE(picVectorBase_+line);
    }
    return BYTE(picVectorBase_+7); // Spurious IRQ7 when none is pending
}

void Main8088Bus::resetDma()
{
    dmaChannels_.fill(DmaChannel{});
    dmaCommand_=dmaMask_=dmaStatus_=dmaFlip_=dmaRequest_=0;
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
    if(port==0x1d)return 0; // temporary register (not yet modelled)
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
        dmaFlip_^=1;
        return;
    }
    if(port>=0x20 && port<=0x23) {
        dmaChannels_[port-0x20].bank=value;
        return;
    }
    switch(port) {
    case 0x18:dmaCommand_=value;break;
    case 0x19:
        if(value&4)dmaRequest_|=BYTE(1u<<(value&3));
        else dmaRequest_&=BYTE(~(1u<<(value&3)));
        dmaStatus_=BYTE((dmaStatus_&0x0f)|(dmaRequest_<<4));
        break;
    case 0x1a:
        if(value&4)dmaMask_|=BYTE(1u<<(value&3));
        else dmaMask_&=BYTE(~(1u<<(value&3)));
        break;
    case 0x1b:dmaChannels_[value&3].mode=value;break;
    case 0x1c:dmaFlip_=0;break;
    case 0x1d:resetDma();break;
    case 0x1e:dmaMask_=0;break;
    case 0x1f:dmaMask_=BYTE(value&0x0f);break;
    }
}
