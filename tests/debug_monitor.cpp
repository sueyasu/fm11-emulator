#include "DebugMonitor.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
int main() {
    Machine machine;
    machine.addBoard(std::make_unique<Main6809Board>(CpuType::MC6809));
    DisplaySubsystem::RomImage rom{};
    rom[0x1000]=0x12;rom[0x1001]=0x12; // two NOPs at C000
    rom[0x4ffe]=0xc0;rom[0x4fff]=0x00;
    machine.loadDisplayRom(rom);
    machine.reset();
    bool paused=false;
    DebugMonitor mon(machine,[&](bool stop){paused=stop;});
    if(mon.execute("info cpus",paused).find("sub  MC6809")==std::string::npos)return 1;
    if(mon.execute("cpu sub",paused).find("Selected: sub")==std::string::npos)return 2;
    if(mon.execute("regs",paused).find("PC=C000")==std::string::npos || !paused)return 3;
    if(mon.execute("step",paused).find("PC=C001")==std::string::npos)return 4;
    if(mon.execute("x c000 2",paused).find("12 12")==std::string::npos)return 5;
    if(mon.execute("x afe4 1",paused).find("FF")==std::string::npos)return 6;
    if(mon.execute("cpu main6809",paused).find("Selected")==std::string::npos)return 7;
    if(mon.execute("xp 0 1",paused).find("00")==std::string::npos)return 8;
    if(mon.execute("cont",paused).find("running")==std::string::npos || paused)return 9;
    if(mon.execute("log start monitor-test.log",paused).find("started")==std::string::npos)return 10;
    mon.recordInput("regs");
    mon.emit(mon.execute("regs",paused));
    if(mon.execute("log stop",paused).find("stopped")==std::string::npos)return 11;
    std::ifstream file("monitor-test.log");
    std::string content((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    std::remove("monitor-test.log");
    if(content.find("regs")==std::string::npos || content.find("PC=")==std::string::npos)return 12;
    // Selection is debugger-only; the guest CPU owner remains unchanged.
    machine.addBoard(std::make_unique<Main8088Board>(CpuType::NECV20));
    machine.addBoard(std::make_unique<MainZ80Board>());
    machine.reset();
    DebugMonitor multi(machine,[&](bool stop){paused=stop;});
    if(multi.execute("cpu main8088",paused).find("Selected")==std::string::npos)return 13;
    if(multi.execute("regs",paused).find("CS:IP=")==std::string::npos)return 14;
    if(multi.execute("x 0000:0000 1",paused).find(":")==std::string::npos)return 15;
    if(multi.execute("step",paused).find("not the active")==std::string::npos)return 16;
    if(multi.execute("cpu z80",paused).find("Selected")==std::string::npos)return 17;
    if(multi.execute("regs",paused).find("AF=")==std::string::npos)return 18;
    if(machine.activeCpuType()!=CpuType::MC6809)return 19;
    machine.setDisplayCpu6309Enabled(true);
    if(multi.execute("cpu sub",paused).find("Selected")==std::string::npos)return 20;
    if(multi.execute("regs",paused).find("MD=")==std::string::npos)return 21;
    return 0;
}
