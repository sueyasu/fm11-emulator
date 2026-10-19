#include "MainBoard.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include <memory>
#include <iostream>
int main() {
    auto bus=std::make_shared<MainBoard>();
    Main6809Board cpu(CpuType::MC6809);
    cpu.setMainBoard(bus);
    bus->reset();
    if(bus->displayControl()!=0 ||
       bus->displayPalette(2)!=2)return 1;
    cpu.writeCpu(0xfda0,0x0d);
    if(bus->displayControl()!=0x0d)return 2;
    cpu.writeCpu(0xfd9a,0x0f);
    if(bus->displayPalette(2)!=15)return 3;
    cpu.writeCpu(0xfd9a,0x12);
    if(bus->displayPalette(2)!=2)return 4;
    if(DisplayBitmap::referenceColor(0)!=0xff000000u ||
       DisplayBitmap::referenceColor(2)!=0xffc00000u ||
       DisplayBitmap::referenceColor(15)!=0xffffffffu)return 5;
    bus->reset();
    if(bus->displayControl()!=0 || bus->displayPalette(2)!=2)return 6;
    std::cout<<"PASS: FD A0, FD98-9F and original RGB table\n";
    return 0;
}
