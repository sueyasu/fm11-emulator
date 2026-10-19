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
    return 0;
}
