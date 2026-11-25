#include "MdcHddDevice.h"
#include <array>
#include <vector>
#include <initializer_list>

static std::array<BYTE,10> result(MdcHddDevice& d) {
    std::array<BYTE,10> out{};
    for(auto& v:out)v=d.read(0xc1);
    return out;
}
static void issue(MdcHddDevice& d,BYTE cmd,std::initializer_list<BYTE> params) {
    d.write(0xc0,cmd);
    for(BYTE p:params)d.write(0xc1,p);
}
int main() {
    MdcHddDevice d;
    d.load(std::vector<BYTE>(157u*4*32*256,0x5a));
    if(d.irqPending() || d.read(0xc0)!=0x80 || d.read(0xc3)!=0x10)
        return 1;
    d.write(0xc2,0);
    d.write(0xc0,0x20);
    if(d.irqPending() || d.read(0xc0)!=0x21 ||
       d.read(0xc3)!=0x24 || d.read(0xc1)!=0)return 2;
    for(BYTE p:{BYTE(0),BYTE(0),BYTE(0),BYTE(0),
                BYTE(0),BYTE(0),BYTE(0),BYTE(0)})d.write(0xc1,p);
    if(!d.irqPending() || d.read(0xc0)!=0x43 ||
       d.read(0xc3)!=0x4c)return 3;
    if(result(d)[0]!=0x10 || d.irqPending() ||
       d.read(0xc0)!=0x80 || d.read(0xc3)!=0x10)return 4;

    issue(d,0x20,{0,0,0,156,3,31,0,0});
    if(result(d)[0]!=0x10)return 5;
    issue(d,0x20,{0,0,0,157,0,0,0,0});
    auto r=result(d);
    if(r[0]!=0x90 || r[1]!=0x40 || r[2]!=0x10 || r[3]!=0x60)
        return 6;
    issue(d,0x20,{0,0,0,0,4,0,0,0});
    if(result(d)[2]!=0x10)return 7;
    issue(d,0x20,{0,0,0,0,0,32,0,0});
    if(result(d)[2]!=0x10)return 8;
    issue(d,0x20,{0,0,0,0,0,31,0,0});
    if(result(d)[0]!=0x10)return 9;

    issue(d,0x11,{0x70,0}); // binary addressing
    if(result(d)[0]!=0x10)return 10;
    issue(d,0x20,{0,0,0,0,0x4e,0x7f,0,0}); // last record
    if(result(d)[0]!=0x10)return 11;
    issue(d,0x20,{0,0,0,0,0x4e,0x80,0,0}); // out of range
    if(result(d)[2]!=0x10)return 12;
    issue(d,0x11,{0x60,0});
    if(result(d)[0]!=0x10)return 13;

    issue(d,0x20,{1,0,0,0,0,0,0,0}); // absent drive 1
    r=result(d);
    if(r[0]!=0x91 || r[1]!=0x40 || r[2]!=0x10 || r[3]!=0)
        return 14;
    issue(d,0x10,{1,0});
    r=result(d);
    if(r[0]!=0x81 || r[1]!=0x80 || r[2]!=0x02)return 15;
    issue(d,0x10,{0,0});
    if(result(d)[0]!=0x10)return 16;
    issue(d,0x01,{});
    r=result(d);
    if(r[0]!=0x10 || r[3]!=0)return 17;
    issue(d,0x12,{0,0});
    if(result(d)[3]!=0)return 18;
    issue(d,0x13,{0,0});
    if(result(d)[3]!=0)return 19;

    issue(d,0x14,{0,0});
    if(!d.irqPending() || d.read(0xc1)!=0x10)return 20;
    d.write(0xc0,0x20); // new command clears old result IRQ
    if(d.irqPending() || d.read(0xc0)!=0x21)return 21;
    d.write(0xc2,0x08); // controller reset
    if(d.irqPending() || d.read(0xc0)!=0x80)return 22;
    d.write(0xc2,0);
    issue(d,0x26,{0,0,0,0,0,0,0,0}); // unsupported READ ADDRESS
    r=result(d);
    if(r[0]!=0x80 || r[1]!=0x80 || r[2]!=0x01 ||
       r[8]!=0x26)return 23;
    return 0;
}
