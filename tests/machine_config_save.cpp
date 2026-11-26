#include "MachineConfig.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    char folder[]="/tmp/fm11-stage4-config-XXXXXX";
    const char* root=::mkdtemp(folder);
    if(!root)return 1;
    const std::filesystem::path home(root);
    if(::setenv("HOME",root,1)!=0)return 2;
    ::unsetenv("XDG_CONFIG_HOME");
    if(userMachineConfigPath()!=(home/".config/fm11emu/fm11emu.conf").string())return 3;

    MachineConfig cfg;
    cfg.main6809Installed=true; cfg.main6809Cpu=CpuType::HD6309;
    cfg.main6809ClockHz=3000000;cfg.main6809Rom="main6809.rom";
    cfg.main8088Installed=true;cfg.main8088Cpu=CpuType::NECV20;
    cfg.main8088ClockHz=8000000;cfg.main8088Rom="main8088.rom";
    cfg.mainSelected=MachineConfig::MainSelection::Cpu8088;
    cfg.z80Installed=true;cfg.z80ClockHz=4000000;
    cfg.displayCpu=CpuType::HD6309;cfg.displayClockHz=3000000;
    cfg.displayRom="display.rom";cfg.ankRom="ank.rom";
    cfg.kanjiRom="kanji1.rom";cfg.kanji2Rom="kanji2.rom";
    cfg.ramKiB=384;cfg.displayHeight=200;
    cfg.floppy2d={"a.d88","b.d88","","d.d88"};
    cfg.floppy2hd={"hd0.d88","","hd2.d88",""};
    cfg.hddImage="hdd.raw";cfg.hddTimed=true;cfg.floppyTimed=true;
    cfg.serial0="/dev/ttyUSB0";cfg.serialClocked=false;cfg.serialRtsCts=true;
    cfg.boot2dFdd=false;cfg.boot2hdFdd=true;cfg.bootHdd=true;
    cfg.dipBootDrive=3;cfg.dipBootBubble=true;
    cfg.dipSerialCd=MachineConfig::SerialCd::CdAndDsr;
    cfg.dipSerialMode=MachineConfig::SerialMode::Sync;
    cfg.dipSerialBaudSwitches=27;cfg.dipSystemDisplayLines=200;

    saveUserMachineConfig(cfg);
    const auto local=userMachineConfigPath();
    struct stat metadata{};
    if(::stat(local.c_str(),&metadata)!=0 || (metadata.st_mode&0777)!=0600)return 4;
    auto roundtrip=loadMachineConfig();
    if(serializeMachineConfig(roundtrip)!=serializeMachineConfig(cfg))return 5;

    // Atomic overwrite, no stale values from the old version.
    cfg.ramKiB=1024;cfg.floppy2d[0].clear();cfg.hddImage.clear();
    saveUserMachineConfig(cfg);
    roundtrip=loadMachineConfig();
    if(serializeMachineConfig(roundtrip)!=serializeMachineConfig(cfg))return 6;

    // XDG_CONFIG_HOME has higher priority than HOME/.config.
    const auto xdg=home/"custom-config";
    if(::setenv("XDG_CONFIG_HOME",xdg.c_str(),1)!=0)return 7;
    if(userMachineConfigPath()!=(xdg/"fm11emu/fm11emu.conf").string())return 8;
    cfg.ramKiB=512;
    saveUserMachineConfig(cfg);
    if(loadMachineConfig().ramKiB!=512)return 9;
    if(parseMachineConfigFile(local).ramKiB!=1024)return 10;

    // An invalid relative XDG location falls back to the HOME location.
    if(::setenv("XDG_CONFIG_HOME","relative",1)!=0)return 11;
    if(userMachineConfigPath()!=local || loadMachineConfig().ramKiB!=1024)return 12;

    // Explicit --config remains highest priority, but not the save target.
    const auto explicitConfig=home/"explicit.conf";
    {std::ofstream out(explicitConfig);out<<"[memory]\nram_kib=256\n";}
    if(loadMachineConfig(explicitConfig.string()).ramKiB!=256)return 13;
    if(loadMachineConfig().ramKiB!=1024)return 14;

    // Invalid configurations are rejected before touching an existing file.
    const auto before=serializeMachineConfig(loadMachineConfig());
    cfg.main8088Installed=false;
    try {saveUserMachineConfig(cfg);return 15;}
    catch(const std::invalid_argument&) {}
    if(serializeMachineConfig(loadMachineConfig())!=before)return 16;
    cfg.main8088Installed=true;
    cfg.main6809Rom="invalid#filename.rom";
    try {saveUserMachineConfig(cfg);return 17;}
    catch(const std::invalid_argument&) {}
    if(serializeMachineConfig(loadMachineConfig())!=before)return 18;

    std::filesystem::remove_all(home);
    return 0;
}
