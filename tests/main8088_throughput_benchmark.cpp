#include "Main8088Board.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

// Explicit local microbenchmark, NOT a CTest pass/fail timing gate.
// NOP fills the entire 64-KiB code segment to avoid IP wraparound
// changing the workload. Compare the same instruction count with
// optional diagnostics disabled, prefetch shadow, and bus tracing.
namespace {
struct Measurement {
    const char* label;
    double seconds;
    std::uint64_t cycles;
    WORD finalIp;
    WORD ax;
};
Measurement run(const char* label,std::uint64_t count,
                bool prefetch,bool events) {
    Main8088Board board(CpuType::I8088);
    for(unsigned i=0;i<65536;++i)
        board.bus().writeMemory(0x10000+i,0x90); // NOP
    std::array<WORD,8> regs{};
    regs[0]=0x1234;
    regs[4]=0x8000;
    const std::array<WORD,4> segments{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,segments,0,2);
    cpu.setPrefetchTrackingEnabled(prefetch);
    cpu.setBusEventTrackingEnabled(events);
    const auto begin=std::chrono::steady_clock::now();
    std::uint64_t cycles=0;
    for(std::uint64_t i=0;i<count;++i) {
        cpu.executeOne();
        cycles+=cpu.lastExecutionCycles();
    }
    const auto end=std::chrono::steady_clock::now();
    const auto elapsed=std::chrono::duration<double>(end-begin).count();
    return {label,elapsed,cycles,cpu.ip(),cpu.reg(0)};
}
void usage(const char* executable) {
    std::cerr<<"Usage: "<<executable<<" [instruction-count]\\n"
             <<"  Default: 1000000; explicit benchmark, not CTest.\\n";
}
}
int main(int argc,char** argv) {
    if(argc>2){usage(argv[0]);return 2;}
    std::uint64_t iterations=1000000;
    if(argc==2) {
        try {
            const std::string argument(argv[1]);
            std::size_t used=0;
            iterations=std::stoull(argument,&used,10);
            if(used!=argument.size())throw std::invalid_argument("trailing text");
        } catch(const std::exception&) {usage(argv[0]);return 2;}
    }
    // Account for a 3-clock NOP and IP wrapping once per 65536 bytes.
    if(iterations==0 || iterations>std::numeric_limits<std::uint64_t>::max()/3) {
        usage(argv[0]);
        return 2;
    }
    const Measurement normal=run("normal",iterations,false,false);
    const Measurement shadow=run("prefetch-shadow",iterations,true,false);
    const Measurement tracing=run("bus-events",iterations,false,true);
    const WORD ip=WORD(iterations&0xffffu);
    for(const auto& m:{normal,shadow,tracing}) {
        if(m.cycles!=iterations*3 || m.finalIp!=ip || m.ax!=0x1234) {
            std::cerr<<"Incorrect instruction result in "<<m.label
                     <<": cycles="<<m.cycles
                     <<" IP="<<m.finalIp<<" AX="<<m.ax<<"\\n";
            return 1;
        }
    }
    std::cout<<std::fixed<<std::setprecision(3)
             <<"8088 NOP throughput, "<<iterations<<" instructions\\n";
    for(const auto& m:{normal,shadow,tracing}) {
        std::cout<<std::setw(16)<<m.label<<": "
                 <<m.seconds<<" s, "
                 <<double(iterations)/m.seconds/1000000.0<<" MIPS, "
                 <<m.seconds/normal.seconds<<"x baseline time\\n";
    }
    std::cout<<"No absolute speed threshold: compare measurements on the"
                " same machine, build type and load.\\n";
    return 0;
}
