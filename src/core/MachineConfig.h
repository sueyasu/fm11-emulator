#pragma once
#include "CpuCore.h"
#include <cstdint>
#include <cstddef>
#include <array>
#include <string>

// Installed cards and DIP-selected startup main CPU are independent.
struct MachineConfig {
    // Installed main RAM, shared capacity setting for all CPU cards.
    // The existing default is 1024 KiB; valid sizes are 128 KiB steps.
    std::size_t ramKiB=1024;
    bool main6809Installed=true;
    CpuType main6809Cpu=CpuType::MC6809;
    std::uint64_t main6809ClockHz=2000000;
    std::string main6809Rom;
    bool main8088Installed=false;
    CpuType main8088Cpu=CpuType::I8088;
    std::uint64_t main8088ClockHz=8000000;
    std::string main8088Rom;
    enum class MainSelection { Cpu6809, Cpu8088 };
    MainSelection mainSelected=MainSelection::Cpu6809;
    bool z80Installed=false;
    std::uint64_t z80ClockHz=4000000;
    std::uint64_t displayClockHz=2000000;
    // Initial SDL viewport height, independent of guest video hardware.
    unsigned displayHeight=400;
    std::string displayRom="external/roms/display6809.bin";
    std::string ankRom;
    // Empty paths mean no medium mounted. 2D accepts raw 320 KiB or D88;
    // 2HD accepts raw 1232 KiB. Drive indices are physical 0..3.
    std::array<std::string,4> floppy2d{};
    std::array<std::string,4> floppy2hd{};
    std::string hddImage;
    bool hddTimed=false; // independent opt-in MDC timing
    bool floppyTimed=false; // opt-in FDC Type-I mechanical delay
    std::string serial0="pty"; // host endpoint for shared 8251A
    bool serialClocked=false; // opt-in virtual character timing
    bool serialRtsCts=false; // default to TX/RX/GND three-wire operation
    // FD23 boot-device DIP switches SW6..SW8 are active-low.
    // Defaults preserve the existing 0x9f hardware setting.
    bool boot2dFdd=true;
    bool boot2hdFdd=true;
    bool bootHdd=false;
    // Stage 2-A physical switch settings; not yet wired to hardware.
    unsigned dipBootDrive=0; // BootLoader SW1-2
    bool dipBootBubble=false; // SW5
    enum class SerialCd { Ignore, CdAndDsr };
    enum class SerialMode { Async, Sync };
    SerialCd dipSerialCd=SerialCd::Ignore; // Communication SW2
    SerialMode dipSerialMode=SerialMode::Async; // SW3
    unsigned dipSerialBaudSwitches=0; // SW4..8, decimal 0..31 (x=1, o=0)
    unsigned dipSystemDisplayLines=400; // System Select SW1
    std::uint8_t bootDip() const {
        return static_cast<std::uint8_t>(0x1f |
            (boot2dFdd?0:0x20) |
            (boot2hdFdd?0:0x40) |
            (bootHdd?0:0x80));
    }
};
MachineConfig loadMachineConfig(const std::string& explicitPath="");
MachineConfig parseMachineConfigFile(const std::string& path);
