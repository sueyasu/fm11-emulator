#pragma once
#include <chrono>
#include <cstdint>

class CpuSpeedMeter {
public:
    enum class Card { Main6809, Z80, Main8088 };
    struct Counter {
        std::uint64_t cycles=0;
        std::uint64_t instructions=0;
    };
    struct Sample {
        double wallSeconds=0;
        Counter main6809, z80, main8088;
        double mhz(Card card) const {
            if(wallSeconds<=0)return 0;
            const Counter& c=card==Card::Z80?z80:
                card==Card::Main8088?main8088:main6809;
            return static_cast<double>(c.cycles)/wallSeconds/1000000.0;
        }
        double virtualSeconds(std::uint64_t hz6809,std::uint64_t hzZ80,
                              std::uint64_t hz8088=0) const {
            return (hz6809?static_cast<double>(main6809.cycles)/hz6809:0.0)+
                   (hzZ80?static_cast<double>(z80.cycles)/hzZ80:0.0)+
                   (hz8088?static_cast<double>(main8088.cycles)/hz8088:0.0);
        }
        double progressRatio(std::uint64_t hz6809,std::uint64_t hzZ80,
                             std::uint64_t hz8088=0) const {
            return wallSeconds>0?
                virtualSeconds(hz6809,hzZ80,hz8088)/wallSeconds:0;
        }
    };
    void record(Card card,std::uint64_t cycles) {
        Counter& counter=card==Card::Z80?z80_:
            card==Card::Main8088?main8088_:main6809_;
        counter.cycles+=cycles;
        ++counter.instructions;
    }
    Sample take(std::chrono::steady_clock::duration interval) {
        Sample result;
        result.wallSeconds=std::chrono::duration<double>(interval).count();
        result.main6809=main6809_;
        result.z80=z80_;
        result.main8088=main8088_;
        reset();
        return result;
    }
    void reset() {main6809_={};z80_={};main8088_={};}
private:
    Counter main6809_,z80_,main8088_;
};
