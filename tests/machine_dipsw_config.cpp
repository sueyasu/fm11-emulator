#include "MachineConfig.h"
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
const char* path="fm11emu-dipsw-regression.conf";
MachineConfig parse(const std::string& settings) {
    std::ofstream file(path);
    if(!file)throw std::runtime_error("cannot write config");
    file<<"[DIPSW]\n"<<settings;
    file.close();
    return parseMachineConfigFile(path);
}
bool rejected(const std::string& settings) {
    try {(void)parse(settings);}catch(const std::runtime_error&){return true;}
    return false;
}
}
int main() {
    struct Cleanup {~Cleanup(){std::remove(path);}} cleanup;
    auto cfg=parse("boot_drive=3\nbubble=true\n"
        "2D_FDD=false\n2HD_FDD=true\nHDD=true\n"
        "serial_cd=cd_dsr\nserial_mode=sync\n"
        "serial_baud_sw4_8=26\ndisplay_lines=200\n"
        "");
    if(cfg.dipBootDrive!=3 || !cfg.dipBootBubble ||
       cfg.dipSerialCd!=MachineConfig::SerialCd::CdAndDsr ||
       cfg.dipSerialMode!=MachineConfig::SerialMode::Sync ||
       cfg.dipSerialBaudSwitches!=26 || cfg.dipSystemDisplayLines!=200 ||
       cfg.boot2dFdd || !cfg.boot2hdFdd || !cfg.bootHdd)return 1;
    // New settings must not affect existing FD23 or SDL display settings.
    if(cfg.bootDip()!=0x3f || cfg.displayHeight!=400)return 2;
    auto defaults=parse("");
    if(defaults.dipBootDrive!=0 || defaults.dipBootBubble ||
       defaults.dipSerialBaudSwitches!=0 ||
       defaults.dipSystemDisplayLines!=400 || defaults.bootDip()!=0x9f)return 3;
    for(unsigned n=0;n<32;++n) {
        if(parse("serial_baud_sw4_8="+std::to_string(n)+"\n").dipSerialBaudSwitches!=n)return 4;
    }
    const char* invalid[]={"boot_drive=-1\n","boot_drive=4\n","boot_drive=3x\n",
      "boot_sw3=true\n","boot_sw4=false\n","bubble=yes\n",
      "serial_sw1=true\n","serial_cd=dsr\n","serial_mode=other\n",
      "serial_baud_sw4_8=-1\n","serial_baud_sw4_8=32\n",
      "serial_baud_sw4_8=xxxxx\n","serial_baud_sw4_8=0x1f\n",
      "serial_baud_sw4_8=999999999999999999999\n",
      "display_lines=300\n","system_sw3=false\n","printer_sw4=true\n"};
    for(const char* s:invalid)if(!rejected(s))return 5;
    return 0;
}
