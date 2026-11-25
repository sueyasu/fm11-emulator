#pragma once
#include "DisplaySubsystem.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

// Standalone, OS-free Kanji ROM gallery. Deliberately uses the modeled
// AFF4/AFF5 selection and AFF6/AFF7 byte ports, not kanjiRomByte(), and
// paints the resulting glyph bits through the ordinary sub-CPU VRAM bus.
// It does not require the sub ROM, an ANK ROM, or a main CPU card.
namespace KanjiTestPattern {
constexpr unsigned columns=40;
constexpr unsigned rows=25;
constexpr unsigned glyphsPerPage=columns*rows;
constexpr unsigned glyphCount=DisplaySubsystem::kKanjiRomSize/32;
constexpr unsigned pages=(glyphCount+glyphsPerPage-1)/glyphsPerPage;
constexpr unsigned bytesPerScanline=80;
constexpr unsigned visibleScanlines=400;

inline void paint(DisplaySubsystem& display,unsigned page) {
    if(page>=pages)throw std::out_of_range("Kanji test page");
    // Three RGB planes receive the same bits -> white-on-black text.
    // Keep the displayed VRAM page and CRTC display start at zero.
    display.writeMemory(0xafe1,0x07);
    display.writeMemory(0xafe2,0);
    display.writeMemory(0xafe3,0);
    display.writeMemory(0xafe8,0);
    display.writeMemory(0xafe9,0);
    display.writeMemory(0xafea,0);
    display.writeMemory(0xafeb,0);
    // Clear the complete 640x400 (32,000-byte) graphics field on every page change.
    for(unsigned offset=0;offset<bytesPerScanline*visibleScanlines;++offset)
        display.writeMemory(static_cast<std::uint16_t>(offset),0);
    for(unsigned cell=0;cell<glyphsPerPage;++cell) {
        const unsigned glyph=page*glyphsPerPage+cell;
        if(glyph>=glyphCount)break;
        const unsigned x=cell%columns;
        const unsigned y=cell/columns;
        display.writeMemory(0xaff4,static_cast<std::uint8_t>(glyph>>8));
        display.writeMemory(0xaff5,static_cast<std::uint8_t>(glyph));
        for(unsigned scan=0;scan<16;++scan) {
            const auto address=static_cast<std::uint16_t>(
                (y*16+scan)*bytesPerScanline+x*2);
            const auto left=display.readMemory(0xaff6);
            const auto right=display.readMemory(0xaff7);
            display.writeMemory(address,left);
            display.writeMemory(static_cast<std::uint16_t>(address+1),right);
        }
    }
}
}
