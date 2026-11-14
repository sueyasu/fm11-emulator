#include "Machine.h"
#include "Main6809Board.h"
#include <chrono>
#include <memory>
#include <thread>

// Exercise the same real-time display worker as SDL, including FD05
// handshake, pause/resume and reset, without needing an SDL window.
int main() {
    using namespace std::chrono_literals;
    Machine machine;
    auto card=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* main=card.get();
    machine.addBoard(std::move(card));
    DisplaySubsystem::RomImage rom{};
    // C000: LDA #$7B; STA $8000; INC $8001; BRA $C005
    rom[0x1000]=0x86;rom[0x1001]=0x7b;
    rom[0x1002]=0xb7;rom[0x1003]=0x80;rom[0x1004]=0x00;
    rom[0x1005]=0x7c;rom[0x1006]=0x80;rom[0x1007]=0x01;
    rom[0x1008]=0x20;rom[0x1009]=0xfb;
    rom[0x4ffe]=0xc0;rom[0x4fff]=0x00;
    machine.loadDisplayRom(rom);
    machine.setDisplayCpuClockHz(2000000);
    if(machine.displayCpuRealtimeClockHz()!=2000000)return 8;
    machine.setDisplayCpuSpeedMultiplier(2);
    if(machine.displayCpuRealtimeClockHz()!=4000000)return 9;
    machine.setDisplayCpuSpeedMultiplier(4);
    if(machine.displayCpuRealtimeClockHz()!=8000000)return 10;
    machine.setDisplayCpuSpeedMultiplier(5);
    if(machine.displayCpuRealtimeClockHz()!=8000000)return 14;
    machine.setDisplayCpuSpeedMultiplier(10);
    if(machine.displayCpuRealtimeClockHz()!=8000000)return 11;
    machine.setDisplayCpuSpeedMultiplier(20);
    if(machine.displayCpuRealtimeClockHz()!=8000000)return 12;
    machine.setDisplayCpuSpeedMultiplier(0);
    if(machine.displayCpuRealtimeClockHz()!=8000000)return 13;
    // Exercise worker startup, pause/resume and HALT at the capped clock,
    // leaving the deterministic clock unchanged.
    machine.reset();
    machine.startDisplayCpu();
    if(!machine.displayCpuThreadEnabled())return 1;
    // No Machine::executeOne() or advanceTimeNanoseconds() is called.
    bool progressed=false;
    for(int i=0;i<100;++i) {
        std::this_thread::sleep_for(2ms);
        auto guard=machine.lockDisplay();
        if(machine.displayMemory(0x8000)==0x7b) {progressed=true;break;}
    }
    if(!progressed)return 2;
    // FD05 must stop only the display CPU even while the worker is active.
    main->writeCpu(0xfd05,0x80);
    std::uint16_t haltedPc=0;
    {
        auto guard=machine.lockDisplay();
        haltedPc=machine.displayCpuPc();
    }
    std::this_thread::sleep_for(20ms);
    {
        auto guard=machine.lockDisplay();
        if(machine.displayCpuPc()!=haltedPc)return 3;
    }
    main->writeCpu(0xfd05,0x00);
    machine.pauseDisplayCpu();
    std::uint16_t pausedPc=0;
    {
        auto guard=machine.lockDisplay();
        pausedPc=machine.displayCpuPc();
    }
    std::this_thread::sleep_for(10ms);
    {
        auto guard=machine.lockDisplay();
        if(machine.displayCpuPc()!=pausedPc)return 4;
    }
    machine.resumeDisplayCpu();
    machine.reset();
    if(!machine.displayCpuThreadEnabled())return 5;
    machine.stopDisplayCpu();
    if(machine.displayCpuThreadEnabled())return 6;
    // The deterministic scheduler still works at 2 MHz even after
    // running the display worker with a capped cycle budget.
    machine.reset();
    machine.advanceTimeNanoseconds(10000);
    if(machine.displayMemory(0x8000)!=0x7b)return 7;
    return 0;
}
