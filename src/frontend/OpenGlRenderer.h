#pragma once
#include <memory>

class Machine;

// OpenGL 3.3 renderer. Uploads raw FM-11 VRAM, character RAM and ANK ROM
// as integer textures; GLSL performs graphics decoding and text composition.
class OpenGlRenderer {
public:
    OpenGlRenderer();
    ~OpenGlRenderer();
    OpenGlRenderer(const OpenGlRenderer&)=delete;
    OpenGlRenderer& operator=(const OpenGlRenderer&)=delete;
    void render(const Machine& machine,bool high,bool testPattern,
                bool hasAnk,int outputWidth,int outputHeight);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
