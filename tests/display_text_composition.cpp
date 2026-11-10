#include "DisplayBitmap.h"
#include "DisplaySubsystem.h"
#include <cstdint>
#include <iostream>

int main() {
    DisplaySubsystem display;
    DisplaySubsystem::AnkRomImage ank{};
    ank[65*16]=0x01;    // low bit is LEFTMOST pixel in reference shader
    ank[65*16+1]=0x02;  // second row, next pixel
    display.loadAnkRom(ank);
    auto setCrtc=[&](BYTE reg,BYTE value) {
        display.writeMemory(0xafee,reg);
        display.writeMemory(0xafef,value);
    };
    setCrtc(1,40); // 80 columns, 8 pixels each
    setCrtc(6,25); // 25 rows, 16 source scanlines each
    display.writeMemory(0x8000,65);
    display.writeMemory(0x8001,0x07);
    auto high=DisplayBitmap::captureFrame(display,true);
    DisplayBitmap::compositeText(high,display);
    if(high.argb[0]!=0xffc0c0c0u || high.argb[1]!=0xff000000u)return 1;
    if(high.argb[640+1]!=0xffc0c0c0u || high.argb[640]!=0xff000000u)return 2;
    if(high.argb[8]!=0xff000000u)return 3;
    // Text scanout occupies 400 virtual scanlines even with 200-line output.
    auto low=DisplayBitmap::captureFrame(display,false);
    DisplayBitmap::compositeText(low,display);
    if(low.argb[0]!=0xffc0c0c0u || low.argb[640+1]!=0xff000000u)return 4;
    // CRTC start address is a TWO-BYTE address per text cell.
    display.writeMemory(0x8000,0);
    display.writeMemory(0x8002,65);
    display.writeMemory(0x8003,0x07);
    setCrtc(13,1);
    high=DisplayBitmap::captureFrame(display,true);
    DisplayBitmap::compositeText(high,display);
    if(high.argb[0]!=0xffc0c0c0u)return 5;
    // A reversed space paints its full cell, matching fm11.hlsl.
    display.writeMemory(0x8002,0x20);
    display.writeMemory(0x8003,0x0a);
    high=DisplayBitmap::captureFrame(display,true);
    DisplayBitmap::compositeText(high,display);
    if(high.argb[0]!=0xffc00000u ||
       high.argb[7]!=0xffc00000u ||
       high.argb[8]!=0xff000000u)return 6;
    // The original renderer truncated the 8 KiB sub-CPU text RAM to
    // 4 KiB, so a long-running CRTC scroll wrapped at 2048 cells.
    // With 80 columns that is 48 columns into a line, causing the
    // conspicuous mid-row split seen after prolonged CP/M output.
    setCrtc(12,0x08); // CRTC cell 0x800 -> RAM byte 0x1000 ($9000)
    setCrtc(13,0);
    display.writeMemory(0x9000,65);
    display.writeMemory(0x9001,0x07);
    display.writeMemory(0x8000,0);
    high=DisplayBitmap::captureFrame(display,true);
    DisplayBitmap::compositeText(high,display);
    if(high.argb[0]!=0xffc0c0c0u)return 7;

    // At the end of the 8 KiB ring, the next character must appear
    // at $8000 (not $9000 and not byte 0x1000 into the 4 KiB ring).
    setCrtc(12,0x0f);
    setCrtc(13,0xff);
    display.writeMemory(0x9ffe,65);
    display.writeMemory(0x9fff,0x07);
    display.writeMemory(0x8000,65);
    display.writeMemory(0x8001,0x07);
    high=DisplayBitmap::captureFrame(display,true);
    DisplayBitmap::compositeText(high,display);
    if(high.argb[0]!=0xffc0c0c0u || high.argb[8]!=0xffc0c0c0u)return 8;

    std::cout<<"PASS: CRTC text CRAM, 8 KiB wrapping, ANK, 200/400 lines, reverse attribute\n";
    return 0;
}
