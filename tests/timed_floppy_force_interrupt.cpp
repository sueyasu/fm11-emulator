#include "FloppyDevice.h"
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <memory>
#include <vector>

// Force Interrupt aborts timed operations and never raises a late IRQ.
static int checkShared(unsigned ch) {
    FloppyDevice f;
    if(ch)f.load2hd(std::vector<BYTE>(77u*2*26*256,0xe5));
    else f.load2d(std::vector<BYTE>(40u*2*16*256,0xe5));
    f.enableSeekTiming(true);
    if(!f.beginTypeII(ch,0,0,0,1,0x80) || !f.typeIIBusy(ch,0))return 1;
    if(f.forceInterrupt(ch,0,0xd0) || f.typeIIBusy(ch,0) ||
       f.forceInterruptPending(ch))return 2;
    f.advanceSeekNanoseconds(5*f.revolutionNanoseconds(ch));
    if(f.typeIIReady() || f.forceInterruptPending(ch))return 3;
    if(!f.forceInterrupt(ch,0,0xd8) ||
       f.forceInterruptPending(ch) || f.takeForceInterrupt(ch))return 4;
    // D4 waits for the NEXT index; the existing index pulse does not fire.
    if(f.forceInterrupt(ch,0,0xd4) || f.forceInterruptPending(ch))return 5;
    const auto rev=f.revolutionNanoseconds(ch);
    f.advanceSeekNanoseconds(rev-1);
    if(f.forceInterruptPending(ch))return 6;
    f.advanceSeekNanoseconds(1);
    if(!f.takeForceInterrupt(ch) || f.takeForceInterrupt(ch))return 7;
    f.forceInterrupt(ch,0,0xd4);
    f.cancelForceInterrupt(ch);
    f.advanceSeekNanoseconds(rev);
    if(f.forceInterruptPending(ch))return 8;
    return 0;
}
static int check6809(unsigned ch) {
    Machine machine;
    auto board=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* cpu=board.get();
    machine.addBoard(std::move(board));
    auto f=machine.systemBoard().floppyDevice();
    if(ch)f->load2hd(std::vector<BYTE>(77u*2*26*256,0xe5));
    else f->load2d(std::vector<BYTE>(40u*2*16*256,0xe5));
    machine.reset();
    f->enableSeekTiming(true);
    const WORD cmd=ch?0xfd30:0xfd18;
    const WORD track=ch?0xfd31:0xfd19;
    const WORD irq=ch?0xfd37:0xfd1f;
    cpu->writeCpu(ch?0xfd35:0xfd1d,0);
    cpu->writeCpu(track,0);
    cpu->writeCpu(ch?0xfd32:0xfd1a,1);
    cpu->writeCpu(cmd,0x80);
    if(!f->typeIIBusy(ch,0))return 1;
    cpu->writeCpu(cmd,0xd0);
    if(f->typeIIBusy(ch,0) || cpu->readCpu(cmd)!=0 ||
       cpu->readCpu(irq)!=0)return 2;
    machine.advanceTimeNanoseconds(f->revolutionNanoseconds(ch));
    if(cpu->readCpu(irq)!=0)return 3;
    cpu->writeCpu(cmd,0xd8);
    if(cpu->readCpu(irq)!=0x40 || f->forceInterruptPending(ch))return 4;
    cpu->writeCpu(cmd,0xd4);
    if(cpu->readCpu(irq)!=0)return 5;
    machine.advanceTimeNanoseconds(f->revolutionNanoseconds(ch));
    if(cpu->readCpu(irq)!=0x40)return 6;
    cpu->writeCpu(track,1);
    cpu->writeCpu(cmd,0xf0);
    if(cpu->readCpu(cmd)!=0x20 || f->trackBusy(ch,0))return 7;
    return 0;
}
static int check8088(unsigned ch) {
    Machine machine;
    auto board=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* cpu=board.get();
    machine.addBoard(std::move(board));
    auto f=machine.systemBoard().floppyDevice();
    if(ch)f->load2hd(std::vector<BYTE>(77u*2*26*256,0xe5));
    else f->load2d(std::vector<BYTE>(40u*2*16*256,0xe5));
    machine.reset();
    f->enableSeekTiming(true);
    auto& bus=cpu->bus();
    const WORD cmd=ch?0xfd30:0xfd18;
    const WORD irq=ch?0xfd37:0xfd1f;
    bus.writeIo(ch?0xfd35:0xfd1d,0);
    bus.writeIo(ch?0xfd31:0xfd19,0);
    bus.writeIo(ch?0xfd32:0xfd1a,1);
    bus.writeIo(cmd,0x80);
    if(!f->typeIIBusy(ch,0))return 1;
    bus.writeIo(cmd,0xd0);
    if(f->typeIIBusy(ch,0) || bus.readIo(cmd)!=0 ||
       bus.readIo(irq)!=0)return 2;
    machine.advanceTimeNanoseconds(f->revolutionNanoseconds(ch));
    if(bus.readIo(irq)!=0)return 3;
    bus.writeIo(cmd,0xd8);
    if(bus.readIo(irq)!=0x40 || f->forceInterruptPending(ch))return 4;
    bus.writeIo(cmd,0xd4);
    if(bus.readIo(irq)!=0)return 5;
    machine.advanceTimeNanoseconds(f->revolutionNanoseconds(ch));
    if(bus.readIo(irq)!=0x40)return 6;
    return 0;
}
int main() {
    for(unsigned ch=0;ch<2;++ch) {
        if(int r=checkShared(ch))return 10+ch*10+r;
        if(int r=check6809(ch))return 40+ch*10+r;
        if(int r=check8088(ch))return 70+ch*10+r;
    }
    FloppyDevice f;
    f.enableSeekTiming(true);
    f.forceInterrupt(0,0,0xd1);
    f.load2d(std::vector<BYTE>(40u*2*16*256,0xe5));
    f.advanceSeekNanoseconds(1);
    if(!f.takeForceInterrupt(0))return 100;
    return 0;
}
