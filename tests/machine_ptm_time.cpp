#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include <cstdint>
#include <memory>

int main()
{
    Machine machine;
    auto cpu09=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* board09=cpu09.get();
    auto cpu86=std::make_unique<Main8088Board>(CpuType::I8088);
    auto* board86=cpu86.get();
    machine.addBoard(std::move(cpu09));
    machine.addBoard(std::move(cpu86));
    machine.setActiveBoard(1);
    board09->writeCpu(0xfd02,0x10); // PTM main IRQ enable
    board09->writeCpu(0xfd39,0x01);
    board09->writeCpu(0xfd38,0x50); // channel 1, continuous with IRQ
    board09->writeCpu(0xfd3a,0);
    board09->writeCpu(0xfd3b,4);
    machine.advanceTimeNanoseconds(309279); // 3 timer ticks
    if(board09->irqPending())return 1;
    machine.advanceTimeNanoseconds(103093); // one additional timer tick
    if(!board09->irqPending())return 2;
    board86->bus().refreshSystemIrq();
    if(!board86->bus().interruptPending() ||
       board86->bus().acknowledgeInterrupt()!=8)return 3;
    board86->bus().writeIo(0,0x20);
    (void)board09->readCpu(0xfd39);
    (void)board09->readCpu(0xfd3a);
    machine.advanceTimeNanoseconds(0);
    if(board09->irqPending())return 4;

    // One large slice and many short slices must advance equal timer ticks.
    Machine a,b;
    auto pa=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto pb=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* ca=pa.get();
    auto* cb=pb.get();
    a.addBoard(std::move(pa));
    b.addBoard(std::move(pb));
    for(auto* card:{ca,cb}) {
        card->writeCpu(0xfd39,0x01);
        card->writeCpu(0xfd38,0x50);
        card->writeCpu(0xfd3a,0x07);
        card->writeCpu(0xfd3b,0xd0); // 2000 ticks
    }
    a.advanceTimeNanoseconds(100000000);
    for(int i=0;i<100;++i)b.advanceTimeNanoseconds(1000000);
    if(ca->readCpu(0xfd3a)!=cb->readCpu(0xfd3a))return 5;
    if(ca->readCpu(0xfd3b)!=cb->readCpu(0xfd3b))return 6;
    // Alternating CPU frequencies must not discard PTM sub-tick phase.
    // At 19.4 kHz each cycle is half of one PTM tick. An intervening
    // 9.7 kHz CPU cycle adds one full tick without clearing that half.
    MainBoard mixed;
    auto ptm=mixed.ptmDevice();
    ptm->write(0x39,0x01);
    ptm->write(0x38,0x50);
    ptm->write(0x3a,0);
    ptm->write(0x3b,4);
    mixed.advanceCpuCycles(1,19400); // 0.5 tick
    mixed.advanceCpuCycles(1,9700);  // 1.5 ticks total
    mixed.advanceCpuCycles(1,19400); // 2 ticks total
    if(ptm->channels[0].counter!=2)return 7;
    mixed.advanceCpuCycles(1,19400); // 2.5 ticks
    mixed.advanceCpuCycles(1,19400); // 3 ticks
    if(ptm->channels[0].counter!=1)return 8;
    mixed.advanceCpuCycles(1,9700);  // 4 ticks; expiry
    if(!ptm->irqRequest())return 9;
    // Explicit elapsed time must share the same fractional phase.
    MainBoard combined;
    auto device=combined.ptmDevice();
    device->write(0x39,0x01);
    device->write(0x38,0x50);
    device->write(0x3a,0);
    device->write(0x3b,2);
    combined.advanceCpuCycles(1,19400); // half a tick
    combined.advanceTimeNanoseconds(51547); // nearly half a tick
    combined.advanceCpuCycles(1,19400); // now more than one tick
    if(device->channels[0].counter!=1)return 10;
    return 0;
}
