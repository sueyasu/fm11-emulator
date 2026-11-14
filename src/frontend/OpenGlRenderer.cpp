#include "OpenGlRenderer.h"
#include "Machine.h"
#include "DisplayBitmap.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {
using GLenum=unsigned int;
using GLuint=unsigned int;
using GLint=int;
using GLsizei=int;
using GLchar=char;
using GLboolean=unsigned char;
using GLbitfield=unsigned int;
constexpr GLenum VERTEX_SHADER=0x8B31,FRAGMENT_SHADER=0x8B30;
constexpr GLenum COMPILE_STATUS=0x8B81,LINK_STATUS=0x8B82;
constexpr GLenum TEXTURE_2D=0x0DE1, TEXTURE0=0x84C0;
constexpr GLenum TEXTURE_MIN_FILTER=0x2801,TEXTURE_MAG_FILTER=0x2800;
constexpr GLenum TEXTURE_WRAP_S=0x2802,TEXTURE_WRAP_T=0x2803;
constexpr GLenum NEAREST=0x2600,CLAMP_TO_EDGE=0x812F;
constexpr GLenum R8UI=0x8232,RED_INTEGER=0x8D94,UNSIGNED_BYTE=0x1401;
constexpr GLenum UNPACK_ALIGNMENT=0x0CF5;
constexpr GLenum COLOR_BUFFER_BIT=0x00004000,SCISSOR_TEST=0x0C11;
constexpr GLenum TRIANGLES=0x0004;

struct Api {
#define GL_FN(ret,name,...) using name##Proc=ret(*)(__VA_ARGS__); name##Proc name=nullptr;
    GL_FN(GLuint,CreateShader,GLenum)
    GL_FN(void,ShaderSource,GLuint,GLsizei,const GLchar* const*,const GLint*)
    GL_FN(void,CompileShader,GLuint)
    GL_FN(void,GetShaderiv,GLuint,GLenum,GLint*)
    GL_FN(void,GetShaderInfoLog,GLuint,GLsizei,GLsizei*,GLchar*)
    GL_FN(void,DeleteShader,GLuint)
    GL_FN(GLuint,CreateProgram)
    GL_FN(void,AttachShader,GLuint,GLuint)
    GL_FN(void,LinkProgram,GLuint)
    GL_FN(void,GetProgramiv,GLuint,GLenum,GLint*)
    GL_FN(void,GetProgramInfoLog,GLuint,GLsizei,GLsizei*,GLchar*)
    GL_FN(void,DeleteProgram,GLuint)
    GL_FN(void,UseProgram,GLuint)
    GL_FN(void,GenVertexArrays,GLsizei,GLuint*)
    GL_FN(void,BindVertexArray,GLuint)
    GL_FN(void,DeleteVertexArrays,GLsizei,const GLuint*)
    GL_FN(void,GenTextures,GLsizei,GLuint*)
    GL_FN(void,BindTexture,GLenum,GLuint)
    GL_FN(void,TexParameteri,GLenum,GLenum,GLint)
    GL_FN(void,TexImage2D,GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)
    GL_FN(void,TexSubImage2D,GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*)
    GL_FN(void,DeleteTextures,GLsizei,const GLuint*)
    GL_FN(void,ActiveTexture,GLenum)
    GL_FN(GLint,GetUniformLocation,GLuint,const GLchar*)
    GL_FN(void,Uniform1i,GLint,GLint)
    GL_FN(void,Uniform1ui,GLint,GLuint)
    GL_FN(void,Uniform4uiv,GLint,GLsizei,const GLuint*)
    GL_FN(void,Uniform2i,GLint,GLint,GLint)
    GL_FN(void,Viewport,GLint,GLint,GLsizei,GLsizei)
    GL_FN(void,ClearColor,float,float,float,float)
    GL_FN(void,Clear,GLbitfield)
    GL_FN(void,Enable,GLenum)
    GL_FN(void,Disable,GLenum)
    GL_FN(void,Scissor,GLint,GLint,GLsizei,GLsizei)
    GL_FN(void,DrawArrays,GLenum,GLint,GLsizei)
    GL_FN(void,PixelStorei,GLenum,GLint)
#undef GL_FN
    Api() {
#define LOAD(name) do { name=reinterpret_cast<name##Proc>(SDL_GL_GetProcAddress("gl" #name)); if(!name)throw std::runtime_error("OpenGL 3.3 function missing: gl" #name); } while(false)
        LOAD(CreateShader);LOAD(ShaderSource);LOAD(CompileShader);
        LOAD(GetShaderiv);LOAD(GetShaderInfoLog);LOAD(DeleteShader);
        LOAD(CreateProgram);LOAD(AttachShader);LOAD(LinkProgram);
        LOAD(GetProgramiv);LOAD(GetProgramInfoLog);LOAD(DeleteProgram);
        LOAD(UseProgram);LOAD(GenVertexArrays);LOAD(BindVertexArray);
        LOAD(DeleteVertexArrays);LOAD(GenTextures);LOAD(BindTexture);
        LOAD(TexParameteri);LOAD(TexImage2D);LOAD(TexSubImage2D);
        LOAD(DeleteTextures);LOAD(ActiveTexture);LOAD(GetUniformLocation);
        LOAD(Uniform1i);LOAD(Uniform1ui);LOAD(Uniform4uiv);LOAD(Uniform2i);
        LOAD(Viewport);LOAD(ClearColor);LOAD(Clear);LOAD(Enable);
        LOAD(Disable);LOAD(Scissor);LOAD(DrawArrays);LOAD(PixelStorei);
#undef LOAD
    }
};

constexpr const char* vertexSource=R"GLSL(#version 330 core
void main() {
    vec2 xy = vec2((gl_VertexID==1)?3.0:-1.0,
                   (gl_VertexID==2)?3.0:-1.0);
    gl_Position=vec4(xy,0.0,1.0);
}
)GLSL";

constexpr const char* fragmentSource=R"GLSL(#version 330 core
uniform usampler2D uVram;
uniform usampler2D uText;
uniform usampler2D uAnk;
uniform uvec4 uPalette0;
uniform uvec4 uPalette1;
uniform ivec2 uOrigin;
uniform int uScale;
uniform int uHeight;
uniform int uGraphics;
uniform int uTextEnabled;
uniform int uTestPattern;
uniform int uPage;
uniform int uDisplayAddress;
uniform int uTextStart;
uniform int uColumns;
uniform int uRows;
out vec4 outputColor;

uint vramByte(int address) {
    return texelFetch(uVram,ivec2(address%1024,address/1024),0).r;
}
uint textByte(int address) {
    int offset=address&4095;
    return texelFetch(uText,ivec2(offset%256,offset/256),0).r;
}
uint ankByte(int character,int row) {
    int offset=character*16+row;
    return texelFetch(uAnk,ivec2(offset%256,offset/256),0).r;
}
uint palette(uint index) {
    if(index<4u)return uPalette0[int(index)];
    return uPalette1[int(index-4u)];
}
vec4 rgba(uint c) {
    return vec4(float((c>>16u)&255u),float((c>>8u)&255u),
                float(c&255u),255.0)/255.0;
}
void main() {
    ivec2 p=ivec2(gl_FragCoord.xy)-uOrigin;
    int x=p.x/uScale;
    // OpenGL is bottom-up, the FM-11 scanout is top-down.
    int y=uHeight-1-p.y/uScale;
    uint color=0u;
    if(uTestPattern!=0) {
        uint index=uint((x/80+y/50)%8);
        color=((index&1u)!=0u?0x00ff0000u:0u)|
              ((index&2u)!=0u?0x0000ff00u:0u)|
              ((index&4u)!=0u?0x000000ffu:0u);
    } else {
        if(uGraphics!=0) {
            int base=uHeight==400?(uPage/2)*32768:uPage*16384;
            int address=(base+((uDisplayAddress+(y*80+x/8))&32767))&65535;
            uint bit=128u>>uint(x%8);
            uint index=0u;
            if((vramByte(address)&bit)!=0u)index|=1u;
            if((vramByte(65536+address)&bit)!=0u)index|=2u;
            if((vramByte(131072+address)&bit)!=0u)index|=4u;
            color=palette(index);
        }
        if(uTextEnabled!=0 && uColumns>0 && uColumns<=160 &&
           uRows>0 && uRows<=100) {
            int charW=640/uColumns,charH=400/uRows;
            int scanY=y*400/uHeight;
            int cx=x/charW,cy=scanY/charH;
            if(cx<uColumns && cy<uRows) {
                int offset=(uTextStart+(cx+cy*uColumns)*2)&4095;
                uint ch=textByte(offset);
                uint attr=textByte(offset+1);
                int sx=x%charW,sy=scanY%charH;
                if(charW>8)sx/=2;
                bool lit=false;
                if(ch!=0u && ch!=32u && sy<16 && sx<8)
                    lit=(ankByte(int(ch),sy)&(1u<<uint(sx)))!=0u;
                if((attr&8u)!=0u)lit=!lit;
                if(lit) {
                    // The CPU renderer overlays foreground pixels, including
                    // reverse-video blanks, on the graphics background.
                    uint ci=(attr&7u)|((attr&32u)!=0u?8u:0u);
                    const uint shades[16]=uint[16](
                        0x000000u,0x0000c0u,0xc00000u,0xc000c0u,
                        0x00c000u,0x00c0c0u,0xc0c000u,0xc0c0c0u,
                        0x606060u,0x0000ffu,0xff0000u,0xff00ffu,
                        0x00ff00u,0x00ffffu,0xffff00u,0xffffffu);
                    color=shades[int(ci)];
                }
            }
        }
    }
    outputColor=rgba(color);
}
)GLSL";

GLuint compileShader(const Api& gl,GLenum kind,const char* source) {
    const GLuint shader=gl.CreateShader(kind);
    gl.ShaderSource(shader,1,&source,nullptr);
    gl.CompileShader(shader);
    GLint ok=0;
    gl.GetShaderiv(shader,COMPILE_STATUS,&ok);
    if(!ok) {
        std::array<char,4096> msg{};
        gl.GetShaderInfoLog(shader,static_cast<GLsizei>(msg.size()),nullptr,msg.data());
        gl.DeleteShader(shader);
        throw std::runtime_error(std::string("GLSL compile failed: ")+msg.data());
    }
    return shader;
}
} // namespace

struct OpenGlRenderer::Impl {
    Api gl;
    GLuint program=0,vao=0;
    std::array<GLuint,3> textures{};
    Impl() {
        const GLuint vertex=compileShader(gl,VERTEX_SHADER,vertexSource);
        const GLuint fragment=compileShader(gl,FRAGMENT_SHADER,fragmentSource);
        program=gl.CreateProgram();
        gl.AttachShader(program,vertex);
        gl.AttachShader(program,fragment);
        gl.LinkProgram(program);
        gl.DeleteShader(vertex);
        gl.DeleteShader(fragment);
        GLint ok=0;
        gl.GetProgramiv(program,LINK_STATUS,&ok);
        if(!ok) {
            std::array<char,4096> msg{};
            gl.GetProgramInfoLog(program,static_cast<GLsizei>(msg.size()),nullptr,msg.data());
            throw std::runtime_error(std::string("GLSL link failed: ")+msg.data());
        }
        gl.GenVertexArrays(1,&vao);
        gl.BindVertexArray(vao);
        gl.GenTextures(3,textures.data());
        gl.PixelStorei(UNPACK_ALIGNMENT,1);
        constexpr int widths[3]={1024,256,256};
        constexpr int heights[3]={192,16,16};
        for(int i=0;i<3;++i) {
            gl.ActiveTexture(TEXTURE0+static_cast<GLenum>(i));
            gl.BindTexture(TEXTURE_2D,textures[i]);
            gl.TexParameteri(TEXTURE_2D,TEXTURE_MIN_FILTER,NEAREST);
            gl.TexParameteri(TEXTURE_2D,TEXTURE_MAG_FILTER,NEAREST);
            gl.TexParameteri(TEXTURE_2D,TEXTURE_WRAP_S,CLAMP_TO_EDGE);
            gl.TexParameteri(TEXTURE_2D,TEXTURE_WRAP_T,CLAMP_TO_EDGE);
            gl.TexImage2D(TEXTURE_2D,0,R8UI,widths[i],heights[i],
                          0,RED_INTEGER,UNSIGNED_BYTE,nullptr);
        }
        gl.UseProgram(program);
        setInt("uVram",0);setInt("uText",1);setInt("uAnk",2);
    }
    ~Impl() {
        if(textures[0])gl.DeleteTextures(3,textures.data());
        if(vao)gl.DeleteVertexArrays(1,&vao);
        if(program)gl.DeleteProgram(program);
    }
    void setInt(const char* key,int val) const {
        gl.Uniform1i(gl.GetUniformLocation(program,key),val);
    }
    void render(const Machine& machine,bool high,bool testPattern,
                bool hasAnk,int outputWidth,int outputHeight) {
        const auto& display=machine.displaySubsystem();
        gl.UseProgram(program);
        gl.BindVertexArray(vao);
        const auto& physical=display.physicalVram();
        std::array<std::uint8_t,4096> text{};
        std::array<std::uint8_t,4096> ank{};
        for(std::size_t i=0;i<text.size();++i)text[i]=display.textByte(i);
        if(hasAnk)
            for(unsigned ch=0;ch<256;++ch)
                for(unsigned row=0;row<16;++row)
                    ank[ch*16+row]=display.ankGlyphRow(static_cast<BYTE>(ch),row);
        const void* memory[]={physical.data(),text.data(),ank.data()};
        constexpr int widths[3]={1024,256,256};
        constexpr int heights[3]={192,16,16};
        for(int i=0;i<3;++i) {
            gl.ActiveTexture(TEXTURE0+static_cast<GLenum>(i));
            gl.BindTexture(TEXTURE_2D,textures[i]);
            gl.TexSubImage2D(TEXTURE_2D,0,0,0,widths[i],heights[i],
                             RED_INTEGER,UNSIGNED_BYTE,memory[i]);
        }
        unsigned int colors[8]{};
        for(unsigned i=0;i<8;++i)
            colors[i]=DisplayBitmap::referenceColor(machine.mainDisplayPalette(i));
        // GL sees only 24-bit RGB; CPU referenceColor carries alpha.
        auto palette0=gl.GetUniformLocation(program,"uPalette0");
        auto palette1=gl.GetUniformLocation(program,"uPalette1");
        gl.Uniform4uiv(palette0,1,colors);
        gl.Uniform4uiv(palette1,1,colors+4);
        setInt("uHeight",high?400:200);
        setInt("uTestPattern",testPattern?1:0);
        const unsigned control=machine.mainDisplayControl();
        setInt("uGraphics",(machine.boardCount()==0 || (control&0x0c))?1:0);
        setInt("uTextEnabled",hasAnk && (machine.boardCount()==0 || (control&3))?1:0);
        setInt("uPage",static_cast<int>(display.graphicsDisplayPage()));
        setInt("uDisplayAddress",display.graphicsDisplayAddress());
        setInt("uTextStart",static_cast<int>(display.textStartAddress()));
        setInt("uColumns",unsigned(display.crtcRegister(1))*2);
        setInt("uRows",display.crtcRegister(6));

        const int nativeHeight=high?400:200;
        const int scale=std::max(1,std::min(outputWidth/640,outputHeight/nativeHeight));
        const int w=640*scale,h=nativeHeight*scale;
        const int x=(outputWidth-w)/2,y=(outputHeight-h)/2;
        gl.Disable(SCISSOR_TEST);
        gl.Viewport(0,0,outputWidth,outputHeight);
        gl.ClearColor(48/255.0f,52/255.0f,56/255.0f,1);
        gl.Clear(COLOR_BUFFER_BIT);
        // Draw the same 1-pixel outline as the software renderer.
        gl.Enable(SCISSOR_TEST);
        gl.Scissor(std::max(0,x-1),std::max(0,y-1),
                   std::min(outputWidth,w+2),std::min(outputHeight,h+2));
        gl.ClearColor(144/255.0f,148/255.0f,152/255.0f,1);
        gl.Clear(COLOR_BUFFER_BIT);
        gl.Disable(SCISSOR_TEST);
        gl.Viewport(x,y,w,h);
        gl.Uniform2i(gl.GetUniformLocation(program,"uOrigin"),x,y);
        setInt("uScale",scale);
        gl.DrawArrays(TRIANGLES,0,3);
    }
};
OpenGlRenderer::OpenGlRenderer():impl_(std::make_unique<Impl>()){}
OpenGlRenderer::~OpenGlRenderer()=default;
void OpenGlRenderer::render(const Machine& machine,bool high,bool testPattern,
                            bool hasAnk,int width,int height) {
    impl_->render(machine,high,testPattern,hasAnk,width,height);
}
