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
    const auto machineInfo=mon.execute("info machine",paused);
    if(machineInfo.find("Active main CPU: MC6809")==std::string::npos ||
       machineInfo.find("RAM=")==std::string::npos ||
       machineInfo.find("FDC-status=")==std::string::npos ||
       !paused)return 87;
    const auto ioInfo=mon.execute("info io FD06",paused);
    if(ioInfo.find("FD06 reads=")==std::string::npos ||
       ioInfo.find("writes=")==std::string::npos ||
       ioInfo.find("last-write=")==std::string::npos)return 88;
    if(mon.execute("info io FC06",paused).find("FD00..FDFF")==std::string::npos)return 89;
    if(mon.execute("cpu sub",paused).find("Selected: sub")==std::string::npos)return 2;
    if(mon.execute("regs",paused).find("PC=C000")==std::string::npos || !paused)return 3;
    const auto stepped=mon.execute("step",paused);
    if(stepped.find("PC=C001")==std::string::npos)return 4;
    // The step command should display the next instruction as well as regs.
    // Missing optional unidasm is acceptable; arbitrary backend errors are not.
    if(stepped.find("External disassembler failed:")!=std::string::npos) {
        if(stepped.find("unidasm not found")==std::string::npos)return 86;
    } else if(stepped.find("C001:")==std::string::npos ||
              (stepped.find("nop")==std::string::npos &&
               stepped.find("NOP")==std::string::npos))return 86;
    if(mon.execute("x c000 2",paused).find("12 12")==std::string::npos)return 5;
    const auto dis=mon.execute("dis c000 2",paused);
    // Disassembly may be unavailable when optional MAME tools are absent,
    // but an installed backend must produce valid instruction listings.
    if(dis.find("External disassembler failed:")!=std::string::npos) {
        if(dis.find("unidasm not found")==std::string::npos)return 83;
    } else if(dis.find("C000:")==std::string::npos ||
              dis.find("C001:")==std::string::npos ||
              (dis.find("nop")==std::string::npos &&
               dis.find("NOP")==std::string::npos))return 83;
    if(mon.execute("dis c000 0",paused).find("1..64")==std::string::npos)return 84;
    if(mon.execute("dis 10000 1",paused).find("out of range")==std::string::npos)return 85;
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
    // Per-CPU breakpoints are non-invasive and checked before fetch.
    if(mon.execute("b c010",paused).find("Breakpoint set")==std::string::npos)return 37;
    if(mon.execute("bl",paused).find("C010")==std::string::npos)return 38;
    if(mon.probe(DebugMonitor::Target::Sub,0xc010)!=false)return 39; // selected main6809
    if(!mon.probe(DebugMonitor::Target::Main6809,0xc010) || !mon.breakPending())return 40;
    if(mon.takeBreak().find("main6809 PC=C010")==std::string::npos)return 41;
    if(mon.execute("cont",paused).find("running")==std::string::npos)return 42;
    if(mon.probe(DebugMonitor::Target::Main6809,0xc010))return 43; // skip precisely once
    if(!mon.probe(DebugMonitor::Target::Main6809,0xc010))return 44;
    if(mon.takeBreak().empty())return 45;
    if(mon.execute("bc c010",paused).find("cleared")==std::string::npos)return 46;
    if(mon.probe(DebugMonitor::Target::Main6809,0xc010))return 47;
    if(mon.execute("cpu sub",paused).find("Selected")==std::string::npos)return 48;
    if(mon.execute("b c000",paused).find("set")==std::string::npos)return 49;
    if(!mon.probe(DebugMonitor::Target::Sub,0xc000))return 50;
    if(mon.takeBreak().find("sub PC=C000")==std::string::npos)return 51;
    if(mon.execute("bc",paused).find("cleared")==std::string::npos)return 52;
    if(mon.execute("bl",paused).find("No breakpoints")==std::string::npos)return 53;
    // Cross-CPU address spaces are independent, including the 16-bit ceiling.
    if(mon.execute("cpu main6809",paused).find("Selected")==std::string::npos)return 62;
    if(mon.execute("b ffff",paused).find("set")==std::string::npos)return 63;
    if(mon.execute("b ffff",paused).find("set")==std::string::npos)return 64; // idempotent
    if(!mon.probe(DebugMonitor::Target::Main6809,0xffff))return 65;
    if(mon.takeBreak().empty())return 66;
    if(mon.execute("bc ffff",paused).find("cleared")==std::string::npos)return 67;
    if(mon.probe(DebugMonitor::Target::Main6809,0xffff))return 68;
    if(mon.execute("b 10000",paused).find("out of range")==std::string::npos)return 69;
    if(mon.execute("cpu sub",paused).find("Selected")==std::string::npos)return 70;
    if(mon.execute("b ffff",paused).find("set")==std::string::npos)return 71;
    if(mon.probe(DebugMonitor::Target::Main6809,0xffff))return 72;
    if(!mon.probe(DebugMonitor::Target::Sub,0xffff))return 73;
    if(mon.takeBreak().empty())return 74;
    if(mon.execute("bc",paused).find("cleared")==std::string::npos)return 75;
    if(mon.execute("bl",paused).find("No breakpoints")==std::string::npos)return 76;
    if(mon.probe(DebugMonitor::Target::Sub,0xffff))return 77;
    // Selection is debugger-only; the guest CPU owner remains unchanged.
    machine.addBoard(std::make_unique<Main8088Board>(CpuType::NECV20));
    machine.addBoard(std::make_unique<MainZ80Board>());
    machine.reset();
    DebugMonitor multi(machine,[&](bool stop){paused=stop;});
    if(multi.execute("cpu main8088",paused).find("Selected")==std::string::npos)return 13;
    if(multi.execute("regs",paused).find("CS:IP=")==std::string::npos)return 14;
    if(multi.execute("x 0000:0000 1",paused).find(":")==std::string::npos)return 15;
    if(multi.execute("step",paused).find("not the active")==std::string::npos)return 16;
    if(multi.execute("b 0000:0000",paused).find("set")==std::string::npos)return 54;
    if(!multi.probe(DebugMonitor::Target::Main8088,0))return 55;
    if(multi.takeBreak().find("PC=00000")==std::string::npos)return 56;
    if(multi.execute("bc 0000:0000",paused).find("cleared")==std::string::npos)return 57;
    // The 8088/V20 physical-address bitmap covers the last byte of 1 MiB.
    if(multi.execute("b ffff:000f",paused).find("FFFFF")==std::string::npos)return 78;
    if(!multi.probe(DebugMonitor::Target::Main8088,0xfffff))return 79;
    if(multi.takeBreak().find("PC=FFFFF")==std::string::npos)return 80;
    if(multi.execute("bc ffff:000f",paused).find("cleared")==std::string::npos)return 81;
    if(multi.probe(DebugMonitor::Target::Main8088,0xfffff))return 82;
    const auto multiInfo=multi.execute("info machine",paused);
    if(multiInfo.find("PIC-mask=")==std::string::npos ||
       multiInfo.find("DMA-mask=")==std::string::npos ||
       multiInfo.find("bank=")==std::string::npos)return 90;
    if(multi.execute("cpu z80",paused).find("Selected")==std::string::npos)return 17;
    if(multi.execute("regs",paused).find("AF=")==std::string::npos)return 18;
    if(multi.execute("b 1234",paused).find("set")==std::string::npos)return 58;
    if(!multi.probe(DebugMonitor::Target::Z80,0x1234))return 59;
    if(multi.takeBreak().find("z80 PC=1234")==std::string::npos)return 60;
    if(multi.execute("bc",paused).find("cleared")==std::string::npos)return 61;

    // Watchpoints are registered by target and direction. Real bus
    // access observers must report the access without a second read.
    if(multi.execute("cpu main8088",paused).find("Selected")==std::string::npos)return 91;
    if(multi.execute("w rw 1DC00",paused).find("Watchpoint set")==std::string::npos)return 92;
    if(multi.execute("wl",paused).find("1DC00")==std::string::npos)return 93;
    auto* v20=dynamic_cast<Main8088Board*>(&machine.board(1));
    if(!v20)return 94;
    v20->bus().writeMemory(0x1dc00,0xc3);
    if(!multi.breakPending())return 95;
    const auto writeHit=multi.takeBreak();
    if(writeHit.find("main8088 write 1DC00 value=C3")==std::string::npos)return 96;
    // Debugger read/inspection is excluded from watchpoint events.
    if(multi.execute("dis 1DC00 1",paused).empty())return 115;
    if(multi.execute("x 1DC00 1",paused).find("C3")==std::string::npos)return 116;
    if(multi.breakPending())return 117;
    if(v20->bus().readMemory(0x1dc00)!=0xc3)return 97;
    if(!multi.breakPending())return 98;
    if(multi.takeBreak().find("main8088 read 1DC00 value=C3")==std::string::npos)return 99;
    if(multi.execute("wc 1DC00",paused).find("cleared")==std::string::npos)return 100;
    if(multi.execute("wl",paused).find("No watchpoints")==std::string::npos)return 101;
    v20->bus().writeMemory(0x1dc00,0x00);
    if(multi.breakPending())return 102;
    if(multi.execute("cpu z80",paused).find("Selected")==std::string::npos)return 103;
    if(multi.execute("w w 1234",paused).find("set")==std::string::npos)return 104;
    auto* z80=dynamic_cast<MainZ80Board*>(&machine.board(2));
    if(!z80)return 105;
    z80->writeMemory(0x1234,0x5a);
    if(multi.takeBreak().find("z80 write 1234 value=5A")==std::string::npos)return 106;
    if(multi.execute("wc",paused).find("cleared")==std::string::npos)return 107;
    if(multi.execute("cpu sub",paused).find("Selected")==std::string::npos)return 108;
    if(multi.execute("w w 2000",paused).find("Watchpoint set")==std::string::npos)return 109;
    machine.systemBoard().displayCpu().SetMemByte(0x2000,0x19);
    if(multi.takeBreak().find("sub write 2000 value=19")==std::string::npos)return 118;
    if(multi.execute("wc",paused).find("cleared")==std::string::npos)return 119;
    if(multi.execute("cpu main6809",paused).find("Selected")==std::string::npos)return 110;
    if(multi.execute("w w 2000",paused).find("set")==std::string::npos)return 111;
    auto* m6809=dynamic_cast<Main6809Board*>(&machine.board(0));
    if(!m6809)return 112;
    m6809->writeCpu(0x2000,0x3c);
    if(multi.takeBreak().find("main6809 write 2000 value=3C")==std::string::npos)return 113;
    if(multi.execute("wc",paused).find("cleared")==std::string::npos)return 114;

    if(machine.activeCpuType()!=CpuType::MC6809)return 19;
    machine.setDisplayCpu6309Enabled(true);
    if(multi.execute("cpu sub",paused).find("Selected")==std::string::npos)return 20;
    if(multi.execute("regs",paused).find("MD=")==std::string::npos)return 21;
    return 0;
}
