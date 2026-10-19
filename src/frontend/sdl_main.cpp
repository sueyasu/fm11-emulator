#include "Machine.h"
#include "Main6809Board.h"
#include "DisplayBitmap.h"
#include "RomFiles.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
struct Sdl {
    Sdl() {
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0)
            throw std::runtime_error(SDL_GetError());
    }
    ~Sdl(){SDL_Quit();}
};
struct Window {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* texture=nullptr;
    ~Window() {
        if(texture)SDL_DestroyTexture(texture);
        if(renderer)SDL_DestroyRenderer(renderer);
        if(window)SDL_DestroyWindow(window);
    }
    void setMode(bool high) {
        const int height=high?400:200;
        if(texture)SDL_DestroyTexture(texture);
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,640,height);
        if(!texture)throw std::runtime_error(SDL_GetError());
        SDL_SetWindowSize(window,960,height*3/2);
    }
};
std::uint64_t parseCount(const std::string& text) {
    std::size_t end=0;
    const auto value=std::stoull(text,&end,0);
    if(end!=text.size())throw std::invalid_argument("invalid cycle limit");
    return value;
}
void usage() {
    std::cout<<"usage: fm11-sdl [--rom FILE] [--high] [--cycles N] [--test-pattern] [--main-rom FILE] [--trace-boot]\n"
             <<"  ROM defaults to external/roms/display6809.bin\n"
             <<"  ESC quit; SPACE pause/resume; F1 switch 200/400-line view\n"
             <<"  Mode is user-selected until hardware mode bits are decoded.\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string romPath="external/roms/display6809.bin", mainRomPath;
        bool high=false, testPattern=false, traceBoot=false;
        std::uint64_t instructions=10000;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--help"){usage();return 0;}
            if(arg=="--high"){high=true;continue;}
            if(arg=="--test-pattern"){testPattern=true;continue;}
            if(arg=="--trace-boot"){traceBoot=true;continue;}
            if((arg=="--rom" || arg=="--cycles" || arg=="--main-rom") && i+1<argc) {
                if(arg=="--rom")romPath=argv[++i];
                else if(arg=="--main-rom")mainRomPath=argv[++i];
                else instructions=parseCount(argv[++i]);
            } else {usage();return 2;}
        }
        if(instructions>1000000)
            throw std::invalid_argument("--cycles must be <= 1000000");
        Machine machine;
        Main6809Board* mainBoard=nullptr;
        if(!testPattern) {
            machine.loadDisplayRomFile(romPath);
            if(!mainRomPath.empty()) {
                auto mainCpu=std::make_unique<Main6809Board>(CpuType::MC6809);
                mainCpu->loadBootRomFile(mainRomPath);
                mainBoard=mainCpu.get();
                mainCpu->setCpuClockHz(2000000);
                machine.addBoard(std::move(mainCpu));
            }
        }
        machine.setDisplayCpuClockHz(testPattern?0:2000000);
        machine.reset();
        Sdl sdl;
        Window ui;
        ui.window=SDL_CreateWindow("FM-11 display subsystem",
             SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,
             high?600:300,SDL_WINDOW_RESIZABLE);
        if(!ui.window)throw std::runtime_error(SDL_GetError());
        ui.renderer=SDL_CreateRenderer(ui.window,-1,
                       SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!ui.renderer)
            ui.renderer=SDL_CreateRenderer(ui.window,-1,SDL_RENDERER_SOFTWARE);
        if(!ui.renderer)throw std::runtime_error(SDL_GetError());
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
        ui.setMode(high);
        bool running=true,paused=false;
        std::uint64_t executed=0,lastReported=0;
        std::uint64_t ioWrites=0;
        machine.setDisplayIoWriteObserver([&](DisplaySubsystem::IoWriteTrace){++ioWrites;});
        auto lastLog=std::chrono::steady_clock::now();
        while(running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_QUIT)running=false;
                if(event.type==SDL_KEYDOWN && !event.key.repeat) {
                    if(event.key.keysym.sym==SDLK_ESCAPE)running=false;
                    if(event.key.keysym.sym==SDLK_SPACE)paused=!paused;
                    if(event.key.keysym.sym==SDLK_F1) {
                        high=!high;
                        ui.setMode(high);
                    }
                }
            }
            if(!paused && !testPattern) {
                if(machine.boardCount()) {
                    for(std::uint64_t i=0;i<instructions;++i) {
                        // Only inspect the PC before execution: reading memory
                        // via CPU bus here might acknowledge I/O registers.
                        if(traceBoot && executed+i<64 && mainBoard) {
                            std::cerr<<"main step="<<(executed+i)
                                     <<" PC=$"<<std::hex
                                     <<std::setw(4)<<std::setfill('0')
                                     <<mainBoard->cpu().pc()
                                     <<" subCtrl=$"<<std::setw(2)
                                     <<unsigned(machine.mainSubCpuControl())
                                     <<std::dec<<std::setfill(' ')<<"\n";
                        }
                        machine.executeOne();
                    }
                } else {
                    // 2 MHz display CPU: advance elapsed time without a main
                    // CPU. The Machine scheduler also handles HALT and waits.
                    machine.advanceTimeNanoseconds(instructions*500);
                }
                executed+=instructions;
            }
            auto frame=machine.captureDisplayFrame(high);
            if(testPattern) {
                for(std::size_t y=0;y<frame.height;++y)
                    for(std::size_t x=0;x<frame.width;++x)
                        frame.argb[y*frame.width+x]=DisplayBitmap::colorArgb(
                            static_cast<unsigned>((x/80+y/50)%8));
            }
            const auto now=std::chrono::steady_clock::now();
            if(now-lastLog>=std::chrono::seconds(2)) {
                std::cerr<<"display PC="<<std::hex<<machine.displayCpuPc()
                         <<std::dec<<" steps="<<executed
                         <<" delta="<<(executed-lastReported)
                         <<" ioWrites="<<ioWrites
                         <<" VRAM_nonzero="<<machine.displayNonzeroVramBytes()
                         <<" page="<<machine.displayPage()
                         <<" revision="<<machine.displayRevision()
                         <<" mainPC="<<(mainBoard?static_cast<int>(mainBoard->cpu().pc()):-1)
                         <<" FD05="<<unsigned(machine.mainSubCpuControl())
                         <<" FD04_latch="<<unsigned(machine.mainEventStatus())
                         <<" BUSY="<<unsigned(machine.displayBusyStatus())
                         <<" sharedWrites="<<machine.displaySharedWriteRevision()
                         <<" shared0="<<unsigned(machine.displaySharedByte(0))
                         <<" halted="<<machine.displayHalted()
                         <<(machine.boardCount()?" main6809":" display-only")
                         <<(testPattern?" test-pattern":"")<<"\n";
                lastReported=executed;
                lastLog=now;
            }
            if(SDL_UpdateTexture(ui.texture,nullptr,frame.argb.data(),
                                 640*static_cast<int>(sizeof(std::uint32_t)))!=0)
                throw std::runtime_error(SDL_GetError());
            // Integer-only pixel scaling: avoid uneven 1/2-pixel stripes
            // when a 640-wide frame is stretched to a 960-wide window.
            // For windows smaller than the native image, use a centered
            // clipped 1:1 image instead of fractional downscaling.
            int outputW=0,outputH=0;
            if(SDL_GetRendererOutputSize(ui.renderer,&outputW,&outputH)!=0)
                throw std::runtime_error(SDL_GetError());
            const int nativeW=static_cast<int>(frame.width);
            const int nativeH=static_cast<int>(frame.height);
            const int scale=std::max(1,std::min(outputW/nativeW,outputH/nativeH));
            const SDL_Rect destination{
                (outputW-nativeW*scale)/2,
                (outputH-nativeH*scale)/2,
                nativeW*scale,nativeH*scale
            };
            SDL_SetRenderDrawColor(ui.renderer,0,0,0,255);
            if(SDL_RenderClear(ui.renderer)!=0)
                throw std::runtime_error(SDL_GetError());
            if(SDL_RenderCopy(ui.renderer,ui.texture,nullptr,&destination)!=0)
                throw std::runtime_error(SDL_GetError());
            SDL_RenderPresent(ui.renderer);
            SDL_Delay(1);
        }
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"fm11-sdl: "<<e.what()<<"\n";
        return 1;
    }
}
