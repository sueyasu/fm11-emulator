#include "Machine.h"
#include "Main6809Board.h"
#include "Main8088Board.h"
#include "MainZ80Board.h"
#include <memory>

int main()
{
    Machine m;
    m.setRamKiB(128);
    auto p=std::make_unique<Main8088Board>();
    auto* x=p.get();
    Main8088Bus::Rom rom{};
    rom.fill(0x90);
    const BYTE code[]={0xba,0x05,0xfd,0xb0,1,0xee,0x90};
    for(unsigned n=0;n<sizeof(code);++n)rom[0xff0+n]=code[n];
    x->bus().loadBootRom(rom);
    const auto xi=m.addBoard(std::move(p));
    auto q=std::make_unique<MainZ80Board>();
    auto* z=q.get();
    const auto zi=m.addBoard(std::move(q));
    m.enableFd05Selection(true);
    m.reset();
    if(m.activeBoard()!=x || z->bank()!=0)return 1;
    // Fixed low and high windows bank 1; middle still bank 0 at reset.
    const WORD offsets[]={0x0000,0x3fff,0x4000,0xbfff,0xc000,0xfbff};
    for(unsigned n=0;n<6;++n) {
        const auto off=offsets[n];
        x->bus().writeMemory(off,static_cast<BYTE>(0x20+n));
        x->bus().writeMemory(0x10000u+off,static_cast<BYTE>(0x40+n));
        const BYTE expected=static_cast<BYTE>((n==2 || n==3?0x20:0x40)+n);
        if(z->readMemory(off)!=expected)return 2;
    }
    z->writeMemory(0xfd2f,0xf1);
    if(z->bank()!=1)return 3;
    for(unsigned n=0;n<6;++n)
        if(z->readMemory(offsets[n])!=0x40+n)return 4;
    z->writeMemory(0xfd2f,3); // 128 KiB: physical bank 3 mirrors bank 1
    z->writeMemory(0x4567,0xa5);
    if(x->bus().readRam(0x14567)!=0xa5)return 5;
    // Native 8237 writes into shared RAM, visible to Z80.
    x->bus().writeIo(0x1c,0);
    x->bus().writeIo(0x14,0x78);x->bus().writeIo(0x14,0x56);
    x->bus().writeIo(0x15,0);x->bus().writeIo(0x15,0);
    x->bus().writeIo(0x22,1);
    x->bus().writeIo(0x1b,0x46);
    x->bus().setDmaDevice(2,[]{return BYTE(0x6d);},{});
    x->bus().writeIo(0x1a,2);
    x->bus().writeIo(0x19,6);
    if(!x->bus().serviceDma() || z->readMemory(0x5678)!=0x6d)return 6;
    // Reset restores only bank register, not installed RAM or fixed mapping.
    m.reset();
    if(z->bank()!=0 || z->readMemory(0x0000)!=0x40)return 7;
    const BYTE zcode[]={0x3e,0,0x32,0x05,0xfd,0};
    for(unsigned n=0;n<sizeof(zcode);++n)x->bus().writeMemory(0x10000+n,zcode[n]);
    m.executeOne();m.executeOne();m.executeOne();
    if(m.activeBoard()!=z || x->cpu().pc()!=0xffff6)return 8;
    m.executeOne();m.executeOne();
    if(m.activeBoard()!=x || z->cpu().pc()!=5)return 9;
    m.executeOne();
    if(x->cpu().pc()!=0xffff7)return 10;
    // All three installed cards see the same memory, and explicit master
    // selection (rather than owner or insertion order) controls fixed banks.
    auto a=std::make_unique<Main6809Board>(CpuType::MC6809);
    auto* y=a.get();
    auto yi=m.addBoard(std::move(a));
    if(y->physicalMemory()!=x->bus().physicalMemory() ||
       z->physicalMemory()!=y->physicalMemory())return 11;
    if(y->readPhysical(0x14567)!=0xa5)return 12;
    y->writePhysical(0x1234,0x11);
    y->writePhysical(0x11234,0x22);
    m.configureFd05Boards(xi,zi);
    if(z->readMemory(0x1234)!=0x22)return 13;
    m.configureFd05Boards(yi,zi);
    if(z->readMemory(0x1234)!=0x11)return 14;
    m.setRamKiB(256);
    z->writeMemory(0xfd2f,3);
    z->writeMemory(0x4567,0x55);
    if(y->ramKiB()!=256 || x->bus().ramKiB()!=256 || z->ramKiB()!=256 ||
       y->readPhysical(0x34567)!=0x55 || x->bus().readRam(0x34567)!=0x55)return 15;
    // Reversed attachment of just 8088/Z80, with automatic FD05 wiring.
    Machine reversed;
    auto rz=std::make_unique<MainZ80Board>();
    auto* rzptr=rz.get();
    reversed.addBoard(std::move(rz));
    auto rx=std::make_unique<Main8088Board>();
    auto* rxptr=rx.get();
    reversed.addBoard(std::move(rx));
    reversed.enableFd05Selection(true);
    reversed.reset();
    rxptr->bus().writeMemory(0x1c123,0xe1);
    if(reversed.activeBoard()!=rxptr || rzptr->readMemory(0xc123)!=0xe1)return 16;
    rxptr->bus().writeIo(0xfd05,1);
    if(reversed.activeBoard()!=rzptr)return 17;
    rzptr->writeMemory(0xfd05,0);
    if(reversed.activeBoard()!=rxptr)return 18;
    return 0;
}
