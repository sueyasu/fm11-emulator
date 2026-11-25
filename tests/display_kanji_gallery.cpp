#include "KanjiTestPattern.h"
#include "DisplayBitmap.h"
#include <cstdint>
#include <stdexcept>

int main() {
    DisplaySubsystem d;
    DisplaySubsystem::KanjiRomImage rom{};
    // Three diagnostic glyphs across page boundaries.
    rom[0]=0x80;rom[1]=0x01;      // glyph 0, leftmost and rightmost
    rom[32]=0x40;rom[33]=0x02;    // glyph 1
    const unsigned page1=KanjiTestPattern::glyphsPerPage;
    rom[page1*32]=0x20;
    rom[page1*32+1]=0x04;
    d.loadKanjiRom(rom);
    d.reset();
    KanjiTestPattern::paint(d,0);
    auto stats=d.kanjiStatistics();
    if(stats.selections!=KanjiTestPattern::glyphsPerPage ||
       stats.leftReads!=16*KanjiTestPattern::glyphsPerPage ||
       stats.rightReads!=16*KanjiTestPattern::glyphsPerPage)return 1;
    auto pixels=DisplayBitmap::decodeSelectedGraphics400(d);
    // Each 16x16 glyph occupies two adjacent VRAM bytes.
    if(pixels[0]!=7 || pixels[15]!=7 ||
       pixels[16+1]!=7 || pixels[16+14]!=7 ||
       pixels[2]!=0 || pixels[32]!=0)return 2;
    // A real graphics frame is also produced by the normal renderer path.
    auto frame=DisplayBitmap::captureFrame(d,true);
    if(frame.width!=640 || frame.height!=400)return 8;
    if(frame.argb[0]!=DisplayBitmap::colorArgb(7) ||
       frame.argb[2]!=DisplayBitmap::colorArgb(0))return 3;
    KanjiTestPattern::paint(d,1);
    pixels=DisplayBitmap::decodeSelectedGraphics400(d);
    if(pixels[2]!=7 || pixels[8+5]!=7 ||
       pixels[0]!=0 || pixels[16+1]!=0)return 4;
    stats=d.kanjiStatistics();
    if(stats.selections!=2*KanjiTestPattern::glyphsPerPage)return 5;
    // In 400-line mode, the bottom row must be stored/displayed as-is.
    // A 200-line decoder would silently drop these scanlines.
    const unsigned lastRow=KanjiTestPattern::rows-1;
    const unsigned lastCell=(lastRow*KanjiTestPattern::columns);
    rom[(KanjiTestPattern::glyphsPerPage+lastCell)*32]=0x80;
    d.loadKanjiRom(rom);
    KanjiTestPattern::paint(d,1);
    pixels=DisplayBitmap::decodeSelectedGraphics400(d);
    if(pixels[lastRow*16*640]!=7 ||
       pixels[(lastRow*16-1)*640]!=0)return 9;
    stats=d.kanjiStatistics();
    if(stats.selections!=3*KanjiTestPattern::glyphsPerPage)return 10;
    KanjiTestPattern::paint(d,KanjiTestPattern::pages-1);
    stats=d.kanjiStatistics();
    const unsigned finalPageGlyphs=KanjiTestPattern::glyphCount-
        (KanjiTestPattern::pages-1)*KanjiTestPattern::glyphsPerPage;
    if(stats.selections!=3*KanjiTestPattern::glyphsPerPage+
                         finalPageGlyphs)return 6;
    try {
        KanjiTestPattern::paint(d,KanjiTestPattern::pages);
        return 7;
    } catch(const std::out_of_range&) {}
    return 0;
}
