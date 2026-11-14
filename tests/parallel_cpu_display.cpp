#include "Machine.h"
#include "Main6809Board.h"
#include "WorkerGroup.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

// The three workers run independently while a frontend takes immutable
// display snapshots; no SDL or host graphics subsystem is needed.
int main() {
    using namespace std::chrono_literals;
    Machine machine;
    auto cpu=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* main=cpu.get();
    Main6809Board::BootRomImage boot{};
    boot[0x0ffe]=0x10; boot[0x0fff]=0x00;
    cpu->loadBootRom(boot);
    machine.addBoard(std::move(cpu));

    DisplaySubsystem::RomImage rom{};
    rom[0x1000]=0x86; rom[0x1001]=0x55; // LDA #$55
    rom[0x1002]=0xb7; rom[0x1003]=0x80; rom[0x1004]=0x00; // STA $8000
    rom[0x1005]=0x20; rom[0x1006]=0xf9; // BRA C000
    rom[0x4ffe]=0xc0; rom[0x4fff]=0x00;
    machine.loadDisplayRom(rom);
    machine.setDisplayCpuClockHz(2000000);
    machine.reset();
    main->setCpuClockHz(2000000);
    main->writeCpu(0x1000,0x20); main->writeCpu(0x1001,0xfe); // BRA $1000

    machine.startRealtimePtm();
    machine.startDisplayCpu();
    std::atomic<std::uint64_t> executed{0};
    WorkerGroup cpuWorker;
    cpuWorker.start([&]{
        for(unsigned i=0;i<64;++i) {
            machine.executeOne();
            executed.fetch_add(1,std::memory_order_relaxed);
        }
        std::this_thread::sleep_for(1ms);
    });
    bool displayReady=false;
    for(unsigned attempt=0;attempt<100 && !displayReady;++attempt) {
        std::this_thread::sleep_for(2ms);
        auto displayLock=machine.lockDisplay();
        displayReady=machine.displayMemory(0x8000)==0x55;
        const auto frame=machine.captureDisplayFrame(false);
        if(frame.argb.empty())return 1;
    }
    if(!displayReady || executed.load()==0)return 2;
    cpuWorker.pause();
    const auto stoppedAt=executed.load();
    std::this_thread::sleep_for(10ms);
    if(executed.load()!=stoppedAt)return 3;
    cpuWorker.resume();
    std::this_thread::sleep_for(10ms);
    if(executed.load()==stoppedAt)return 4;
    cpuWorker.stop();
    machine.stopDisplayCpu();
    machine.stopRealtimePtm();
    return 0;
}
