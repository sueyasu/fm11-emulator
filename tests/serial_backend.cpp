#include "MachineConfig.h"
#include "SerialBackend.h"
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <string>

int main() {
    const char* file="fm11-serial-backend-regression.conf";
    struct Cleanup {const char* path;~Cleanup(){std::remove(path);}} cleanup{file};
    for(const std::string endpoint:{"pty","none","/dev/ttyUSB0"}) {
        {std::ofstream out(file);out<<"[serial]\nserial0 = "<<endpoint<<"\n";}
        if(parseMachineConfigFile(file).serial0!=endpoint)return 1;
    }
    for(const std::string endpoint:{"","ttyUSB0","/tmp/tty","/dev/"}) {
        {std::ofstream out(file);out<<"[serial]\nserial0 = "<<endpoint<<"\n";}
        try {(void)parseMachineConfigFile(file);return 2;}
        catch(const std::runtime_error&) {}
    }
    if(MachineConfig{}.serial0!="pty")return 3;
    SerialBackend none("none");
    if(none.enabled())return 4;
    SerialBackend pty("pty");
    if(!pty.enabled() || pty.physical() ||
       pty.label().compare(0,9,"/dev/pts/")!=0)return 5;
    Usart8251 uart;
    uart.reset();uart.writeCommand(0x4e);uart.writeCommand(0x05);
    uart.writeData('A');uart.writeData('B');
    pty.service(
        [&](unsigned char& ch){BYTE b=0;if(!uart.takeOutput(b))return false;ch=b;return true;},
        [&](unsigned char ch){uart.receive(ch);});
    if(!uart.txReady())return 6;
    return 0;
}
