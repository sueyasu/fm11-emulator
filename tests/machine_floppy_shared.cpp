#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>
#include <vector>

int main()
{
    Machine machine;
    auto board6809=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* p6809=board6809.get();
    auto board8088=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* p8088=board8088.get();
    machine.addBoard(std::move(board6809));
    machine.addBoard(std::move(board8088));
    std::vector<BYTE> image(40u*2*16*256,0);
    for(unsigned i=0;i<256;++i)image[i]=BYTE(i^0x5a);
    p6809->loadFloppy2d(image);
    if(p8088->bus().floppyImage()!=image)return 1;

    auto& bus=p8088->bus();
    bus.reset();
    bus.writeIo(0xfd19,0);
    bus.writeIo(0xfd1a,1);
    bus.writeIo(0xfd1c,0);
    bus.writeIo(0xfd1d,0);
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0);
    bus.writeIo(0x10,0x20);
    bus.writeIo(0x11,0xff);
    bus.writeIo(0x11,0);
    bus.writeIo(0x1b,0x44);
    bus.writeIo(0x1a,0);
    // Device callback is installed when the image is inserted through
    // either adapter; the shared instance must work for both.
    bus.writeIo(0xfd18,0x80);
    for(unsigned i=0;i<256;++i)
        if(bus.readRam(0x2000+i)!=BYTE(i^0x5a))return 2;
    // Writing a sector through the 8088 card must update the media
    // visible through the 6809 adapter without reloading the image.
    for(unsigned i=0;i<256;++i)bus.writeMemory(0x3000+i,BYTE(i^0xa5));
    bus.writeIo(0x1c,0);
    bus.writeIo(0x10,0);
    bus.writeIo(0x10,0x30);
    bus.writeIo(0x11,0xff);
    bus.writeIo(0x11,0);
    bus.writeIo(0x1b,0x48);
    bus.writeIo(0x1a,0);
    bus.writeIo(0xfd18,0xa0);
    const auto shared=p6809->floppyImage();
    for(unsigned i=0;i<256;++i)
        if(shared[i]!=BYTE(i^0xa5))return 3;
    if(!p6809->floppyDirty())return 4;
    return 0;
}
