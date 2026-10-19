#include "FloppyDevice.h"

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
    if(fdc.beginSector(0,0,0,1,0,false))return 2;
    if(fdc.transferStatus()!=0x10)return 3;
    fdc.finishTransfer();
    if(fdc.transferStatus()!=0)return 4;
    return 0;
}
