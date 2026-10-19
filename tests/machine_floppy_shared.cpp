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
    // CHS register writes are visible from the other CPU card.
    p6809->writeCpu(0xfd19,2);
    p6809->writeCpu(0xfd1a,3);
    if(p8088->bus().readIo(0xfd19)!=2 || p8088->bus().readIo(0xfd1a)!=3)
        return 8;
    p8088->bus().writeIo(0xfd19,0);
    p8088->bus().writeIo(0xfd1a,1);
    if(p6809->readCpu(0xfd19)!=0 || p6809->readCpu(0xfd1a)!=1)
        return 9;
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
    // The same system device also handles programmed I/O on the 6809.
    p6809->writeCpu(0xfd19,0);
    p6809->writeCpu(0xfd1a,1);
    p6809->writeCpu(0xfd1c,0);
    p6809->writeCpu(0xfd1d,0);
    p6809->writeCpu(0xfd50,0); // PIO, not 6844 DMA
    p6809->writeCpu(0xfd18,0x80);
    if((p6809->readCpu(0xfd18)&2)==0)return 5;
    for(unsigned i=0;i<256;++i)
        if(p6809->readCpu(0xfd1b)!=BYTE(i^0xa5))return 6;
    if(p6809->readCpu(0xfd18)!=0)return 7;
    // Standard FDC parameter registers use the same main-board bus.
    bus.loadFloppy2hd(std::vector<BYTE>(77u*2*8*1024,0));
    bus.writeIo(0xfd31,7);
    bus.writeIo(0xfd32,2);
    if(bus.readIo(0xfd31)!=7 || bus.readIo(0xfd32)!=2)return 10;
    // Unrelated card-local control registers must not be claimed by bus.
    MainBoard main;
    BYTE value=0;
    if(main.readIo(0xfd55,value) || main.writeIo(0xfd55,0x7f))
        return 11;
    // Main-board USART is shared across the two main CPU cards.
    p6809->queueSerialInput(0x41);
    if((bus.readIo(0xfd07)&0x02)==0 || bus.readIo(0xfd06)!=0x41)return 12;
    if((p6809->readCpu(0xfd07)&0x02)!=0)return 13;
    bus.writeIo(0xfd06,0x42);
    BYTE sent=0;
    if(!p6809->takeSerialOutput(sent) || sent!=0x42)return 14;
    p6809->writeCpu(0xfd06,0x43);
    if(!p6809->takeSerialOutput(sent) || sent!=0x43)return 15;
    // USART receive IRQ reaches the on-card 8259 only on assertion edges.
    bus.writeIo(0xfd07,0x04); // receiver enabled, transmitter disabled
    p6809->queueSerialInput(0x51);
    bus.refreshSystemIrq();
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=12)return 16;
    bus.writeIo(0x00,0x20); // EOI
    bus.refreshSystemIrq();
    if(bus.interruptPending())return 17; // no phantom re-trigger
    if(bus.readIo(0xfd06)!=0x51)return 18;
    bus.refreshSystemIrq();
    p6809->queueSerialInput(0x52);
    bus.refreshSystemIrq();
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=12)return 19;
    bus.writeIo(0x00,0x20);
    if(bus.readIo(0xfd06)!=0x52)return 20;
    // Two simultaneous board-level sources retain the 8259 priority order.
    auto irqBoard=std::make_shared<MainBoard>();
    Main8088Bus irqBus;
    irqBus.setMainBoard(irqBoard);
    irqBoard->setIrqLine(MainBoard::IrqLine::Timer,true);
    irqBoard->setIrqLine(MainBoard::IrqLine::Disk5Inch,true);
    irqBus.refreshSystemIrq();
    if(!irqBus.interruptPending() || irqBus.acknowledgeInterrupt()!=8)return 21;
    irqBus.writeIo(0x00,0x20);
    if(!irqBus.interruptPending() || irqBus.acknowledgeInterrupt()!=14)return 22;
    irqBus.writeIo(0x00,0x20);
    irqBus.refreshSystemIrq();
    if(irqBus.interruptPending())return 23;
    irqBoard->setIrqLine(MainBoard::IrqLine::Timer,false);
    irqBus.refreshSystemIrq();
    irqBoard->setIrqLine(MainBoard::IrqLine::Timer,true);
    irqBus.refreshSystemIrq();
    if(!irqBus.interruptPending() || irqBus.acknowledgeInterrupt()!=8)return 24;
    return 0;
}
