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
    auto cfg=parse("boot_drive=3\nboot_sw3=x\nboot_sw4=x\nbubble=true\n"
        "2D_FDD=false\n2HD_FDD=true\nHDD=true\n"
        "serial_sw1=x\nserial_cd=cd_dsr\nserial_mode=sync\n"
        "serial_baud_sw4_8=xxoxo\ndisplay_lines=200\n"
        "system_sw3=x\nprinter_sw4=o\n");
    if(cfg.dipBootDrive!=3 || cfg.dipBootSw3!='x' || cfg.dipBootSw4!='x' ||
       !cfg.dipBootBubble || cfg.dipCommSw1!='x' ||
       cfg.dipSerialCd!=MachineConfig::SerialCd::CdAndDsr ||
       cfg.dipSerialMode!=MachineConfig::SerialMode::Sync ||
       cfg.dipSerialBaudSwitches!="xxoxo" || cfg.dipSystemDisplayLines!=200 ||
       cfg.dipSystemSw3!='x' || cfg.dipPrinterSw4!='o' ||
       cfg.boot2dFdd || !cfg.boot2hdFdd || !cfg.bootHdd)return 1;
    // New settings must not affect existing FD23 or SDL display settings.
    if(cfg.bootDip()!=0x3f || cfg.displayHeight!=400)return 2;
    auto defaults=parse("");
    if(defaults.dipBootDrive!=0 || defaults.dipBootBubble ||
       defaults.dipSerialBaudSwitches!="ooooo" ||
       defaults.dipSystemDisplayLines!=400 || defaults.bootDip()!=0x9f)return 3;
    for(unsigned n=0;n<32;++n) {
        std::string switches;
        for(int bit=4;bit>=0;--bit)switches.push_back((n&(1u<<bit))?'x':'o');
        if(parse("serial_baud_sw4_8="+switches+"\n").dipSerialBaudSwitches!=switches)return 4;
    }
    const char* invalid[]={"boot_drive=-1\n","boot_drive=4\n","boot_drive=3x\n",
      "boot_sw3=on\n","boot_sw4=\n","bubble=yes\n","serial_sw1=z\n",
      "serial_cd=dsr\n","serial_mode=other\n","serial_baud_sw4_8=xxxx\n",
      "serial_baud_sw4_8=xxxxxx\n","serial_baud_sw4_8=xxqxo\n",
      "display_lines=300\n","system_sw3=1\n","printer_sw4=true\n"};
    for(const char* s:invalid)if(!rejected(s))return 5;
    return 0;
}
