#include "MachineConfig.h"
#include "SerialBackend.h"
#include <fstream>
#include <initializer_list>
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
    if(MachineConfig{}.serial0!="pty" || MachineConfig{}.serialRtsCts)return 3;
    for(const std::string setting:{"none","rtscts"}) {
        {std::ofstream out(file);out<<"[serial]\nflow_control = "<<setting<<"\n";}
        if(parseMachineConfigFile(file).serialRtsCts!=(setting=="rtscts"))return 20;
    }
    {std::ofstream out(file);out<<"[serial]\nflow_control = invalid\n";}
    try {(void)parseMachineConfigFile(file);return 21;}
    catch(const std::runtime_error&) {}
    // DIP SW2 affects RxRDY/receive IRQ; the physical DSR status
    // bit remains an independent reflection of the modem pin.
    Usart8251 dipUart;
    dipUart.reset();
    dipUart.requireCarrierAndDsr(true);
    dipUart.writeCommand(0x4e);
    dipUart.writeCommand(0x05);
    dipUart.receive('A');
    dipUart.setModemInputs(true,true,false);
    if(dipUart.rxReady() || (dipUart.status()&2))return 28;
    dipUart.setModemInputs(true,false,true);
    if(dipUart.rxReady() || (dipUart.status()&0x82))return 29;
    dipUart.setModemInputs(true,true,true);
    if(!dipUart.rxReady() || dipUart.readData()!='A')return 30;
    dipUart.writeCommand(0x40);
    dipUart.writeCommand(0x4e);
    dipUart.writeCommand(0x05);
    if(!dipUart.carrierAndDsrRequired())return 31;
    SerialBackend none("none");
    Usart8251 virtualUart;
    virtualUart.reset();
    virtualUart.writeCommand(0x4e);
    virtualUart.writeCommand(0x22);
    none.synchronize(virtualUart);
    if(!virtualUart.cts() || !virtualUart.dsr() || !virtualUart.dtr() ||
       !virtualUart.rts())return 7;
    if(none.enabled())return 4;
    SerialBackend pty("pty");
    if(!pty.enabled() || pty.physical() ||
       pty.label().compare(0,9,"/dev/pts/")!=0)return 5;
    pty.synchronize(virtualUart);
    if(!virtualUart.cts() || !virtualUart.dsr())return 8;
    Usart8251 uart;
    uart.reset();uart.writeCommand(0x4e);uart.writeCommand(0x05);
    uart.writeData('A');uart.writeData('B');
    pty.service(
        [&](unsigned char& ch){BYTE b=0;if(!uart.takeOutput(b))return false;ch=b;return true;},
        [&](unsigned char ch){uart.receive(ch);});
    if(!uart.txReady())return 6;
    // Validate 8251A/termios stop-bit encodings independently of a driver.
    if(!SerialBackend::stopBitsRepresentable(0x4e) ||
       !SerialBackend::stopBitsRepresentable(0xce) ||
       !SerialBackend::stopBitsRepresentable(0x82) || // 5-bit 1.5 stop
       SerialBackend::stopBitsRepresentable(0x8e) || // 8-bit 1.5 stop
       SerialBackend::stopBitsRepresentable(0xc2) || // 5-bit 2 stops
       SerialBackend::stopBitsRepresentable(0x0e))return 9;

    // Validate all representable asynchronous framing translations before
    // testing the host driver. PTY drivers may silently clear PARENB/CS7.
    struct Framing {unsigned mode;tcflag_t size;bool parity,odd,twoStops;};
    for(const Framing f:std::initializer_list<Framing>{
        {0x4e,CS8,false,false,false}, // 8N1
        {0xce,CS8,false,false,true},  // 8N2
        {0x7a,CS7,true,false,false},  // 7E1
        {0x5a,CS7,true,true,false},   // 7O1
        {0x7e,CS8,true,false,false},  // 8E1
        {0x82,CS5,false,false,true},  // 5-bit 1.5 stops (POSIX CS5+CSTOPB)
    }) {
        const auto flags=SerialBackend::lineControlFlags(f.mode);
        if((flags&CSIZE)!=f.size || !!(flags&PARENB)!=f.parity ||
           !!(flags&PARODD)!=f.odd || !!(flags&CSTOPB)!=f.twoStops)
            return 22;
    }
    for(const unsigned mode:{0x00u,0x0eu,0x8eu,0xc2u}) {
        try {(void)SerialBackend::lineControlFlags(mode);return 23;}
        catch(const std::runtime_error&) {}
    }
    // Verify all defined asynchronous DIP baud rates through a PTY. Only
    // baud and 8-bit stop flags are asserted using tcgetattr() here.
    struct BaudCase {unsigned dip;speed_t speed;};
    for(const BaudCase rate:std::initializer_list<BaudCase>{
        {8u,B75},{9u,B150},{0u,B300},{1u,B600},{2u,B1200},
        {3u,B2400},{4u,B4800},{5u,B9600}
    }) {
        const int m=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
        if(m<0 || grantpt(m)!=0 || unlockpt(m)!=0)return 24;
        const char* path=ptsname(m);
        if(!path)return 25;
        {
            SerialBackend backend(path,rate.dip);
            termios t{};
            const int fd=open(path,O_RDWR|O_NOCTTY|O_NONBLOCK);
            if(fd<0)return 26;
            const bool valid=tcgetattr(fd,&t)==0 &&
                             cfgetispeed(&t)==rate.speed &&
                             cfgetospeed(&t)==rate.speed;
            close(fd);
            if(!valid)return 27;
        }
        close(m);
    }
    // Exercise the /dev/... branch against a PTY slave (no USB device).
    const int master=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
    if(master<0 || grantpt(master)!=0 || unlockpt(master)!=0)return 10;
    const char* slave=ptsname(master);
    if(!slave)return 11;
    const std::string slaveName=slave;
    {
        SerialBackend physical(slaveName,4); // Communication SW4..8 = 00100
        if(!physical.enabled() || !physical.physical())return 12;
        const int verify=open(slaveName.c_str(),O_RDWR|O_NOCTTY|O_NONBLOCK);
        if(verify<0)return 13;
        termios actual{};
        if(tcgetattr(verify,&actual)!=0 || cfgetospeed(&actual)!=B4800 ||
           (actual.c_cflag&CSIZE)!=CS8 || (actual.c_cflag&CSTOPB))return 14;
        Usart8251 guest;
        guest.reset();
        guest.writeCommand(0xce); // asynchronous 8N2
        guest.writeCommand(0x27);
        physical.synchronize(guest);
        if(tcgetattr(verify,&actual)!=0 || cfgetospeed(&actual)!=B4800 ||
           !(actual.c_cflag&CSTOPB))return 15;
        guest.writeCommand(0x40);
        guest.writeCommand(0x4e); // reset then asynchronous 8N1
        guest.writeCommand(0x27);
        physical.synchronize(guest);
        if(tcgetattr(verify,&actual)!=0 || (actual.c_cflag&CSTOPB))return 16;
        guest.writeData('K');
        auto tx=[&](unsigned char& ch) {
            BYTE value=0;
            if(!guest.takeOutput(value))return false;
            ch=value;return true;
        };
        auto rx=[&](unsigned char ch) {guest.receive(ch);};
        physical.service(tx,rx);
        unsigned char received=0;
        if(read(master,&received,1)!=1 || received!='K')return 17;
        const unsigned char answer='M';
        if(write(master,&answer,1)!=1)return 18;
        physical.service(tx,rx);
        if(!guest.rxReady() || guest.readData()!='M')return 19;
        close(verify);
    }
    close(master);
    return 0;
}
