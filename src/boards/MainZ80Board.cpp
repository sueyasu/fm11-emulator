#include "MainZ80Board.h"

MainZ80Board::MainZ80Board():core_(*this) {}

void MainZ80Board::reset()
{
    bank_=0;
    core_.reset();
}

std::uint32_t MainZ80Board::physicalAddress(WORD address) const
{
    const BYTE bank=(address>=0x4000 && address<0xc000)?bank_:fixedBank_;
    return (static_cast<std::uint32_t>(bank)<<16)|address;
}

BYTE MainZ80Board::readMemory(WORD address) const
{
    if(address>=0xfc00)return readIo(address);
    const auto physical=physicalAddress(address);
    return (*memory_)[physical%installedRamBytes_];
}

BYTE MainZ80Board::peekMemory(WORD address) const
{
    if(address>=0xfc00) {
        if(address>=0xfc80 && address<=0xfcff && mainBoard_)
            return mainBoard_->displaySubsystem().peekMemory(WORD(0x9f80+address-0xfc80));
        return 0xff;
    }
    return (*memory_)[physicalAddress(address)%installedRamBytes_];
}

void MainZ80Board::writeMemory(WORD address,BYTE value)
{
    if(address>=0xfc00) {writeIo(address,value);return;}
    const auto physical=physicalAddress(address);
    (*memory_)[physical%installedRamBytes_]=value;
}

BYTE MainZ80Board::readIo(WORD address) const
{
    if(address==0xfd2f)return bank_;
    if(!mainBoard_)return 0xff;
    if(address>=0xfc80 && address<=0xfcff)
        return mainBoard_->displaySubsystem().readShared(address-0xfc80);
    BYTE value=0;
    if(address>=0xfd00 && address<=0xfdff &&
       mainBoard_->readIo(address,value))return value;
    return 0xff;
}

void MainZ80Board::writeIo(WORD address,BYTE value)
{
    if(address==0xfd2f) {bank_=value&0x0f;return;}
    if(!mainBoard_)return;
    if(address>=0xfc80 && address<=0xfcff) {
        mainBoard_->displaySubsystem().writeShared(address-0xfc80,value);
        return;
    }
    if(address>=0xfd00 && address<=0xfdff)
        mainBoard_->writeIo(address,value);
}
