#pragma once
#include "DisplaySubsystem.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Diagnostic interpretation of a single raw 1-bpp VRAM plane, not the
// final FM-11 color/plane decoder. 640 * 200 / 8 = 16000 bytes.
// Keeping it independent of the CPU and display-control flags makes the
// hardware-specific renderer replaceable once the VRAM banking is known.
class DisplayBitmap {
public:
    static constexpr std::size_t kWidth=640;
    static constexpr std::size_t kHeight=200;
    static constexpr std::size_t kBytesPerLine=kWidth/8;
    using Pixels=std::array<std::uint8_t,kWidth*kHeight>;

    // 192 KiB can hold four 640x200 3-plane pages (48 KiB per page).
    // This decoder treats each page as three consecutive 16 KiB planes.
    // The hardware's actual CPU-side bank mapping and plane order remain
    // unverified; the output is an RGB-bit index, not a palette color.
    static constexpr std::size_t kColorPageBytes=0xc000;
    static constexpr std::size_t kColorPlaneBytes=0x4000;
    static constexpr std::size_t kColorPages=
        DisplaySubsystem::kPhysicalVramSize/kColorPageBytes;
    static Pixels decodeColorPage(const DisplaySubsystem& display,
                                  std::size_t page) {
        if(page>=kColorPages)
            throw std::out_of_range("display color page");
        Pixels output{};
        const auto& memory=display.physicalVram();
        const auto base=page*kColorPageBytes;
        for(std::size_t y=0;y<kHeight;++y)
            for(std::size_t x=0;x<kWidth;++x) {
                const auto pos=y*kBytesPerLine+x/8;
                const auto shift=7-(x%8);
                unsigned color=0;
                for(std::size_t plane=0;plane<3;++plane)
                    color|=((memory[base+plane*kColorPlaneBytes+pos]>>shift)&1u)<<plane;
                output[y*kWidth+x]=static_cast<std::uint8_t>(color);
            }
        return output;
    }

    // Diagnostic 640x400 mode: 32 KiB per plane, three planes per
    // 96 KiB page. 192 KiB physical VRAM holds two such pages.
    // Plane order and CPU-side bank selection remain hardware-dependent.
    static constexpr std::size_t kHighHeight=400;
    static constexpr std::size_t kHighPlaneBytes=0x8000;
    static constexpr std::size_t kHighPageBytes=3*kHighPlaneBytes;
    static constexpr std::size_t kHighPages=
        DisplaySubsystem::kPhysicalVramSize/kHighPageBytes;
    using HighPixels=std::array<std::uint8_t,kWidth*kHighHeight>;

    static HighPixels decodeHighColorPage(const DisplaySubsystem& display,
                                          std::size_t page) {
        if(page>=kHighPages)
            throw std::out_of_range("display high-resolution page");
        HighPixels output{};
        const auto& memory=display.physicalVram();
        const auto base=page*kHighPageBytes;
        for(std::size_t y=0;y<kHighHeight;++y)
            for(std::size_t x=0;x<kWidth;++x) {
                const auto pos=y*kBytesPerLine+x/8;
                const auto shift=7-(x%8);
                unsigned color=0;
                for(std::size_t plane=0;plane<3;++plane)
                    color|=((memory[base+plane*kHighPlaneBytes+pos]>>shift)&1u)<<plane;
                output[y*kWidth+x]=static_cast<std::uint8_t>(color);
            }
        return output;
    }

    // Graphics scanout follows the source emulator's three separate
    // 64KiB VRAM planes, unlike the earlier diagnostic contiguous layout.
    // In 200-line mode each plane contains four 16KiB page slots;
    // in 400-line mode each plane contains two 32KiB page slots.
    static Pixels decodeGraphics200(const DisplaySubsystem& display,
                                    std::size_t page) {
        if(page>=4)throw std::out_of_range("graphics 200-line page");
        Pixels output{};
        const auto& memory=display.physicalVram();
        for(std::size_t pos=0;pos<kBytesPerLine*kHeight;++pos) {
            const auto address=(page*0x4000+((display.graphicsDisplayAddress()+pos)&0x7fff))&0xffff;
            for(unsigned bit=0;bit<8;++bit) {
                const auto shift=7-bit;
                unsigned index=0;
                for(unsigned plane=0;plane<3;++plane)
                    index|=((memory[plane*0x10000+address]>>shift)&1u)<<plane;
                output[pos*8+bit]=static_cast<std::uint8_t>(index);
            }
        }
        return output;
    }

    static HighPixels decodeGraphics400(const DisplaySubsystem& display,
                                        std::size_t page) {
        if(page>=2)throw std::out_of_range("graphics 400-line page");
        HighPixels output{};
        const auto& memory=display.physicalVram();
        for(std::size_t pos=0;pos<kBytesPerLine*kHighHeight;++pos) {
            const auto address=(page*0x8000+((display.graphicsDisplayAddress()+pos)&0x7fff))&0xffff;
            for(unsigned bit=0;bit<8;++bit) {
                const auto shift=7-bit;
                unsigned index=0;
                for(unsigned plane=0;plane<3;++plane)
                    index|=((memory[plane*0x10000+address]>>shift)&1u)<<plane;
                output[pos*8+bit]=static_cast<std::uint8_t>(index);
            }
        }
        return output;
    }

    static Pixels decodeSelectedGraphics200(const DisplaySubsystem& display) {
        return decodeGraphics200(display,display.graphicsDisplayPage());
    }
    static HighPixels decodeSelectedGraphics400(const DisplaySubsystem& display) {
        return decodeGraphics400(display,display.graphicsDisplayPage()>>1);
    }

    // Digital 8-color RGB palette: low 3 index bits map to R/G/B.
    // The array is frontend-friendly ARGB8888, independent of SDL.
    using ArgbPixels=std::array<std::uint32_t,kWidth*kHeight>;
    static std::uint32_t colorArgb(unsigned c) {
        return 0xff000000u | ((c&1u)?0x00ff0000u:0u) |
            ((c&2u)?0x0000ff00u:0u) | ((c&4u)?0x000000ffu:0u);
    }
    static ArgbPixels toArgb(const Pixels& indexes) {
        ArgbPixels out{};
        for(std::size_t i=0;i<out.size();++i) {
            out[i]=colorArgb(indexes[i]);
        }
        return out;
    }

    // Frontend frame capture. A caller chooses the display height explicitly
    // until mode-select bits have been verified against the original ROM.
    // All RGB conversion is done in one call on a copied pixel buffer;
    // neither the sub CPU state nor physical VRAM is changed.
    struct Frame {
        std::size_t width=kWidth;
        std::size_t height=0;
        std::uint64_t revision=0;
        std::vector<std::uint32_t> argb;
    };
    static Frame captureFrame(const DisplaySubsystem& display,bool highResolution) {
        Frame frame{};
        frame.height=highResolution?kHighHeight:kHeight;
        frame.revision=display.videoRevision();
        if(highResolution) {
            const auto pixels=decodeSelectedGraphics400(display);
            frame.argb.resize(pixels.size());
            for(std::size_t i=0;i<pixels.size();++i)
                frame.argb[i]=colorArgb(pixels[i]);
        } else {
            const auto pixels=decodeSelectedGraphics200(display);
            const auto image=toArgb(pixels);
            frame.argb.assign(image.begin(),image.end());
        }
        return frame;
    }

    // Composite a single 8x16 ANK glyph into an indexed 640x200 frame.
    // This primitive makes no assumptions about text VRAM or cursor I/O.
    static void drawAnk(Pixels& pixels,const DisplaySubsystem& display,
                        BYTE character,std::size_t x,std::size_t y,
                        BYTE foreground,BYTE background) {
        for(std::size_t row=0;row<16 && y+row<kHeight;++row) {
            if(x>=kWidth)break;
            const auto bits=display.ankGlyphRow(character,static_cast<unsigned>(row));
            for(std::size_t bit=0;bit<8 && x+bit<kWidth;++bit)
                pixels[(y+row)*kWidth+x+bit]=
                    (bits&(0x80u>>bit))?foreground:background;
        }
    }

    // Diagnostic two-plane combination: each bit from the first 16000
    // bytes contributes bit 0, and each corresponding bit at +0x4000
    // contributes bit 1. This is not yet a confirmed FM-11 color mode.
    static Pixels decodeTwoPlanes(const DisplaySubsystem::VideoSnapshot& frame) {
        Pixels output{};
        for(std::size_t y=0;y<kHeight;++y)
            for(std::size_t x=0;x<kWidth;++x) {
                const auto pos=y*kBytesPerLine+x/8;
                const auto shift=7-(x%8);
                const auto low=(frame.vram[pos]>>shift)&1u;
                const auto high=(frame.vram[0x4000+pos]>>shift)&1u;
                output[y*kWidth+x]=static_cast<std::uint8_t>(low|(high<<1));
            }
        return output;
    }

    static Pixels decodeMonochrome(const DisplaySubsystem::VideoSnapshot& frame,
                                   std::size_t offset=0) {
        Pixels output{};
        if(offset>frame.vram.size()-kBytesPerLine*kHeight)
            return output;
        for(std::size_t y=0;y<kHeight;++y)
            for(std::size_t x=0;x<kWidth;++x) {
                const auto packed=frame.vram[offset+y*kBytesPerLine+x/8];
                output[y*kWidth+x]=static_cast<std::uint8_t>(
                    (packed>>(7-(x%8)))&1);
            }
        return output;
    }
};
