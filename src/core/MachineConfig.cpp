#include "MachineConfig.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
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
                   section!="display_cpu")
                    throw std::invalid_argument("unknown section ["+section+"]");
                continue;
            }
            const auto equals=line.find('=');
            if(equals==std::string::npos || section.empty())
                throw std::invalid_argument("expected section and key=value");
            const auto key=trim(line.substr(0,equals));
            const auto value=trim(line.substr(equals+1));
            bool handled=true;
            if(section=="main") {
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
                else handled=false;
            } else if(section=="main_8088") {
                if(key=="installed")cfg.main8088Installed=boolean(value);
                else if(key=="cpu") {
                    if(value=="8088")cfg.main8088Cpu=CpuType::I8088;
                    else if(value=="v20")cfg.main8088Cpu=CpuType::NECV20;
                    else throw std::invalid_argument("main_8088.cpu must be 8088 or v20");
                } else if(key=="clock_hz")cfg.main8088ClockHz=clockHz(value);
                else handled=false;
            } else if(section=="optional_cpu") {
                if(key=="type") {
                    if(value=="none")cfg.z80Installed=false;
                    else if(value=="z80")cfg.z80Installed=true;
                    else throw std::invalid_argument("optional_cpu.type must be none or z80");
                } else if(key=="clock_hz")cfg.z80ClockHz=clockHz(value);
                else handled=false;
            } else if(section=="display_cpu") {
                if(key=="cpu") {
                    if(value!="6809")
                        throw std::invalid_argument("display CPU supports 6809 only (6309 not implemented)");
                } else if(key=="clock_hz")cfg.displayClockHz=clockHz(value);
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
    if(const char* home=std::getenv("HOME")) {
        const auto path=std::string(home)+"/.config/fm11emu/fm11emu.conf";
        if(exists(path))return parseMachineConfigFile(path);
    }
    if(exists("/etc/fm11emu.conf"))return parseMachineConfigFile("/etc/fm11emu.conf");
    return {};
}
