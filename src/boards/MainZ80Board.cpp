#include "MainZ80Board.h"

MainZ80Board::MainZ80Board():core_(*this) {}

void MainZ80Board::reset()
{
    bank_=0;
    core_.reset();
}

BYTE MainZ80Board::readMemory(WORD address) const
{
    if(address>=0xfc00)return readIo(address);
    const std::uint32_t physical=(address>=0x4000 && address<0xc000)
        ? (static_cast<std::uint32_t>(bank_)<<16)|address : address;
    return (*memory_)[physical];
}

void MainZ80Board::writeMemory(WORD address,BYTE value)
{
    if(address>=0xfc00) {writeIo(address,value);return;}
    const std::uint32_t physical=(address>=0x4000 && address<0xc000)
        ? (static_cast<std::uint32_t>(bank_)<<16)|address : address;
    (*memory_)[physical]=value;
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
