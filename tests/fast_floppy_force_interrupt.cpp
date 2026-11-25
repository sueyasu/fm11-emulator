#include "FloppyDevice.h"
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>
#include <vector>

static int check6809(unsigned ch) {
    Machine machine;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    machine.addBoard(std::move(board));
    auto f=machine.systemBoard().floppyDevice();
    if(ch)f->load2hd(std::vector<BYTE>(77u*2*26*256,0x5a));
    else f->load2d(std::vector<BYTE>(40u*2*16*256,0x5a));
    machine.reset();
    const WORD cmd=ch?0xfd30:0xfd18,irq=ch?0xfd37:0xfd1f;
    cpu->writeCpu(ch?0xfd35:0xfd1d,0);
    cpu->writeCpu(ch?0xfd31:0xfd19,0);
    cpu->writeCpu(ch?0xfd32:0xfd1a,1);
    cpu->writeCpu(cmd,0xd0);
    if(cpu->readCpu(cmd)!=0 || cpu->readCpu(irq)!=0)return 1;
    cpu->writeCpu(cmd,0xd8);
    if(cpu->readCpu(irq)!=0x40 || cpu->readCpu(cmd)!=0)return 2;
    cpu->writeCpu(cmd,0xd4);
    if(cpu->readCpu(irq)!=0)return 3;
    for(int n=0;n<255;++n)cpu->readCpu(cmd);
    if(cpu->readCpu(irq)!=0)return 4;
    cpu->readCpu(cmd);
    if(cpu->readCpu(irq)!=0x40)return 5;
    cpu->writeCpu(cmd,0xd4);
    cpu->writeCpu(cmd,0xd0);
    for(int n=0;n<300;++n)cpu->readCpu(cmd);
    if(cpu->readCpu(irq)!=0)return 6;
    return 0;
}
static int check8088(unsigned ch) {
    Machine machine;
    auto board=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=board.get();
    machine.addBoard(std::move(board));
    auto f=machine.systemBoard().floppyDevice();
    if(ch)f->load2hd(std::vector<BYTE>(77u*2*26*256,0x5a));
    else f->load2d(std::vector<BYTE>(40u*2*16*256,0x5a));
    machine.reset();
    auto& bus=cpu->bus();
    const WORD cmd=ch?0xfd30:0xfd18,irq=ch?0xfd37:0xfd1f;
    bus.writeIo(ch?0xfd35:0xfd1d,0);
    bus.writeIo(ch?0xfd31:0xfd19,0);
    bus.writeIo(ch?0xfd32:0xfd1a,1);
    bus.writeIo(cmd,0xd0);
    if(bus.readIo(cmd)!=0 || bus.readIo(irq)!=0)return 1;
    bus.writeIo(cmd,0xd8);
    if(bus.readIo(irq)!=0x40 || bus.readIo(cmd)!=0)return 2;
    bus.writeIo(cmd,0xd4);
    if(bus.readIo(irq)!=0)return 3;
    for(int n=0;n<255;++n)bus.readIo(cmd);
    if(bus.readIo(irq)!=0)return 4;
    bus.readIo(cmd);
    if(bus.readIo(irq)!=0x40)return 5;
    bus.writeIo(cmd,0xd4);
    bus.writeIo(cmd,0xd0);
    for(int n=0;n<300;++n)bus.readIo(cmd);
    if(bus.readIo(irq)!=0)return 6;
    return 0;
}

static int checkFast8088BootRomLatch(unsigned ch) {
    Machine machine;
    auto board=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=board.get();
    machine.addBoard(std::move(board));
    auto f=machine.systemBoard().floppyDevice();
    if(ch)f->load2hd(std::vector<BYTE>(77u*2*26*256,0x5a));
    else f->load2d(std::vector<BYTE>(40u*2*16*256,0x5a));
    machine.reset();
    auto& bus=cpu->bus();
    if(f->seekTimingEnabled())return 1;
    const WORD cmd=ch?0xfd30:0xfd18, irq=ch?0xfd37:0xfd1f;
    const WORD track=ch?0xfd31:0xfd19, data=ch?0xfd33:0xfd1b;
    bus.writeIo(ch?0xfd35:0xfd1d,0);
    bus.writeIo(cmd,0x08); // ROM RESTORE: fast completion must latch INTRQ
    if((bus.readIo(irq)&0x40)==0)return 2;
    bus.writeIo(data,1);
    bus.writeIo(cmd,0x18); // ROM SEEK: same completion contract
    if((bus.readIo(irq)&0x40)==0 || bus.readIo(track)!=1)return 3;
    bus.writeIo(cmd,0x80); // Type-II replaces previous Type-I IRQ
    if((bus.readIo(irq)&0x40)==0)return 4;
    bus.writeIo(cmd,0xd0);
    if((bus.readIo(irq)&0x40)!=0)return 5;
    return 0;
}

int main() {
    for(unsigned ch=0;ch<2;++ch) {
        if(int r=check6809(ch))return 10+ch*10+r;
        if(int r=check8088(ch))return 40+ch*10+r;
        if(int r=checkFast8088BootRomLatch(ch))return 60+ch*10+r;
    }
    FloppyDevice device;
    device.forceInterrupt(0,0,0xd1);
    device.load2d(std::vector<BYTE>(40u*2*16*256,0xe5));
    if(!device.pollFastForceInterrupt(0) ||
       !device.takeForceInterrupt(0))return 80;
    return 0;
}
