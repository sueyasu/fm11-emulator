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
    // Logging is off by default, and repeated stop/start operations are safe.
    const std::string logPath="monitor-lifecycle-test.log";
    std::remove(logPath.c_str());
    if(mon.execute("log status",paused)!="Logging: off\n")return 22;
    mon.recordInput("help"); // must not create a log before start
    if(std::ifstream(logPath).good())return 23;
    if(mon.execute("log stop",paused).find("not active")==std::string::npos)return 24;
    if(mon.execute("log start "+logPath,paused).find("started")==std::string::npos)return 25;
    if(mon.execute("log status",paused).find(logPath)==std::string::npos)return 26;
    if(mon.execute("log start "+logPath,paused).find("Already logging")==std::string::npos)return 27;
    mon.recordInput("info cpus");
    mon.emit(mon.execute("info cpus",paused));
    if(mon.execute("log stop",paused).find("stopped")==std::string::npos)return 28;
    if(mon.execute("log status",paused)!="Logging: off\n")return 29;
    {
        std::ifstream logged(logPath);
        const std::string transcript((std::istreambuf_iterator<char>(logged)),
                                      std::istreambuf_iterator<char>());
        if(transcript.find("info cpus")==std::string::npos ||
           transcript.find("main6809")==std::string::npos ||
           transcript.find("Logging stopped")==std::string::npos)return 30;
    }
    if(mon.execute("log start "+logPath,paused).find("started")==std::string::npos)return 31;
    mon.recordInput("help");
    mon.emit(mon.execute("help",paused));
    if(mon.execute("log stop",paused).find("stopped")==std::string::npos)return 32;
    {
        std::ifstream logged(logPath);
        const std::string transcript((std::istreambuf_iterator<char>(logged)),
                                      std::istreambuf_iterator<char>());
        if(transcript.find("info cpus")==std::string::npos ||
           transcript.find("help")==std::string::npos)return 33;
    }
    std::remove(logPath.c_str());
    // Dump length and step count use decimal by default, address uses hex.
    if(mon.execute("x 10 10",paused).find("0010:")==std::string::npos)return 34;
    if(mon.execute("x 10 0x10",paused).find("0010:")==std::string::npos)return 35;
    if(mon.execute("step 0",paused).find("1..10000")==std::string::npos)return 36;
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
