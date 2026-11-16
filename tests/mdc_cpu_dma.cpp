#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>
#include <vector>
#include <cstdio>

static BYTE result6809(Main6809Board& cpu) {
    const BYTE first=cpu.readCpu(0xfdc1);
    for(unsigned i=1;i<10;++i)cpu.readCpu(0xfdc1);
    return first;
}
static BYTE result8088(Main8088Bus& bus) {
    const BYTE first=bus.readIo(0xfdc1);
    for(unsigned i=1;i<10;++i)bus.readIo(0xfdc1);
    return first;
}
int main() {
    Machine machine;
    auto b6809=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=b6809.get();
    machine.addBoard(std::move(b6809));
    auto b8088=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* x86=b8088.get();
    machine.addBoard(std::move(b8088));
    auto hdd=machine.systemBoard().hddDevice();
    std::vector<BYTE> disk(157u*4*32*256,0xee);
    disk[0]=0x5c;
    hdd->load(disk);
    auto& bus=x86->bus();

    // 6809 channel 0 DMA: I/O -> memory 2000h, 256 bytes.
    cpu->writeCpu(0xfd40,0x20);
    cpu->writeCpu(0xfd41,0x00);
    cpu->writeCpu(0xfd42,0x01);
    cpu->writeCpu(0xfd43,0x00);
    cpu->writeCpu(0xfd50,0x04);
    cpu->writeCpu(0xfd94,0);
    cpu->writeCpu(0xfdc2,0);
    cpu->writeCpu(0xfdc0,0x22);
    for(BYTE byte:{BYTE(0),BYTE(0),BYTE(0),BYTE(0),
                   BYTE(0),BYTE(0),BYTE(1),BYTE(0)})
        cpu->writeCpu(0xfdc1,byte);
    if(result6809(*cpu)!=0x10 || cpu->readCpu(0x2000)!=0x5c)return 1;

    // 8088 channel 2 DMA: I/O -> memory 3000h.
    bus.writeIo(0x1c,0);
    bus.writeIo(0x14,0);bus.writeIo(0x14,0x30);
    bus.writeIo(0x15,0xff);bus.writeIo(0x15,0);
    bus.writeIo(0x22,0);
    bus.writeIo(0x1b,0x46);
    bus.writeIo(0x1a,2);
    bus.writeIo(0xfdc2,0x20);
    bus.writeIo(0xfdc0,0x22);
    for(BYTE byte:{BYTE(0),BYTE(0),BYTE(0),BYTE(0),
                   BYTE(0),BYTE(0),BYTE(1),BYTE(0)})
        bus.writeIo(0xfdc1,byte);
    if(result8088(bus)!=0x10 || bus.readRam(0x3000)!=0x5c)return 2;

    // 8088 channel 2 DMA: memory 3200h -> selected HDD.
    for(unsigned i=0;i<256;++i)bus.writeMemory(0x3200+i,0xa5);
    bus.writeIo(0x1c,0);
    bus.writeIo(0x14,0);bus.writeIo(0x14,0x32);
    bus.writeIo(0x15,0xff);bus.writeIo(0x15,0);
    bus.writeIo(0x1b,0x4a);
    bus.writeIo(0x1a,2);
    bus.writeIo(0xfdc0,0x23);
    for(BYTE byte:{BYTE(0),BYTE(0),BYTE(0),BYTE(0),
                   BYTE(0),BYTE(0),BYTE(1),BYTE(0)})
        bus.writeIo(0xfdc1,byte);
    if(result8088(bus)!=0x10 || hdd->image()[0]!=0xa5)return 3;

    // A completed transfer through one CPU is visible to the other.
    cpu->writeCpu(0xfd40,0x21);
    cpu->writeCpu(0xfd41,0x00);
    cpu->writeCpu(0xfd42,0x01);
    cpu->writeCpu(0xfd43,0x00);
    cpu->writeCpu(0xfd50,0x04);
    cpu->writeCpu(0xfdc0,0x22);
    for(BYTE byte:{BYTE(0),BYTE(0),BYTE(0),BYTE(0),
                   BYTE(0),BYTE(0),BYTE(1),BYTE(0)})
        cpu->writeCpu(0xfdc1,byte);
    if(result6809(*cpu)!=0x10 || cpu->readCpu(0x2100)!=0xa5)return 4;
    std::puts("PASS: shared HDD visible via 6809 DMA and 8088 DMA");
    return 0;
}
