#include "Main8088Bus.h"
#include <utility>

Main8088Bus::Main8088Bus():memory_(kMemorySize,0) {}

void Main8088Bus::reset()
{
    cardIoRam_.fill(0);
    picMask_=0;picRequest_=0;picInService_=0;
    picVectorBase_=8;picReadIsr_=0;picInitStep_=0;
    picNeedsIcw4_=false;picSingle_=false;
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
    if(port==0x00)return picReadIsr_?picInService_:picRequest_;
    if(port==0x01)return picMask_;
    if(port>=0xfc00 && port<=0xfcff) return cardIoRam_[port&0xff];
    if(port>=0xfd00 && port<=0xfdff && systemReader_) return systemReader_(port);
    // 8259 PIC and 8237 DMA belong to the future Sim86 adapter.
    return 0xff;
}

void Main8088Bus::writeIo(WORD port,BYTE value)
{
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
