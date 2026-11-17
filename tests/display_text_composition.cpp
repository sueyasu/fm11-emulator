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
    // Each 200-line pixel covers two ANK raster lines. Both must
    // contribute, otherwise fine horizontal strokes disappear.
    auto low=DisplayBitmap::captureFrame(display,false);
    DisplayBitmap::compositeText(low,display);
    if(low.argb[0]!=0xffc0c0c0u ||
       low.argb[1]!=0xffc0c0c0u ||
       low.argb[640+1]!=0xff000000u)return 4;
    // The final baseline must not become a two-row-thick horizontal
    // stroke after combining 16 ANK rasters into eight 200-line rows.
    DisplaySubsystem baselineDisplay;
    DisplaySubsystem::AnkRomImage baselineAnk{};
    baselineAnk[66*16+12]=0x7e;
    baselineAnk[66*16+13]=0x7e;
    baselineAnk[66*16+14]=0x7e;
    baselineAnk[66*16+15]=0x7e;
    baselineDisplay.loadAnkRom(baselineAnk);
    baselineDisplay.writeMemory(0xafee,1);
    baselineDisplay.writeMemory(0xafef,40);
    baselineDisplay.writeMemory(0xafee,6);
    baselineDisplay.writeMemory(0xafef,25);
    baselineDisplay.writeMemory(0x8000,66);
    baselineDisplay.writeMemory(0x8001,7);
    auto baselineLow=DisplayBitmap::captureFrame(baselineDisplay,false);
    DisplayBitmap::compositeText(baselineLow,baselineDisplay);
    if(baselineLow.argb[6*640+1]!=0xffc0c0c0u ||
       baselineLow.argb[7*640+1]!=0xff000000u)return 18;
    // A genuinely new isolated bottom-row pixel remains visible.
    baselineAnk[66*16+15]=0x7f;
    baselineDisplay.loadAnkRom(baselineAnk);
    baselineLow=DisplayBitmap::captureFrame(baselineDisplay,false);
    DisplayBitmap::compositeText(baselineLow,baselineDisplay);
    if(baselineLow.argb[7*640]!=0xffc0c0c0u ||
       baselineLow.argb[7*640+1]!=0xff000000u)return 19;
    auto baselineHigh=DisplayBitmap::captureFrame(baselineDisplay,true);
    DisplayBitmap::compositeText(baselineHigh,baselineDisplay);
    if(baselineHigh.argb[14*640+1]!=0xffc0c0c0u ||
       baselineHigh.argb[15*640+1]!=0xffc0c0c0u)return 20;

    // Match CSubCpu::SubCRTC and OnDraw: R13=1 starts four
    // bytes (two character/attribute cells) into the 4 KiB CRAM.
    display.writeMemory(0x8000,0);
    display.writeMemory(0x8004,65);
    display.writeMemory(0x8005,0x07);
    setCrtc(13,1);
    high=DisplayBitmap::captureFrame(display,true);
    DisplayBitmap::compositeText(high,display);
    if(high.argb[0]!=0xffc0c0c0u)return 5;
    // A reversed space paints its full cell, matching fm11.hlsl.
    display.writeMemory(0x8004,0x20);
    display.writeMemory(0x8005,0x0a);
    high=DisplayBitmap::captureFrame(display,true);
    DisplayBitmap::compositeText(high,display);
    if(high.argb[0]!=0xffc00000u ||
       high.argb[7]!=0xffc00000u ||
       high.argb[8]!=0xff000000u)return 6;
    // R12 writes do not change the active start until a subsequent R13
    // write, even if the new R13 value is identical to the previous one.
    display.writeMemory(0x8004,65);
    display.writeMemory(0x8005,7);
    setCrtc(12,1);
    if(display.textStartAddress()!=4)return 7;
    high=DisplayBitmap::captureFrame(display,true);
    DisplayBitmap::compositeText(high,display);
    if(high.argb[0]!=0xffc0c0c0u)return 8;
    setCrtc(13,1);
    if(display.textStartAddress()!=1028)return 9;
    display.writeMemory(0x8404,65);
    display.writeMemory(0x8405,7);
    high=DisplayBitmap::captureFrame(display,true);
    DisplayBitmap::compositeText(high,display);
    if(high.argb[0]!=0xffc0c0c0u)return 10;

    // Original start wraps at 0x0800 before scaling to byte offsets.
    setCrtc(12,4);
    setCrtc(13,0);
    if(display.textStartAddress()!=0)return 11;
    // The 6845 cursor is drawn by the frontend, not written to text RAM.
    // Verify steady, disabled, two blink periods and AFE0 position bit.
    DisplaySubsystem cursorDisplay;
    auto setCursorReg=[&](BYTE reg,BYTE value) {
        cursorDisplay.writeMemory(0xafee,reg);
        cursorDisplay.writeMemory(0xafef,value);
    };
    setCursorReg(1,40);
    setCursorReg(6,25);
    setCursorReg(10,14);             // steady underline, first raster 14
    setCursorReg(11,15);
    setCursorReg(14,0);
    setCursorReg(15,0);
    auto cf=DisplayBitmap::captureFrame(cursorDisplay,true);
    DisplayBitmap::compositeCursor(cf,cursorDisplay,0);
    if(cf.argb[14*640]!=0xffffffffu || cf.argb[13*640]!=0xff000000u)return 12;
    setCursorReg(10,0x2e);          // hidden
    cf=DisplayBitmap::captureFrame(cursorDisplay,true);
    DisplayBitmap::compositeCursor(cf,cursorDisplay,0);
    if(cf.argb[14*640]!=0xff000000u)return 13;
    setCursorReg(10,0x4e);          // blink at 16-frame intervals
    cf=DisplayBitmap::captureFrame(cursorDisplay,true);
    DisplayBitmap::compositeCursor(cf,cursorDisplay,16);
    if(cf.argb[14*640]!=0xff000000u)return 14;
    setCursorReg(10,0x6e);          // blink at 32-frame intervals
    cf=DisplayBitmap::captureFrame(cursorDisplay,true);
    DisplayBitmap::compositeCursor(cf,cursorDisplay,16);
    if(cf.argb[14*640]!=0xffffffffu)return 15;
    cursorDisplay.writeMemory(0xafe0,0x20);
    cf=DisplayBitmap::captureFrame(cursorDisplay,true);
    DisplayBitmap::compositeCursor(cf,cursorDisplay,0);
    if(cf.argb[14*640]!=0xff000000u || cf.argb[14*640+8]!=0xffffffffu)
        return 16;
    // An underline on *only* odd raster 15 must survive in low mode.
    setCursorReg(10,15);
    setCursorReg(11,15);
    cursorDisplay.writeMemory(0xafe0,0);
    cf=DisplayBitmap::captureFrame(cursorDisplay,false);
    DisplayBitmap::compositeCursor(cf,cursorDisplay,0);
    if(cf.argb[7*640]!=0xffffffffu || cf.argb[6*640]!=0xff000000u)
        return 17;
    std::cout<<"PASS: CRTC start latch/scale/wrap, 200-line ANK strokes, cursor modes and raster\\n";
    return 0;
}
