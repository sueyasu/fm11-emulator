#include "Main8088Bus.h"

int main()
{
    Main8088Bus bus;
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea; // x86 reset fetch at physical FFFF0
    rom[0xff1]=0x00;
    bus.loadBootRom(rom);
    if(bus.readMemory(0xffff0)!=0xea)return 1;
    if(bus.readMemory(0xffff1)!=0)return 2;
    bus.writeMemory(0xffff0,0x12);
    if(bus.readMemory(0xffff0)!=0xea || bus.readRam(0xffff0)!=0x12)return 3;
    bus.writeMemory(0x100020,0x77);
    if(bus.readMemory(0x20)!=0x77)return 4;
    bus.writeIo(0xfc50,0xa5);
    if(bus.readIo(0xfc50)!=0xa5)return 5;
    WORD lastPort=0; BYTE lastValue=0;
    bus.setSystemIo([](WORD port){return port==0xfd18 ? BYTE(0x42):BYTE(0);},
                    [&](WORD port,BYTE value){lastPort=port;lastValue=value;});
    if(bus.readIo(0xfd18)!=0x42)return 6;
    bus.writeIo(0xfd18,0x80);
    if(lastPort!=0xfd18 || lastValue!=0x80)return 7;
    if(bus.readIo(0xfd18)!=0x42)return 8;
    bus.reset();
    if(bus.readIo(0xfc50)!=0)return 9;
    if(bus.readMemory(0xffff0)!=0xea)return 10;
    return 0;
}
