#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

// Single-channel FM-family beeper, separate from the CPU IRQ status FD03.
// FD03 write: bit 0 speaker enable, bit 7 continuous tone, bit 6
// 205 ms one-shot tone. Reference timing/bit assignment follows the
// related Fujitsu FM-7 implementation pending FM-11 source verification.
// No SDL dependency: renderMono() is deterministic and CTest-friendly.
class BeeperDevice {
public:
    static constexpr unsigned kFrequencyHz=1200;
    static constexpr unsigned kPulseMilliseconds=205;
    void reset() {
        std::lock_guard<std::mutex> g(mutex_);
        speaker_=active_=false;
        pulseFrames_=0;
        pulseRemainingNs_=0;
        pulsePending_=false;
        virtualOneShotPending_.store(false,std::memory_order_relaxed);
        sampleRate_=0;
        phase_=0;
    }
    // Host PCM frames drive timing when an output device exists; otherwise
    // virtual CPU time advances a pending one-shot (also in headless).
    void setAudioDriven(bool enabled) {
        std::lock_guard<std::mutex> g(mutex_);
        audioDriven_=enabled;
        virtualOneShotPending_.store(!enabled && active_ && pulseRemainingNs_!=0,
                                     std::memory_order_relaxed);
    }
    bool virtualOneShotPending() const {
        return virtualOneShotPending_.load(std::memory_order_relaxed);
    }
    bool audioDriven() const {
        std::lock_guard<std::mutex> g(mutex_);
        return audioDriven_;
    }
    void advanceNanoseconds(std::uint64_t ns) {
        std::lock_guard<std::mutex> g(mutex_);
        if(audioDriven_ || !active_ || !pulseRemainingNs_)return;
        if(ns>=pulseRemainingNs_) {
            pulseRemainingNs_=0;
            pulseFrames_=0;
            pulsePending_=false;
            active_=false;
            virtualOneShotPending_.store(false,std::memory_order_relaxed);
        } else pulseRemainingNs_-=ns;
    }
    void writeControl(std::uint8_t value) {
        std::lock_guard<std::mutex> g(mutex_);
        speaker_=(value&1u)!=0;
        if(!speaker_) {
            active_=false;
            pulseFrames_=0;
            pulseRemainingNs_=0;
            pulsePending_=false;
            virtualOneShotPending_.store(false,std::memory_order_relaxed);
        } else if(value&0x40u) {
            active_=true;
            pulseRemainingNs_=std::uint64_t(kPulseMilliseconds)*1000000ULL;
            virtualOneShotPending_.store(!audioDriven_,std::memory_order_relaxed);
            // The remaining one-shot duration is calculated at render rate
            // so it stays 205 ms of audible time at accelerated CPU speeds.
            pulseFrames_=sampleRate_?
                (std::uint64_t(sampleRate_)*kPulseMilliseconds+999)/1000:0;
            pulsePending_=!sampleRate_;
        } else {
            pulsePending_=false;
            pulseFrames_=0;
            pulseRemainingNs_=0;
            active_=(value&0x80u)!=0;
            virtualOneShotPending_.store(false,std::memory_order_relaxed);
        }
    }
    bool active() const {
        std::lock_guard<std::mutex> g(mutex_);
        return active_;
    }
    bool speakerEnabled() const {
        std::lock_guard<std::mutex> g(mutex_);
        return speaker_;
    }
    // Called only by the audio callback; the short mutex also serializes
    // CPU writes and RESET. No allocation or host I/O in this path.
    void renderMono(std::int16_t* samples,std::size_t count,unsigned rate) {
        std::lock_guard<std::mutex> g(mutex_);
        if(!rate){std::fill_n(samples,count,std::int16_t{0});return;}
        if(sampleRate_!=rate) {
            if(pulseFrames_ && sampleRate_)
                pulseFrames_=(pulseFrames_*rate+sampleRate_-1)/sampleRate_;
            sampleRate_=rate;
        }
        if(pulsePending_) {
            pulseFrames_=(std::uint64_t(rate)*kPulseMilliseconds+999)/1000;
            pulsePending_=false;
        }
        for(std::size_t i=0;i<count;++i) {
            if(!speaker_ || !active_)samples[i]=0;
            else {
                samples[i]=phase_<rate/2?5000:-5000;
                phase_+=kFrequencyHz;
                if(phase_>=rate)phase_%=rate;
            }
            if(pulseFrames_ && --pulseFrames_==0) {
                active_=false;
                virtualOneShotPending_.store(false,std::memory_order_relaxed);
            }
        }
    }
private:
    mutable std::mutex mutex_;
    std::atomic<bool> virtualOneShotPending_{false};
    bool speaker_=false,active_=false,pulsePending_=false;
    bool audioDriven_=false;
    std::uint64_t pulseRemainingNs_=0;
    unsigned sampleRate_=0;
    std::uint64_t pulseFrames_=0;
    unsigned phase_=0;
};
