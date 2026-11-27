#include "Machine.h"
#include "Main8088Board.h"
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
    // Infinite short JMP, recognizable as the disk IPL entry.
    bytes[0]=0xeb;bytes[1]=0xfe;
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
                    out[cursor+16+i]=offset==0?0xeb:
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
    bytes[0]=0xeb;bytes[1]=0xfe;
    for(unsigned i=2;i<512;++i)bytes[i]=BYTE(i^0xa5);
    return bytes;
}
bool boot(const Main8088Bus::Rom& rom,CpuType cpuType,unsigned dipsw,
          unsigned media,unsigned caseNumber) {
    Machine machine;
    auto board=std::make_unique<Main8088Board>(cpuType);
    auto* card=board.get();
    card->bus().loadBootRom(rom);
    machine.addBoard(std::move(board));
    machine.systemBoard().setBootDip(BYTE(dipsw));
    if(media==0)machine.systemBoard().floppyDevice()->load2d(make2d());
    if(media==1)machine.systemBoard().floppyDevice()->loadD882hd(make2hdD88());
    if(media==2)machine.systemBoard().hddDevice()->load(makeHdd());
    machine.reset();
    if(machine.systemBoard().displayControl()!=0) {
        std::cerr<<"Unexpected display control after reset\n";
        return false;
    }
    auto& core=static_cast<Main8088Board::Core&>(card->cpu());
    // A valid IPL starts with JMP $-2 (EB FE). It must only be reached
    // after BOTH disk sectors have been DMA-transferred.
    for(unsigned i=0;i<600000;++i) {
        machine.executeOne();
        if(core.pc()==0x7c00 || core.pc()==0x7c02) {
            if(card->bus().readRam(0x7c00)!=0xeb ||
               card->bus().readRam(0x7c01)!=0xfe)break;
            bool payloadOkay=true;
            for(unsigned j=2;j<512;++j) {
                const BYTE expected=media==0?BYTE(j^0x5a):
                    media==1?BYTE(j^0x3c):BYTE(j^0xa5);
                if(card->bus().readRam(0x7c00+j)!=expected) {
                    payloadOkay=false;break;
                }
            }
            if(payloadOkay) {
                // The actual SDL renderer discards text when FDA0[1:0]
                // remains zero. Reaching the IPL alone is insufficient.
                if((machine.systemBoard().displayControl()&0x03)==0) {
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
    if(bytes.size()!=Main8088Bus::kRomSize) {
        std::cerr<<"ROM must be 4096 bytes\n";
        return 4;
    }
    Main8088Bus::Rom rom{};
    std::copy(bytes.begin(),bytes.end(),rom.begin());
    if(rom[0xff0]!=0xea) { // far JMP at reset vector
        std::cerr<<"Missing far JMP reset vector\n";
        return 5;
    }
    unsigned count=0;
    for(auto cpu:{CpuType::I8088,CpuType::NECV20}) {
        if(!boot(rom,cpu,0xdf,0,++count))return 10+count; // 2D only
        if(!boot(rom,cpu,0xbf,1,++count))return 10+count; // 2HD only
        if(!boot(rom,cpu,0x7f,2,++count))return 10+count; // HDD only
        if(!boot(rom,cpu,0x9f,1,++count))return 10+count; // 2D absent -> 2HD
        if(!boot(rom,cpu,0x1f,2,++count))return 10+count; // both FDDs absent -> HDD
    }
    return 0;
}
