#include "MachineConfig.h"
#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include "MainZ80Board.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

int main() {
    const std::string path="fm11emu-config-regression.conf";
    {
        std::ofstream out(path);
        if(!out)return 1;
        out<<"[main_6809]\ninstalled = true\ncpu = 6309\nclock_hz = 3000000\n"
              "[main_8088]\ninstalled = true\ncpu = v20\nclock_hz = 8000000\n"
              "[main]\nactive = 8088\n"
              "[optional_cpu]\ntype = z80\nclock_hz = 4000000\n"
              "[display_cpu]\ncpu = 6809\nclock_hz = 2000000\n";
    }
    auto config=parseMachineConfigFile(path);
    if(!config.main6809Installed || !config.main8088Installed ||
       config.main6809Cpu!=CpuType::HD6309 ||
       config.main8088Cpu!=CpuType::NECV20 ||
       config.mainSelected!=MachineConfig::MainSelection::Cpu8088 ||
       !config.z80Installed || config.main6809ClockHz!=3000000 ||
       config.main8088ClockHz!=8000000)return 2;

    Machine machine;
    auto m6809=std::make_unique<Main6809Board>(config.main6809Cpu);
    m6809->setCpuClockHz(config.main6809ClockHz);
    auto* board6809=m6809.get();
    const auto a=machine.addBoard(std::move(m6809));
    const auto b=machine.addBoard(std::make_unique<Main8088Board>(config.main8088Cpu));
    const auto c=machine.addBoard(std::make_unique<MainZ80Board>());
    machine.set8088ClockHz(config.main8088ClockHz);
    machine.setZ80ClockHz(config.z80ClockHz);
    machine.setActiveBoard(b); // DIP-selected 8088/V20.
    machine.configureFd05Boards(b,c);
    machine.enableFd05Selection(true);
    machine.reset();
    if(machine.activeCpuType()!=CpuType::NECV20 || machine.boardCount()!=3)return 3;
    // FD05 selects Z80, not the otherwise installed 6809 card.
    board6809->writeCpu(0xfd05,0x01);
    if(machine.activeCpuType()!=CpuType::Z80)return 4;
    board6809->writeCpu(0xfd05,0x00);
    if(machine.activeCpuType()!=CpuType::NECV20)return 5;
    // DIP-selected 6809 uses the same hardware with no board removal.
    machine.configureFd05Boards(a,c);
    machine.reset();
    if(machine.activeCpuType()!=CpuType::HD6309)return 6;
    board6809->writeCpu(0xfd05,0x01);
    if(machine.activeCpuType()!=CpuType::Z80)return 7;
    board6809->writeCpu(0xfd05,0x00);
    if(machine.activeCpuType()!=CpuType::HD6309)return 8;
    std::remove(path.c_str());
    // Explicitly reject a DIP selection of a card that is not fitted.
    {
        std::ofstream out(path);
        out<<"[main]\nactive=8088\n";
    }
    try { (void)parseMachineConfigFile(path);return 9; }
    catch(const std::runtime_error&) {}
    std::remove(path.c_str());
    return 0;
}
