#include "MachineConfig.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <stdexcept>
#include <string>

namespace {
std::string trim(const std::string& s) {
    const auto valid=[](unsigned char c){return !std::isspace(c);};
    const auto first=std::find_if(s.begin(),s.end(),valid);
    if(first==s.end())return {};
    return std::string(first,std::find_if(s.rbegin(),s.rend(),valid).base());
}
bool boolean(const std::string& s) {
    if(s=="true")return true;
    if(s=="false")return false;
    throw std::invalid_argument("expected true or false");
}
std::uint64_t clockHz(const std::string& s) {
    if(s.empty() || !std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isdigit(c);}))
        throw std::invalid_argument("clock_hz must be decimal digits");
    std::size_t end=0;
    const auto hz=std::stoull(s,&end,10);
    if(end!=s.size() || hz==0 || hz>1000000000ULL)
        throw std::invalid_argument("clock_hz must be 1..1000000000");
    return hz;
}
std::size_t ramKiB(const std::string& value) {
    if(value.empty() || !std::all_of(value.begin(),value.end(),
          [](unsigned char c){return std::isdigit(c);}))
        throw std::invalid_argument("memory.ram_kib must be a decimal integer");
    std::size_t consumed=0;
    const auto kib=std::stoull(value,&consumed,10);
    if(consumed!=value.size() || kib<128 || kib>1024 || kib%128!=0)
        throw std::invalid_argument("memory.ram_kib must be 128..1024 in steps of 128");
    return static_cast<std::size_t>(kib);
}
unsigned dipDrive(const std::string& value) {
    if(value.size()!=1 || value[0]<'0' || value[0]>'3')
        throw std::invalid_argument("DIPSW.boot_drive must be 0..3");
    return unsigned(value[0]-'0');
}
unsigned dipBaud(const std::string& value) {
    if(value.empty() || value.size()>2 ||
       !std::all_of(value.begin(),value.end(),[](unsigned char ch){return std::isdigit(ch);}))
        throw std::invalid_argument("DIPSW.serial_baud_sw4_8 must be decimal 0..31");
    const auto number=std::stoul(value);
    if(number>31)
        throw std::invalid_argument("DIPSW.serial_baud_sw4_8 must be decimal 0..31");
    return static_cast<unsigned>(number);
}
bool exists(const std::string& path) {
    std::ifstream stream(path);
    return stream.good();
}
}
MachineConfig parseMachineConfigFile(const std::string& path) {
    std::ifstream stream(path);
    if(!stream)throw std::runtime_error("cannot open config: "+path);
    MachineConfig cfg;
    std::string section,line;
    unsigned number=0;
    bool legacyMain=false,legacyBoot=false,dipswMain=false,dipswBoot=false;
    while(std::getline(stream,line)) {
        ++number;
        const auto comment=line.find_first_of("#;");
        if(comment!=std::string::npos)line.erase(comment);
        line=trim(line);
        if(line.empty())continue;
        try {
            if(line.front()=='[' && line.back()==']') {
                section=trim(line.substr(1,line.size()-2));
                if(section!="main" && section!="main_6809" &&
                   section!="main_8088" && section!="optional_cpu" &&
                   section!="display_cpu" && section!="floppy_2d" &&
                   section!="floppy_2hd" && section!="hdd" &&
                   section!="boot" && section!="DIPSW" &&
                   section!="display" && section!="memory" && section!="serial" && section!="floppy")
                    throw std::invalid_argument("unknown section ["+section+"]");
                continue;
            }
            const auto equals=line.find('=');
            if(equals==std::string::npos || section.empty())
                throw std::invalid_argument("expected section and key=value");
            const auto key=trim(line.substr(0,equals));
            const auto value=trim(line.substr(equals+1));
            bool handled=true;
            if(section=="DIPSW") {
                if(key=="main_cpu") {
                    if(legacyMain)throw std::invalid_argument("cannot mix [DIPSW].main_cpu with [main].active");
                    dipswMain=true;
                    if(value=="6809")cfg.mainSelected=MachineConfig::MainSelection::Cpu6809;
                    else if(value=="8088")cfg.mainSelected=MachineConfig::MainSelection::Cpu8088;
                    else throw std::invalid_argument("DIPSW.main_cpu must be 6809 or 8088");
                } else if(key=="2D_FDD" || key=="2HD_FDD" || key=="HDD") {
                    if(legacyBoot)throw std::invalid_argument("cannot mix [DIPSW] boot switches with [boot]");
                    dipswBoot=true;
                    if(key=="2D_FDD")cfg.boot2dFdd=boolean(value);
                    else if(key=="2HD_FDD")cfg.boot2hdFdd=boolean(value);
                    else cfg.bootHdd=boolean(value);
                } else if(key=="boot_drive")cfg.dipBootDrive=dipDrive(value);
                else if(key=="bubble")cfg.dipBootBubble=boolean(value);
                else if(key=="serial_cd") {
                    if(value=="ignore")cfg.dipSerialCd=MachineConfig::SerialCd::Ignore;
                    else if(value=="cd_dsr")cfg.dipSerialCd=MachineConfig::SerialCd::CdAndDsr;
                    else throw std::invalid_argument("serial_cd must be ignore or cd_dsr");
                } else if(key=="serial_mode") {
                    if(value=="async")cfg.dipSerialMode=MachineConfig::SerialMode::Async;
                    else if(value=="sync")cfg.dipSerialMode=MachineConfig::SerialMode::Sync;
                    else throw std::invalid_argument("serial_mode must be async or sync");
                } else if(key=="serial_baud_sw4_8")cfg.dipSerialBaudSwitches=dipBaud(value);
                else if(key=="display_lines") {
                    if(value=="200")cfg.dipSystemDisplayLines=200;
                    else if(value=="400")cfg.dipSystemDisplayLines=400;
                    else throw std::invalid_argument("display_lines must be 200 or 400");
                } else handled=false;
            } else if(section=="main") {
                if(dipswMain)throw std::invalid_argument("cannot mix [main].active with [DIPSW].main_cpu");
                legacyMain=true;
                if(key!="active")handled=false;
                else if(value=="6809")cfg.mainSelected=MachineConfig::MainSelection::Cpu6809;
                else if(value=="8088")cfg.mainSelected=MachineConfig::MainSelection::Cpu8088;
                else throw std::invalid_argument("main.active must be 6809 or 8088");
            } else if(section=="main_6809") {
                if(key=="installed")cfg.main6809Installed=boolean(value);
                else if(key=="cpu") {
                    if(value=="6809")cfg.main6809Cpu=CpuType::MC6809;
                    else if(value=="6309")cfg.main6809Cpu=CpuType::HD6309;
                    else throw std::invalid_argument("main_6809.cpu must be 6809 or 6309");
                } else if(key=="clock_hz")cfg.main6809ClockHz=clockHz(value);
                else if(key=="rom")cfg.main6809Rom=value;
                else handled=false;
            } else if(section=="main_8088") {
                if(key=="installed")cfg.main8088Installed=boolean(value);
                else if(key=="cpu") {
                    if(value=="8088")cfg.main8088Cpu=CpuType::I8088;
                    else if(value=="v20")cfg.main8088Cpu=CpuType::NECV20;
                    else throw std::invalid_argument("main_8088.cpu must be 8088 or v20");
                } else if(key=="clock_hz")cfg.main8088ClockHz=clockHz(value);
                else if(key=="rom")cfg.main8088Rom=value;
                else handled=false;
            } else if(section=="optional_cpu") {
                if(key=="type") {
                    if(value=="none")cfg.z80Installed=false;
                    else if(value=="z80")cfg.z80Installed=true;
                    else throw std::invalid_argument("optional_cpu.type must be none or z80");
                } else if(key=="clock_hz")cfg.z80ClockHz=clockHz(value);
                else handled=false;
            } else if(section=="floppy_2d" || section=="floppy_2hd") {
                // Each section maps a physical drive index to a host image.
                if(key.size()!=6 || key.substr(0,5)!="drive" ||
                   key[5]<'0' || key[5]>'3')
                    throw std::invalid_argument("floppy key must be drive0..drive3");
                auto& slots=section=="floppy_2d"?cfg.floppy2d:cfg.floppy2hd;
                slots[key[5]-'0']=value;
            } else if(section=="boot") {
                if(dipswBoot)throw std::invalid_argument("cannot mix [boot] with [DIPSW] boot switches");
                legacyBoot=true;
                if(key=="2D_FDD")cfg.boot2dFdd=boolean(value);
                else if(key=="2HD_FDD")cfg.boot2hdFdd=boolean(value);
                else if(key=="HDD")cfg.bootHdd=boolean(value);
                else handled=false;
            } else if(section=="floppy") {
                if(key=="timing") {
                    if(value=="fast")cfg.floppyTimed=false;
                    else if(value=="timed")cfg.floppyTimed=true;
                    else throw std::invalid_argument("floppy.timing must be fast or timed");
                } else handled=false;
            } else if(section=="serial") {
                if(key=="flow_control") {
                    if(value=="none")cfg.serialRtsCts=false;
                    else if(value=="rtscts")cfg.serialRtsCts=true;
                    else throw std::invalid_argument(
                        "serial.flow_control must be none or rtscts");
                } else if(key=="timing") {
                    if(value=="immediate")cfg.serialClocked=false;
                    else if(value=="clocked")cfg.serialClocked=true;
                    else throw std::invalid_argument(
                        "serial.timing must be immediate or clocked");
                } else if(key!="serial0")handled=false;
                else if(value=="pty" || value=="none" ||
                        (value.size()>5 && value.compare(0,5,"/dev/")==0))
                    cfg.serial0=value;
                else throw std::invalid_argument(
                    "serial.serial0 must be pty, none or an absolute /dev/ path");
            } else if(section=="hdd") {
                if(key=="image")cfg.hddImage=value;
                else if(key=="timing") {
                    if(value=="fast")cfg.hddTimed=false;
                    else if(value=="timed")cfg.hddTimed=true;
                    else throw std::invalid_argument("hdd.timing must be fast or timed");
                } else handled=false;
            } else if(section=="memory") {
                if(key=="ram_kib")cfg.ramKiB=ramKiB(value);
                else handled=false;
            } else if(section=="display") {
                if(key=="height") {
                    if(value=="200")cfg.displayHeight=200;
                    else if(value=="400")cfg.displayHeight=400;
                    else throw std::invalid_argument("display.height must be 200 or 400");
                } else handled=false;
            } else if(section=="display_cpu") {
                if(key=="cpu") {
                    if(value=="6809")cfg.displayCpu=CpuType::MC6809;
                    else if(value=="6309")cfg.displayCpu=CpuType::HD6309;
                    else throw std::invalid_argument("display_cpu.cpu must be 6809 or 6309");
                } else if(key=="clock_hz")cfg.displayClockHz=clockHz(value);
                else if(key=="rom")cfg.displayRom=value;
                else if(key=="ank_rom")cfg.ankRom=value;
                else if(key=="kanji_rom")cfg.kanjiRom=value;
                else if(key=="kanji2_rom")cfg.kanji2Rom=value;
                else handled=false;
            }
            if(!handled)throw std::invalid_argument("unknown setting ["+section+"]."+key);
        } catch(const std::exception& e) {
            throw std::runtime_error(path+":"+std::to_string(number)+": "+e.what());
        }
    }
    if(stream.bad())throw std::runtime_error("error reading config: "+path);
    if(!cfg.main6809Installed && !cfg.main8088Installed)
        throw std::runtime_error(path+": no main CPU card installed");
    if(cfg.mainSelected==MachineConfig::MainSelection::Cpu6809 && !cfg.main6809Installed)
        throw std::runtime_error(path+": selected main 6809 card not installed");
    if(cfg.mainSelected==MachineConfig::MainSelection::Cpu8088 && !cfg.main8088Installed)
        throw std::runtime_error(path+": selected main 8088 card not installed");
    return cfg;
}
MachineConfig loadMachineConfig(const std::string& explicitPath) {
    if(!explicitPath.empty())return parseMachineConfigFile(explicitPath);
    const auto userPath=userMachineConfigPath();
    if(!userPath.empty() && exists(userPath))return parseMachineConfigFile(userPath);
    if(exists("/etc/fm11emu.conf"))return parseMachineConfigFile("/etc/fm11emu.conf");
    return {};
}


std::string userMachineConfigPath() {
    if(const char* xdg=std::getenv("XDG_CONFIG_HOME")) {
        // XDG Base Directory: only absolute paths are valid.
        if(*xdg=='/')return (std::filesystem::path(xdg)/"fm11emu"/"fm11emu.conf").string();
    }
    if(const char* home=std::getenv("HOME")) {
        if(*home=='/')return (std::filesystem::path(home)/".config"/"fm11emu"/"fm11emu.conf").string();
    }
    return {};
}

std::string serializeMachineConfig(const MachineConfig& cfg) {
    if(!cfg.main6809Installed && !cfg.main8088Installed)
        throw std::invalid_argument("at least one main CPU card is required");
    if((cfg.mainSelected==MachineConfig::MainSelection::Cpu6809 && !cfg.main6809Installed) ||
       (cfg.mainSelected==MachineConfig::MainSelection::Cpu8088 && !cfg.main8088Installed))
        throw std::invalid_argument("the selected main CPU card is not installed");
    if(cfg.ramKiB<128 || cfg.ramKiB>1024 || cfg.ramKiB%128 ||
       cfg.displayHeight!=200 && cfg.displayHeight!=400 ||
       cfg.dipBootDrive>3 || cfg.dipSerialBaudSwitches>31 ||
       (cfg.dipSystemDisplayLines!=200 && cfg.dipSystemDisplayLines!=400))
        throw std::invalid_argument("invalid machine configuration field");
    const auto clock=[](std::uint64_t hz) {
        if(!hz || hz>1000000000ULL)throw std::invalid_argument("invalid CPU clock");
        return hz;
    };
    clock(cfg.main6809ClockHz);clock(cfg.main8088ClockHz);
    clock(cfg.z80ClockHz);clock(cfg.displayClockHz);
    const auto validPath=[](const std::string& value) {
        if(value.find_first_of("\r\n#;")!=std::string::npos)
            throw std::invalid_argument("path contains INI delimiter or newline");
        // The INI parser trims whitespace; preserving a path with leading or
        // trailing whitespace would silently change its meaning on reload.
        if(!value.empty() && (std::isspace(static_cast<unsigned char>(value.front())) ||
                              std::isspace(static_cast<unsigned char>(value.back()))))
            throw std::invalid_argument("path has leading/trailing whitespace");
        return value;
    };
    for(const auto& name:{cfg.main6809Rom,cfg.main8088Rom,cfg.displayRom,
                          cfg.ankRom,cfg.kanjiRom,cfg.kanji2Rom,cfg.hddImage,cfg.serial0})
        validPath(name);
    for(const auto& path:cfg.floppy2d)validPath(path);
    for(const auto& path:cfg.floppy2hd)validPath(path);
    if(cfg.serial0!="pty" && cfg.serial0!="none" && cfg.serial0.rfind("/dev/",0)!=0)
        throw std::invalid_argument("serial0 must be pty, none or /dev/...");
    const auto boolean=[](bool enabled){return enabled?"true":"false";};
    std::ostringstream out;
    out<<"# FM-11 emulator user configuration (Stage 4)\n"
       <<"[main_6809]\ninstalled = "<<boolean(cfg.main6809Installed)
       <<"\ncpu = "<<(cfg.main6809Cpu==CpuType::HD6309?"6309":"6809")
       <<"\nclock_hz = "<<cfg.main6809ClockHz
       <<"\nrom = "<<cfg.main6809Rom<<"\n\n"
       <<"[main_8088]\ninstalled = "<<boolean(cfg.main8088Installed)
       <<"\ncpu = "<<(cfg.main8088Cpu==CpuType::NECV20?"v20":"8088")
       <<"\nclock_hz = "<<cfg.main8088ClockHz
       <<"\nrom = "<<cfg.main8088Rom<<"\n\n"
       <<"[optional_cpu]\ntype = "<<(cfg.z80Installed?"z80":"none")
       <<"\nclock_hz = "<<cfg.z80ClockHz<<"\n\n"
       <<"[display_cpu]\ncpu = "<<(cfg.displayCpu==CpuType::HD6309?"6309":"6809")
       <<"\nclock_hz = "<<cfg.displayClockHz
       <<"\nrom = "<<cfg.displayRom
       <<"\nank_rom = "<<cfg.ankRom
       <<"\nkanji_rom = "<<cfg.kanjiRom
       <<"\nkanji2_rom = "<<cfg.kanji2Rom<<"\n\n"
       <<"[memory]\nram_kib = "<<cfg.ramKiB<<"\n\n"
       <<"[display]\nheight = "<<cfg.displayHeight<<"\n\n"
       <<"[DIPSW]\nmain_cpu = "<<(cfg.mainSelected==MachineConfig::MainSelection::Cpu6809?"6809":"8088")
       <<"\n2D_FDD = "<<boolean(cfg.boot2dFdd)
       <<"\n2HD_FDD = "<<boolean(cfg.boot2hdFdd)
       <<"\nHDD = "<<boolean(cfg.bootHdd)
       <<"\nboot_drive = "<<cfg.dipBootDrive
       <<"\nbubble = "<<boolean(cfg.dipBootBubble)
       <<"\nserial_cd = "<<(cfg.dipSerialCd==MachineConfig::SerialCd::Ignore?"ignore":"cd_dsr")
       <<"\nserial_mode = "<<(cfg.dipSerialMode==MachineConfig::SerialMode::Async?"async":"sync")
       <<"\nserial_baud_sw4_8 = "<<cfg.dipSerialBaudSwitches
       <<"\ndisplay_lines = "<<cfg.dipSystemDisplayLines<<"\n\n"
       <<"[floppy_2d]\n";
    for(unsigned i=0;i<4;++i)out<<"drive"<<i<<" = "<<cfg.floppy2d[i]<<"\n";
    out<<"\n[floppy_2hd]\n";
    for(unsigned i=0;i<4;++i)out<<"drive"<<i<<" = "<<cfg.floppy2hd[i]<<"\n";
    out<<"\n[floppy]\ntiming = "<<(cfg.floppyTimed?"timed":"fast")
       <<"\n\n[hdd]\nimage = "<<cfg.hddImage
       <<"\ntiming = "<<(cfg.hddTimed?"timed":"fast")
       <<"\n\n[serial]\nserial0 = "<<cfg.serial0
       <<"\nflow_control = "<<(cfg.serialRtsCts?"rtscts":"none")
       <<"\ntiming = "<<(cfg.serialClocked?"clocked":"immediate")<<"\n";
    return out.str();
}

void saveUserMachineConfig(const MachineConfig& config) {
    const auto destination=userMachineConfigPath();
    if(destination.empty())throw std::runtime_error("HOME/XDG_CONFIG_HOME has no absolute path");
    const auto contents=serializeMachineConfig(config);
    const auto parent=std::filesystem::path(destination).parent_path();
    std::error_code ec;
    std::filesystem::create_directories(parent,ec);
    if(ec)throw std::runtime_error("cannot create user config directory: "+ec.message());
    // Do not follow a symlink at the destination; rename replaces the entry
    // itself. The temporary file must be in the same directory for atomicity.
    std::string pattern=destination+".tmp.XXXXXX";
    std::vector<char> temporary(pattern.begin(),pattern.end());
    temporary.push_back('\0');
    const int fd=::mkstemp(temporary.data());
    if(fd<0)throw std::runtime_error("cannot create config temp file: "+std::string(std::strerror(errno)));
    const std::string tmp=temporary.data();
    bool fdOpen=true;
    try {
        if(::fchmod(fd,0600)!=0)throw std::runtime_error("cannot chmod config");
        std::size_t offset=0;
        while(offset<contents.size()) {
            const auto written=::write(fd,contents.data()+offset,contents.size()-offset);
            if(written<0 && errno==EINTR)continue;
            if(written<=0)throw std::runtime_error("config write failed");
            offset+=static_cast<std::size_t>(written);
        }
        if(::fsync(fd)!=0)throw std::runtime_error("config fsync failed");
        const int closeResult=::close(fd);
        fdOpen=false;
        if(closeResult!=0)throw std::runtime_error("config close failed");
        if(::rename(tmp.c_str(),destination.c_str())!=0)
            throw std::runtime_error("config rename failed: "+std::string(std::strerror(errno)));
    } catch(...) {
        if(fdOpen)::close(fd);
        ::unlink(tmp.c_str());
        throw;
    }
}
