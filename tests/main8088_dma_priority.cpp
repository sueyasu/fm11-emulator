#include "Main8088Bus.h"
#include <array>

int main()
{
    Main8088Bus bus;
    bus.reset();
    std::array<unsigned,4> served{};
    for(unsigned channel=0;channel<4;++channel) {
        bus.setDmaDevice(channel,[&,channel]()->BYTE {
            ++served[channel];
            return BYTE(channel);
        },{});
        bus.writeIo(0x1c,0); // reset address/count byte flip-flop
        bus.writeIo(WORD(0x10+2*channel),0);
        bus.writeIo(WORD(0x10+2*channel),0);
        bus.writeIo(WORD(0x11+2*channel),3); // 4 bytes per channel
        bus.writeIo(WORD(0x11+2*channel),0);
        bus.writeIo(0x1b,BYTE(0x44|channel)); // single mode, write memory
        bus.writeIo(0x1a,BYTE(channel));      // unmask
        bus.writeIo(0x19,BYTE(4|channel));    // software request
    }
    // Fixed priority repeatedly selects channel 0.
    if(!bus.serviceDma() || !bus.serviceDma())return 1;
    if(served[0]!=2 || served[1]!=0)return 2;
    // Rotating priority: after channel 0, channels 1, 2, 3 become next.
    bus.writeIo(0x18,0x10);
    for(int i=0;i<4;++i)if(!bus.serviceDma())return 3;
    if(served[0]!=3 || served[1]!=1 || served[2]!=1 || served[3]!=1)return 4;
    // Disabling rotation restores fixed-priority selection.
    bus.writeIo(0x18,0x00);
    if(!bus.serviceDma() || served[0]!=4)return 5;
    return 0;
}
