#pragma once
#include <chrono>
#include <cstdint>

// Counts guest main-CPU T-states against an independent host wall-clock
// interval. The display sub-CPU is not included. Record the CPU card BEFORE
// executing each instruction, since FD05 may switch it during execution.
class CpuSpeedMeter {
public:
    enum class Card { Main6809, Z80 };
    struct Counter {
        std::uint64_t cycles=0;
        std::uint64_t instructions=0;
    };
    struct Sample {
        double wallSeconds=0;
        Counter main6809, z80;
        double mhz(Card card) const {
            if(wallSeconds<=0)return 0;
            const auto& c=card==Card::Z80?z80:main6809;
            return static_cast<double>(c.cycles)/wallSeconds/1000000.0;
        }
        // Sum of the emulated main-CPU times, divided by host wall time.
        // This is not a pure processor performance measurement when the
        // emulated guest intentionally waits in SYNC/CWAI.
        double virtualSeconds(std::uint64_t hz6809,std::uint64_t hzZ80) const {
            return (hz6809?static_cast<double>(main6809.cycles)/hz6809:0.0)+
                   (hzZ80?static_cast<double>(z80.cycles)/hzZ80:0.0);
        }
        double progressRatio(std::uint64_t hz6809,std::uint64_t hzZ80) const {
            return wallSeconds>0?virtualSeconds(hz6809,hzZ80)/wallSeconds:0;
        }
    };
    void record(Card card,std::uint64_t cycles) {
        Counter& counter=card==Card::Z80?z80_:main6809_;
        counter.cycles+=cycles;
        ++counter.instructions;
    }
    Sample take(std::chrono::steady_clock::duration wallInterval) {
        Sample result;
        result.wallSeconds=std::chrono::duration<double>(wallInterval).count();
        result.main6809=main6809_;
        result.z80=z80_;
        main6809_={};
        z80_={};
        return result;
    }
    void reset() {main6809_={};z80_={};}
private:
    Counter main6809_,z80_;
};
