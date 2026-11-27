#include "Machine.h"
#include "Main6809Board.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes=std::vector<BYTE>;
void le16(Bytes& out,std::size_t off,unsigned x) {
    out.at(off)=BYTE(x);out.at(off+1)=BYTE(x>>8);
}
void le32(Bytes& out,std::size_t off,std::uint32_t x) {
    for(unsigned i=0;i<4;++i)out.at(off+i)=BYTE(x>>(8*i));
}
Bytes make2d() {
    Bytes bytes(40u*2*16*256);
    // Infinite BRA, recognizable as the disk IPL entry.
    bytes[0]=0x20;bytes[1]=0xfe;
    for(unsigned i=2;i<512;++i)bytes[i]=BYTE(i^0x5a);
    return bytes;
}
Bytes make2hdD88() {
    // Native FM-11 boot track: 26*128 FM; subsequent tracks 26*256 MFM.
    Bytes out(0x2b0);
    out[0x1b]=0x20;
    unsigned cursor=0x2b0;
    for(unsigned t=0;t<77*2;++t) {
        le32(out,0x20+t*4,cursor);
        for(unsigned sec=1;sec<=26;++sec) {
            const unsigned n=t==0?0:1;
            const unsigned len=t==0?128:256;
            out.resize(cursor+16+len);
            out[cursor]=BYTE(t/2);
            out[cursor+1]=BYTE(t%2);
            out[cursor+2]=BYTE(sec);
            out[cursor+3]=BYTE(n);
            le16(out,cursor+4,26);
            le16(out,cursor+14,len);
            if(t==0 && sec<=4) {
                for(unsigned i=0;i<len;++i) {
                    const unsigned offset=(sec-1)*128+i;
                    out[cursor+16+i]=offset==0?0x20:
                        offset==1?0xfe:BYTE(offset^0x3c);
                }
            }
            cursor+=16+len;
        }
    }
    le32(out,0x1c,cursor);
    return out;
}
Bytes makeHdd() {
    Bytes bytes(256*64);
    bytes[0]=0x20;bytes[1]=0xfe;
    for(unsigned i=2;i<512;++i)bytes[i]=BYTE(i^0xa5);
    return bytes;
}
bool boot(const Main6809Board::BootRomImage& rom,CpuType cpuType,unsigned dipsw,
          unsigned media,unsigned caseNumber,unsigned installed=0) {
    Machine machine;
    auto board=std::make_unique<Main6809Board>(cpuType);
    auto* card=board.get();
    card->loadBootRom(rom);
    machine.addBoard(std::move(board));
    machine.systemBoard().setBootDip(BYTE(dipsw));
    card->setBootDip(BYTE(dipsw));
    if(!installed)installed=1u<<media;
    if(installed&1)machine.systemBoard().floppyDevice()->load2d(make2d());
    if(installed&2)machine.systemBoard().floppyDevice()->loadD882hd(make2hdD88());
    if(installed&4)machine.systemBoard().hddDevice()->load(makeHdd());
    machine.reset();
    // Simulate the display ROM mailbox endpoint without proprietary ROM bytes.
    const std::array<BYTE,5> expectedChars{{0x1b,'G',7,0x16,0x0b}};
    unsigned chars=0;
    bool wasHalted=false;
    machine.writeDisplayMemory(0xafe0,1);
    if(machine.systemBoard().displayControl()!=0) {
        std::cerr<<"Unexpected display control after reset\n";
        return false;
    }
    auto& core=card->cpu();
    // A valid IPL starts with BRA $-2 (20 FE). It must only be reached
    // after all IPL sectors have been DMA-transferred.
    for(unsigned i=0;i<600000;++i) {
        machine.executeOne();
        const bool halted=machine.displayHalted();
        if(wasHalted && !halted) {
            if(chars>=expectedChars.size() ||
               machine.displaySharedByte(2)!=0x12 ||
               machine.displaySharedByte(3)!=expectedChars[chars]) {
                std::cerr<<"Unexpected display initialization command\n";
                return false;
            }
            ++chars;
            machine.writeDisplayMemory(0xafe0,1);
        }
        wasHalted=halted;
        if(chars<expectedChars.size() && machine.systemBoard().displayControl()!=0) {
            std::cerr<<"Display enabled before initialization completed\n";
            return false;
        }
        if(core.pc()==0x0400 || core.pc()==0x0402) {
            if(card->readCpu(0x0400)!=0x20 ||
               card->readCpu(0x0401)!=0xfe)break;
            bool payloadOkay=true;
            for(unsigned j=2;j<512;++j) {
                const BYTE expected=media==0?BYTE(j^0x5a):
                    media==1?BYTE(j^0x3c):BYTE(j^0xa5);
                if(card->readCpu(0x0400+j)!=expected) {
                    payloadOkay=false;break;
                }
            }
            if(payloadOkay) {
                // The actual SDL renderer discards text when FDA0[1:0]
                // remains zero. Reaching the IPL alone is insufficient.
                if(chars!=expectedChars.size() || (machine.systemBoard().displayControl()&0x03)!=0x03) {
                    std::cerr<<"Boot IPL reached with text display disabled\n";
                    return false;
                }
                return true;
            }
            break;
        }
    }
    std::cerr<<"compatible boot ROM failed case "<<caseNumber
             <<" CPU="<<int(cpuType)<<" media="<<media
             <<" DIP="<<std::hex<<dipsw
             <<" PC="<<core.pc()<<std::dec<<"\n";
    return false;
}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ifstream f(argv[1],std::ios::binary);
    if(!f)return 3;
    Bytes bytes(std::istreambuf_iterator<char>{f},std::istreambuf_iterator<char>{});
    if(bytes.size()!=Main6809Board::kBootRomSize) {
        std::cerr<<"ROM must be 4096 bytes\n";
        return 4;
    }
    Main6809Board::BootRomImage rom{};
    std::copy(bytes.begin(),bytes.end(),rom.begin());
    if(rom[0xffe]!=0x50 || rom[0xfff]!=0x00) {
        std::cerr<<"Incorrect reset vector\n";
        return 5;
    }
    unsigned count=0;
    for(auto cpu:{CpuType::MC6809,CpuType::HD6309}) {
        if(!boot(rom,cpu,0xdf,0,++count))return 10+count; // 2D only
        if(!boot(rom,cpu,0xbf,1,++count))return 10+count; // 2HD only
        if(!boot(rom,cpu,0x7f,2,++count))return 10+count; // HDD only
        if(!boot(rom,cpu,0x1f,2,++count,7))return 10+count; // all present: HDD wins
        if(!boot(rom,cpu,0x1f,1,++count,3))return 10+count; // HDD absent: 2HD wins
        if(!boot(rom,cpu,0x1f,0,++count,1))return 10+count; // HDD and 2HD absent
        if(!boot(rom,cpu,0x9f,1,++count,7))return 10+count; // HDD disabled
        if(!boot(rom,cpu,0x5f,2,++count,7))return 10+count; // 2HD disabled
        if(!boot(rom,cpu,0xdf,0,++count,7))return 10+count; // HDD and 2HD disabled
    }
    return 0;
}
