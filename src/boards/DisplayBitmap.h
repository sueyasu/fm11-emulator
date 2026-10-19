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
