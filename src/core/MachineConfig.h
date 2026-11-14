#pragma once
#include "CpuCore.h"
#include <cstdint>
#include <string>

// Installed cards and DIP-selected startup main CPU are independent.
struct MachineConfig {
    bool main6809Installed=true;
    CpuType main6809Cpu=CpuType::MC6809;
    std::uint64_t main6809ClockHz=2000000;
    bool main8088Installed=false;
    CpuType main8088Cpu=CpuType::I8088;
    std::uint64_t main8088ClockHz=8000000;
    enum class MainSelection { Cpu6809, Cpu8088 };
    MainSelection mainSelected=MainSelection::Cpu6809;
    bool z80Installed=false;
    std::uint64_t z80ClockHz=4000000;
    std::uint64_t displayClockHz=2000000;
};
MachineConfig loadMachineConfig(const std::string& explicitPath="");
MachineConfig parseMachineConfigFile(const std::string& path);
