#include "Machine.h"
#include "DisplaySubsystem.h"
#include <cstdint>

int main() {
    Machine machine;
    auto& d=machine.systemBoard().displaySubsystem();
    DisplaySubsystem::KanjiRomImage rom{};
    for(std::size_t glyph=0;glyph<rom.size()/32;++glyph)
        for(unsigned row=0;row<16;++row) {
            rom[glyph*32+row*2]=static_cast<BYTE>(0x40u+(glyph&15u)+row);
            rom[glyph*32+row*2+1]=static_cast<BYTE>(0x80u+(glyph&15u)+row);
        }
    d.loadKanjiRom(rom);
    machine.reset();
    // Before a glyph is selected, AFF6 returns 0 and AFF7 returns 1.
    if(d.readMemory(0xaff6)!=0 || d.readMemory(0xaff7)!=1)return 1;
    if(const auto stats=d.kanjiStatistics();
       stats.selections || stats.leftReads || stats.rightReads)return 11;
    d.writeMemory(0xaff4,0x00);
    d.writeMemory(0xaff5,0x03);
    // Independent row cursors: left may lead right.
    if(d.readMemory(0xaff6)!=0x43 || d.readMemory(0xaff6)!=0x44 ||
       d.readMemory(0xaff7)!=0x83 || d.readMemory(0xaff7)!=0x84)
        return 2;
    for(unsigned row=2;row<16;++row) {
        if(d.readMemory(0xaff6)!=BYTE(0x43+row) ||
           d.readMemory(0xaff7)!=BYTE(0x83+row))return 3;
    }
    if(d.readMemory(0xaff6)!=0 || d.readMemory(0xaff7)!=1)return 4;
    if(const auto stats=d.kanjiStatistics();
       stats.selections!=1 || stats.leftReads!=16 || stats.rightReads!=16)
        return 12;
    // Last 32-byte glyph in the 128-KiB ROM.
    d.writeMemory(0xaff4,0x0f);
    d.writeMemory(0xaff5,0xff);
    if(d.readMemory(0xaff6)!=0x4f || d.readMemory(0xaff7)!=0x8f)
        return 5;
    // Out-of-range selector leaves the previous glyph cursors intact.
    d.writeMemory(0xaff4,0x10);
    d.writeMemory(0xaff5,0x00);
    if(d.readMemory(0xaff6)!=0x50 || d.readMemory(0xaff7)!=0x90)
        return 6;
    // Optional bank uses AFF4 bit4 (address A16).
    DisplaySubsystem::KanjiRomImage level2{};
    level2[3*32]=0x13;
    level2[3*32+1]=0x93;
    level2[0]=0x27;
    level2[1]=0xa7;
    d.loadKanji2Rom(level2);
    d.writeMemory(0xaff4,0x10);
    d.writeMemory(0xaff5,0x03);
    if(d.readMemory(0xaff6)!=0x13 || d.readMemory(0xaff7)!=0x93)return 14;
    d.writeMemory(0xaff4,0x00);
    d.writeMemory(0xaff5,0x03);
    if(d.readMemory(0xaff6)!=0x43 || d.readMemory(0xaff7)!=0x83)return 15;
    d.writeMemory(0xaff4,0x10);
    d.writeMemory(0xaff5,0x00);
    if(d.readMemory(0xaff6)!=0x27 || d.readMemory(0xaff7)!=0xa7)return 16;
    // High bit selects the RTC window on AFF7, disables Kanji on AFF6.
    d.writeMemory(0xaff4,0x8d);
    d.writeMemory(0xaff7,0xa5);
    if(d.readMemory(0xaff7)!=0xa5 || d.readMemory(0xaff6)!=0)return 7;
    d.writeMemory(0xaff4,0x00);
    d.writeMemory(0xaff5,0x00);
    if(d.readMemory(0xaff6)!=0x40 || d.readMemory(0xaff7)!=0x80)
        return 8;
    // Reset does not remove ROM contents but clears selection/cursors.
    machine.reset();
    if(d.readMemory(0xaff6)!=0 || d.readMemory(0xaff7)!=1)return 9;
    if(const auto stats=d.kanjiStatistics();
       stats.selections || stats.leftReads || stats.rightReads)return 13;
    d.writeMemory(0xaff4,0);
    d.writeMemory(0xaff5,3);
    if(d.readMemory(0xaff6)!=0x43)return 10;
    return 0;
}
