#include "Main8088Bus.h"
#include <utility>

Main8088Bus::Main8088Bus():memory_(kMemorySize,0) {}

void Main8088Bus::reset()
{
    cardIoRam_.fill(0);
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
    if(port>=0xfc00 && port<=0xfcff) return cardIoRam_[port&0xff];
    if(port>=0xfd00 && port<=0xfdff && systemReader_) return systemReader_(port);
    // 8259 PIC and 8237 DMA belong to the future Sim86 adapter.
    return 0xff;
}

void Main8088Bus::writeIo(WORD port,BYTE value)
{
    if(port>=0xfc00 && port<=0xfcff) cardIoRam_[port&0xff]=value;
    else if(port>=0xfd00 && port<=0xfdff && systemWriter_) systemWriter_(port,value);
}
