#include "MdcHddDevice.h"
#include <algorithm>
#include <cstdio>
#include <vector>
#include <initializer_list>

static void command(MdcHddDevice& hdd,BYTE opcode,
                    std::initializer_list<BYTE> params) {
    hdd.write(0xc0,opcode);
    for(BYTE value:params)hdd.write(0xc1,value);
}
static std::vector<BYTE> result(MdcHddDevice& hdd) {
    std::vector<BYTE> bytes;
    for(unsigned n=0;n<10;++n)bytes.push_back(hdd.read(0xc1));
    return bytes;
}
int main() {
    MdcHddDevice hdd;
    // Original M2231B geometry: 157 cylinders, 4 heads, 32 records,
    // 256 bytes/record (5,144,576 bytes).
    std::vector<BYTE> image(157u*4*32*256,0x0e);
    hdd.load(image);
    if(!hdd.present() || hdd.geometry().type!=0x04)return 1;
    hdd.write(0xc2,0x20); // 6844/8237 DMA channel 2, drive 0
    command(hdd,0x14,{0,0});
    if((hdd.read(0xc0)&0x43)!=0x43)return 2;
    const auto type=result(hdd);
    if(type[0]!=0x10 || type[4]!=0x04 || hdd.read(0xc0)!=0x80)return 3;

    unsigned commits=0,reads=0,writes=0;
    hdd.setHostWriter([&](std::size_t offset,const BYTE* bytes,std::size_t length) {
        if(offset!=256 || length!=256 || bytes[0]!=0xa5)return false;
        ++commits;
        return true;
    });
    hdd.setDmaTransfer([&](unsigned channel,bool writing,std::vector<BYTE>& bytes) {
        if(channel!=2 || bytes.size()!=256)return false;
        if(writing) {
            std::fill(bytes.begin(),bytes.end(),0xa5);
            ++writes;
        } else {
            if(bytes[0]!=0xa5)return false;
            ++reads;
        }
        return true;
    });
    // CHR 0:0:1, byte count 256.
    command(hdd,0x23,{0,0,0,0,0,1,1,0});
    if(result(hdd)[0]!=0x10 || writes!=1 || commits!=1 ||
       hdd.image()[256]!=0xa5 || hdd.image()[0]!=0x0e)return 4;
    command(hdd,0x22,{0,0,0,0,0,1,1,0});
    if(result(hdd)[0]!=0x10 || reads!=1)return 5;
    // Drive 1 must not alias the one and only installed HDD.
    command(hdd,0x22,{1,0,0,0,0,1,1,0});
    // MDC result byte 0 includes the selected AD code (drive 1).
    if(result(hdd)[0]!=0x81 || reads!=1)return 6;
    // Host write failure must not mutate the in-memory disk.
    hdd.setHostWriter([](std::size_t,const BYTE*,std::size_t){return false;});
    command(hdd,0x23,{0,0,0,0,0,1,1,0});
    if(result(hdd)[0]!=0x90 || hdd.image()[256]!=0xa5)return 7;
    std::puts("PASS: one-drive MDC geometry, DMA read/write and failure");
    return 0;
}
