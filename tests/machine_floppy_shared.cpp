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

    // A real PTM expiration on the 6809 side reaches the same IR0 input
    // of the 8088 PIC, and acknowledgement deasserts the main-board line.
    p6809->writeCpu(0xfd39,0x01);
    p6809->writeCpu(0xfd38,0x50);
    p6809->writeCpu(0xfd3a,0);
    p6809->writeCpu(0xfd3b,4);
    p6809->advancePtm(4);
    bus.refreshSystemIrq();
    if(!bus.interruptPending() || bus.acknowledgeInterrupt()!=8)return 25;
    bus.writeIo(0x00,0x20);
    (void)p6809->readCpu(0xfd39);
    (void)p6809->readCpu(0xfd3a);
    bus.refreshSystemIrq();
    if(bus.interruptPending())return 26;
    // The MC6840 timer state now belongs to the shared main board.
    auto timerBoard=std::make_shared<MainBoard>();
    Main6809Board timerCard(CpuType::MC6809);
    timerCard.setMainBoard(timerBoard);
    timerCard.writeCpu(0xfd39,0x01);
    timerCard.writeCpu(0xfd38,0x50);
    timerCard.writeCpu(0xfd3a,0);
    timerCard.writeCpu(0xfd3b,4);
    if(timerBoard->ptmDevice()->channels[0].counter!=4)return 27;
    timerCard.advancePtm(4);
    if((timerBoard->ptmDevice()->status&0x81)!=0x81)return 28;
    if((timerBoard->irqLines()&0x01)==0)return 29;
    (void)timerCard.readCpu(0xfd39);
    if((timerBoard->irqLines()&0x01)!=0)return 30;
    // PTM execution no longer requires a 6809 CPU-card method call.
    auto standalone=std::make_shared<MainBoard>();
    auto timer=standalone->ptmDevice();
    timer->write(0x39,0x01);
    timer->write(0x38,0x50);
    timer->write(0x3a,0);
    timer->write(0x3b,4);
    if(timer->nextEventNanoseconds()==UINT64_MAX)return 31;
    timer->advance(4);
    if(!timer->irqRequest() || (timer->read(0x39)&0x81)!=0x81)return 32;
    if(timer->irqRequest())return 33;
    (void)timer->read(0x3a);
    if(timer->read(0x39)!=0)return 34;

    // Both CPU-card buses access the same PTM registers on the main board.
    Machine ptmMachine;
    auto ptm6809=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* card09=ptm6809.get();
    auto ptm8088=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* card86=ptm8088.get();
    ptmMachine.addBoard(std::move(ptm6809));
    ptmMachine.addBoard(std::move(ptm8088));
    card86->bus().writeIo(0xfd39,0x01);
    card86->bus().writeIo(0xfd38,0x50);
    card86->bus().writeIo(0xfd3a,0);
    card86->bus().writeIo(0xfd3b,4);
    if(card09->readCpu(0xfd3a)!=0 || card09->readCpu(0xfd3b)!=4)
        return 35;
    card09->advancePtm(4);
    card86->bus().refreshSystemIrq();
    if(!card86->bus().interruptPending() ||
       card86->bus().acknowledgeInterrupt()!=8)return 36;
    // ACK issued via 8088 must clear the shared source seen by 6809.
    if((card86->bus().readIo(0xfd39)&0x81)!=0x81)return 37;
    card09->syncPtmIrq();
    if((card09->readCpu(0xfd03)&0x01)!=0)return 38;
    // FD04 event latch is physically shared, regardless of CPU card.
    p6809->signalAttention();
    p6809->signalBreak();
    if((bus.readIo(0xfd04)&0x03)!=0x03)return 39;
    if((p6809->readCpu(0xfd04)&0x03)!=0)return 40;
    p6809->signalAttention();
    p6809->signalBreak();
    bus.writeIo(0xfd04,0x80); // ATTENTION ACK does not clear BREAK
    if((p6809->readCpu(0xfd04)&0x03)!=0x02)return 41;
    if((bus.readIo(0xfd04)&0x03)!=0)return 42;
    // FD05 control/status is common; HALT request latches bit 7.
    p6809->writeCpu(0xfd05,0x80);
    if((bus.readIo(0xfd05)&0x80)==0)return 43;
    bus.writeIo(0xfd05,0x00);
    if((p6809->readCpu(0xfd05)&0x80)!=0)return 44;
    // Writing a zero releases display HALT.
    machine.reset();
    if(bus.readIo(0xfd05)!=0)return 45;
    bus.writeIo(0xfd05,0x80);
    if((p6809->readCpu(0xfd05)&0x80)==0)return 46;
    // FD05 bit 0 is a CPU-card takeover request, distinct from bit 7
    // display-sub-CPU HALT status; the board does not arbitrate yet.
    MainBoard selector;
    if(selector.optionalCpuRequested())return 47;
    selector.writeIo(0xfd05,0x01);
    if(!selector.optionalCpuRequested() || selector.subCpuStatus()!=0)return 48;
    selector.writeIo(0xfd05,0x00);
    if(selector.optionalCpuRequested())return 49;
    selector.writeIo(0xfd05,0x80);
    if(selector.optionalCpuRequested() || (selector.subCpuStatus()&0x80)==0)
        return 50;
    selector.resetSubCpuControl();
    if(selector.optionalCpuRequested() || selector.subCpuStatus()!=0)return 51;
    // FD05 selection is opt-in; manual board selection remains supported.
    Machine selectorMachine;
    auto primary=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto secondary=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* secondaryCard=secondary.get();
    selectorMachine.addBoard(std::move(primary));
    selectorMachine.addBoard(std::move(secondary));
    selectorMachine.setActiveBoard(1);
    secondaryCard->bus().writeIo(0xfd05,0x00);
    if(selectorMachine.activeBoard()!=secondaryCard)return 52;
    selectorMachine.enableFd05Selection(true);
    if(selectorMachine.activeBoard()!=&selectorMachine.board(0))return 53;
    secondaryCard->bus().writeIo(0xfd05,0x01);
    selectorMachine.enableFd05Selection(true);
    if(selectorMachine.activeBoard()!=secondaryCard)return 54;
    selectorMachine.enableFd05Selection(false);
    secondaryCard->bus().writeIo(0xfd05,0x00);
    if(selectorMachine.activeBoard()!=secondaryCard)return 55;
    selectorMachine.enableFd05Selection(true);
    if(selectorMachine.activeBoard()!=&selectorMachine.board(0))return 56;
    // Resetting either CPU card must not clear state on the shared board.
    p6809->queueSerialInput(0x61);
    p6809->signalAttention();
    p6809->writeCpu(0xfd05,0x80);
    p8088->reset();
    p6809->reset();
    if((bus.readIo(0xfd07)&0x02)==0 || bus.readIo(0xfd06)!=0x61)
        return 57;
    if((bus.readIo(0xfd04)&0x01)==0)return 58;
    if((bus.readIo(0xfd05)&0x80)==0)return 59;
    // Conversely a full machine reset clears common latches and USART.
    p6809->queueSerialInput(0x62);
    p6809->signalAttention();
    machine.reset();
    if((bus.readIo(0xfd07)&0x02)!=0)return 60;
    if((bus.readIo(0xfd04)&0x03)!=0)return 61;
    if(bus.readIo(0xfd05)!=0)return 62;
    // HALT request is owned by the display subsystem rather than either
    // main CPU card. A CPU-card reset must leave it intact.
    MainBoard displayOwner;
    if(displayOwner.displaySubsystem().haltRequested())return 63;
    displayOwner.writeIo(0xfd05,0x80);
    if(!displayOwner.displaySubsystem().haltRequested() ||
       displayOwner.subCpuStatus()!=0x80)return 64;
    displayOwner.writeIo(0xfd05,0x00);
    if(displayOwner.displaySubsystem().haltRequested())return 65;
    displayOwner.reset();
    if(displayOwner.displaySubsystem().haltRequested() ||
       displayOwner.subCpuStatus()!=0)return 66;
    // Basic sub-CPU map: VRAM 0000-7FFF, RAM 8000-9FFF,
    // unmapped A000-AFFF, and 20KiB ROM B000-FFFF.
    auto& display=displayOwner.displaySubsystem();
    DisplaySubsystem::RomImage displayRom{};
    displayRom[0]=0x91;
    displayRom[0x1000]=0x83;
    displayRom[0x4fff]=0x42;
    display.loadRom(displayRom);
    display.writeMemory(0x0000,0x12);
    display.writeMemory(0x7fff,0x34);
    display.writeMemory(0x8000,0x56);
    display.writeMemory(0x9fff,0x78);
    if(display.readMemory(0)!=0x12 ||
       display.readMemory(0x7fff)!=0x34 ||
       display.readMemory(0x8000)!=0x56 ||
       display.readMemory(0x9fff)!=0x78)return 67;
    display.writeMemory(0xb000,0);
    display.writeMemory(0xc000,0);
    display.writeMemory(0xffff,0);
    if(display.readMemory(0xb000)!=0x91 ||
       display.readMemory(0xc000)!=0x83 ||
       display.readMemory(0xffff)!=0x42)return 68;
    display.writeMemory(0xa000,0x55);
    if(display.readMemory(0xa000)!=0xff ||
       display.readMemory(0xbfff)!=0xff)return 69;
    displayOwner.reset();
    if(display.readMemory(0x7fff)!=0x34 ||
       display.readMemory(0x9fff)!=0x78 ||
       display.readMemory(0xc000)!=0x91)return 70;
    return 0;
}
