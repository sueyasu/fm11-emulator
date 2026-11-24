#include "FloppyDevice.h"
#include <vector>

int main()
{
    using C=FloppyDevice::Command;
    for(unsigned value=0;value<256;++value) {
        const auto expected=(value&0xf0)==0x80?C::ReadSector:
                            (value&0xf0)==0xa0?C::WriteSector:C::Other;
        if(FloppyDevice::decodeCommand(static_cast<BYTE>(value))!=expected)
            return 1;
    }
    FloppyDevice fdc;
    fdc.enableSeekTiming(true);
    fdc.writeRegister(0x1b,12);
    if(!fdc.beginTypeI(0,0,0x10) || !fdc.seekBusy(0,0) || fdc.registerValue(0x19)!=0)return 10;
    fdc.advanceSeekNanoseconds(10000000);
    if(!fdc.seekBusy(0,0) || fdc.headTrack(0,0)!=0)return 11;
    fdc.advanceSeekNanoseconds(100000000);
    if(fdc.seekBusy(0,0) || !fdc.seekComplete(0,0) || fdc.headTrack(0,0)!=12 || fdc.registerValue(0x19)!=12)return 12;
    fdc.ackSeek(0,0);
    if(fdc.seekComplete(0,0))return 13;
    if(!fdc.beginTypeI(0,0,0x00))return 14;
    fdc.advanceSeekNanoseconds(100000000);
    if(fdc.headTrack(0,0)!=0 || fdc.registerValue(0x19)!=0 ||
       (fdc.typeIStatus(0,0)&0x04)==0)return 15;
    fdc.writeRegister(0x33,5);
    if(!fdc.beginTypeI(1,1,0x10))return 16;
    fdc.advanceSeekNanoseconds(50000000);
    if(fdc.headTrack(1,1)!=5 || fdc.headTrack(0,0)!=0)return 17;
    // A logical track-register write cannot move the physical head.
    fdc.load2d(std::vector<BYTE>(40u*2*16*256,0));
    fdc.writeRegister(0x19,1);
    std::size_t seekOffset=0;
    if(fdc.sectorOffset(0,1,0,1,0,seekOffset))return 19;
    if(!fdc.beginTypeI(0,0,0x10))return 20;
    fdc.advanceSeekNanoseconds(100000000);
    if(fdc.sectorOffset(0,1,0,1,0,seekOffset))return 21;
    fdc.enableSeekTiming(false);
    if(fdc.beginTypeI(0,0,0x10))return 18;
    if(fdc.beginSector(0,0,0,1,0,false))return 2;
    if(fdc.transferStatus()!=0x10)return 3;
    fdc.finishTransfer();
    if(fdc.transferStatus()!=0)return 4;
    return 0;
}
