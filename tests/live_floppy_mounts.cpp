#include "LiveFloppyMounts.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
void writeImage(const std::string& path,std::size_t length,BYTE fill) {
    std::ofstream out(path,std::ios::binary);
    std::vector<BYTE> bytes(length,fill);
    out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if(!out)throw std::runtime_error("test image creation failed");
}
BYTE firstByte(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    return BYTE(in.get());
}
}
int main() {
    char name[]="/tmp/fm11-hot-fdd-XXXXXX";
    const char* dir=::mkdtemp(name);
    if(!dir)return 1;
    const auto base=std::filesystem::path(dir);
    const auto imageA=(base/"a.raw").string();
    const auto imageB=(base/"b.raw").string();
    const auto imageC=(base/"c.raw").string();
    const auto imageHd=(base/"hd.raw").string();
    writeImage(imageA,40u*2*16*256,0x11);
    writeImage(imageB,40u*2*16*256,0x22);
    writeImage(imageC,40u*2*16*256,0x33);
    writeImage(imageHd,77u*2*8*1024,0x44);
    auto fdc=std::make_shared<FloppyDevice>();
    HostImageSync sync;
    LiveFloppyMounts mounts(fdc,sync);
    mounts.installWriters();

    mounts.exchange(0,0,imageA);
    if(!fdc->has2d(0) || fdc->raw2d(0)[0]!=0x11)return 2;
    if(!fdc->beginSector(0,0,0,1,0,true))return 3;
    // A partial sector write must block media exchange, not be lost.
    fdc->transferWrite(0x99);
    try { mounts.exchange(0,0,imageB);return 4; }
    catch(const std::runtime_error&) {}
    if(mounts.path(0,0)!=imageA || fdc->raw2d(0)[0]!=0x11)return 5;
    for(unsigned n=1;n<256;++n)fdc->transferWrite(0x99);
    if(firstByte(imageA)!=0x99)return 6;

    mounts.exchange(0,0,imageB);
    if(mounts.path(0,0)!=imageB || fdc->raw2d(0)[0]!=0x22)return 7;
    if(!fdc->beginSector(0,0,0,1,0,true))return 8;
    for(unsigned n=0;n<256;++n)fdc->transferWrite(0x66);
    if(firstByte(imageB)!=0x66 || firstByte(imageA)!=0x99)return 9;

    // A second virtual drive cannot mount the same physical file even
    // through an alias; the existing media must remain unchanged.
    mounts.exchange(0,1,imageC);
    try {mounts.exchange(0,1,imageB);return 10;}
    catch(const std::runtime_error&) {}
    if(mounts.path(0,1)!=imageC || fdc->raw2d(1)[0]!=0x33)return 11;
    const auto link=(base/"alias.raw").string();
    std::filesystem::create_symlink(imageB,link);
    try {mounts.exchange(0,1,link);return 12;}
    catch(const std::runtime_error&) {}
    if(mounts.path(0,1)!=imageC)return 13;

    // Validation must finish before any host descriptor is detached.
    const auto invalid=(base/"wrong.bin").string();
    writeImage(invalid,100,0xee);
    try {mounts.exchange(0,0,invalid);return 14;}
    catch(const std::invalid_argument&) {}
    if(mounts.path(0,0)!=imageB || fdc->raw2d(0)[0]!=0x66)return 15;

    // The two controllers share a callback but maintain independent slots.
    mounts.exchange(1,3,imageHd);
    if(!fdc->has2hd(3) || !fdc->has2d(1))return 16;
    mounts.exchange(0,0,{});
    if(fdc->has2d(0) || mounts.path(0,0)!="")return 17;
    if(!fdc->has2hd(3) || !fdc->has2d(1))return 18;
    mounts.exchange(0,0,imageA);
    if(fdc->raw2d(0)[0]!=0x99)return 19;
    if(!sync.shutdown())return 20;
    std::filesystem::remove_all(base);
    return 0;
}
