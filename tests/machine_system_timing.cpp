#include "Machine.h"
#include "Main8088Board.h"
#include <cstdint>
#include <memory>
#include <vector>

// Stage 1-G: one main-CPU instruction advances exactly one common
// deterministic time interval. PTM and display consume that same interval;
// an immediate 8237 DMA transfer currently consumes no emulated bus time.
static int check(CpuType type)
{
    Machine machine;
    auto cpu=std::make_unique<Main8088Board>(type);
    auto* card=cpu.get();
    Main8088Bus::Rom boot{};
    boot.fill(0x90); // NOP: three clocks on 8088 and V20
    cpu->bus().loadBootRom(boot);
    machine.addBoard(std::move(cpu));
    machine.set8088ClockHz(29100); // 3 cycles = exactly one 9700 Hz PTM tick

    DisplaySubsystem::RomImage display{};
    // LDA #$7B; STA $8000; BRA $C005
    display[0x1000]=0x86; display[0x1001]=0x7b;
    display[0x1002]=0xb7; display[0x1003]=0x80; display[0x1004]=0x00;
    display[0x1005]=0x20; display[0x1006]=0xfe;
    display[0x4ffe]=0xc0; display[0x4fff]=0x00;
    machine.loadDisplayRom(display);
    machine.setDisplayCpuClockHz(19400); // 2 sub-CPU cycles per NOP
    machine.reset();

    auto& board=machine.systemBoard();
    board.writePtmRegister(0x39,0x01); // channel 0 timer enabled
    board.writePtmRegister(0x38,0x50); // continuous, IRQ enabled
    board.writePtmRegister(0x3a,0x00);
    board.writePtmRegister(0x3b,0x05); // expire after five PTM ticks

    machine.executeOne();
    if(machine.lastExecutionCycles()!=3 ||
       board.ptmDevice()->channels[0].counter!=4 ||
       board.ptmDevice()->irqRequest())return 1;
    machine.executeOne();
    if(machine.lastExecutionCycles()!=3 ||
       board.ptmDevice()->channels[0].counter!=3 ||
       machine.displayMemory(0x8000)!=0x7b)return 2;

    // Program a real single-byte 8237 memory->I/O transfer between CPU
    // instructions. It must not advance the PTM or display scheduler a
    // second time. Bus-steal timing remains an explicit model limitation.
    std::vector<BYTE> sent;
    auto& bus=card->bus();
    bus.writeMemory(0x4000,0x5a);
    bus.setDmaDevice(0,{},[&sent](BYTE b){sent.push_back(b);});
    bus.writeIo(0x1c,0); // clear address/count flip-flop
    bus.writeIo(0x10,0x00); bus.writeIo(0x10,0x40);
    bus.writeIo(0x11,0x00); bus.writeIo(0x11,0x00);
    bus.writeIo(0x20,0);
    bus.writeIo(0x1b,0x48); // channel 0, single, memory->I/O
    bus.setDmaRequest(0,true);
    if(!bus.serviceDma() || sent.size()!=1 || sent[0]!=0x5a)return 3;
    if(board.ptmDevice()->channels[0].counter!=3 ||
       board.ptmDevice()->irqRequest())return 4;

    for(int i=0;i<2;++i) {
        machine.executeOne();
        if(machine.lastExecutionCycles()!=3)return 5;
    }
    if(board.ptmDevice()->channels[0].counter!=1 ||
       board.ptmDevice()->irqRequest())return 6;
    machine.executeOne();
    if(machine.lastExecutionCycles()!=3 ||
       !board.ptmDevice()->irqRequest())return 7;
    return 0;
}

int main()
{
    const int intel=check(CpuType::I8088);
    if(intel)return intel;
    const int nec=check(CpuType::NECV20);
    return nec?100+nec:0;
}
