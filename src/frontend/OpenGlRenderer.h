#pragma once
#include <memory>
#include <array>
#include <cstdint>

class Machine;

// OpenGL 3.3 renderer. Uploads raw FM-11 VRAM, character RAM and ANK ROM
// as integer textures; GLSL performs graphics decoding and text composition.
class OpenGlRenderer {
public:
    OpenGlRenderer();
    ~OpenGlRenderer();
    OpenGlRenderer(const OpenGlRenderer&)=delete;
    OpenGlRenderer& operator=(const OpenGlRenderer&)=delete;
    // Capture guest state under the display lock; GPU work never holds it.
    struct Snapshot {
        std::array<std::uint8_t,1024*192> vram{};
        std::array<std::uint8_t,4096> text{};
        std::array<std::uint8_t,4096> ank{};
        std::array<unsigned int,8> palette{};
        unsigned control=0;
        int page=0,displayAddress=0,textStart=0,columns=0,rows=0;
        bool hasMainBoard=false;
    };
    static Snapshot capture(const Machine& machine,bool hasAnk);
    void render(const Snapshot& snapshot,bool high,bool testPattern,
                bool hasAnk,int outputWidth,int outputHeight);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
