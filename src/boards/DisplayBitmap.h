#pragma once
#include "DisplaySubsystem.h"
#include <array>
#include <cstddef>
#include <cstdint>

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
