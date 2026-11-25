#pragma once
#include "BeeperDevice.h"
#include <SDL.h>
#include <cstdint>
#include <iostream>
#include <memory>

// Output mono 16-bit samples at the actual SDL playback rate. The audio
// callback is the sole renderer; CPU writes are synchronized by BeeperDevice.
class SdlBeeper {
public:
    explicit SdlBeeper(std::shared_ptr<BeeperDevice> device)
        :device_(std::move(device)) {
        if(SDL_InitSubSystem(SDL_INIT_AUDIO)!=0) {
            std::cerr<<"FM-11 audio disabled: "<<SDL_GetError()<<"\n";
            return;
        }
        audioInitialized_=true;
        SDL_AudioSpec desired{};
        desired.freq=48000;
        desired.format=AUDIO_S16SYS;
        desired.channels=1;
        desired.samples=512;
        desired.callback=&SdlBeeper::callback;
        desired.userdata=this;
        SDL_AudioSpec obtained{};
        id_=SDL_OpenAudioDevice(nullptr,0,&desired,&obtained,0);
        if(!id_) {
            std::cerr<<"FM-11 audio device unavailable: "<<SDL_GetError()<<"\n";
            return;
        }
        sampleRate_=static_cast<unsigned>(obtained.freq);
        SDL_PauseAudioDevice(id_,0);
    }
    ~SdlBeeper() {
        if(id_)SDL_CloseAudioDevice(id_);
        if(audioInitialized_)SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    SdlBeeper(const SdlBeeper&)=delete;
    SdlBeeper& operator=(const SdlBeeper&)=delete;
private:
    static void callback(void* userdata,Uint8* stream,int length) {
        auto& self=*static_cast<SdlBeeper*>(userdata);
        auto* samples=reinterpret_cast<std::int16_t*>(stream);
        self.device_->renderMono(samples,static_cast<std::size_t>(length)/2,
                                 self.sampleRate_);
    }
    std::shared_ptr<BeeperDevice> device_;
    SDL_AudioDeviceID id_=0;
    unsigned sampleRate_=0;
    bool audioInitialized_=false;
};
