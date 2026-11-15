// SingleStepTests/8088 v2 hardware-generated instruction conformance adapter.
// Input is a small binary stream prepared by tests/run_8088_singlestep.py.
// The ordinary FM-11 machine, ROM overlay and bus remain unchanged.
#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using U8=std::uint8_t;
using U16=std::uint16_t;
using U32=std::uint32_t;
U8 u8(std::istream& in) {
    const auto v=in.get();
    if(v==std::char_traits<char>::eof())throw std::runtime_error("truncated input");
    return U8(v);
}
U16 u16(std::istream& in) {const U16 l=u8(in);return U16(l|(U16(u8(in))<<8));}
U32 u32(std::istream& in) {const U32 a=u16(in);return a|(U32(u16(in))<<16);}
std::string hex(unsigned x,unsigned width=4) {
    static constexpr char digits[]="0123456789ABCDEF";
    std::string s(width,'0');
    for(unsigned i=0;i<width;++i){s[width-1-i]=digits[x&15];x>>=4;}
    return s;
}
// Binary record: initial/expected 14 words in AX BX CX DX SP BP SI DI
// ES CS SS DS IP FLAGS order; initial/final memory byte pairs.
struct Registers {
    std::array<WORD,8> g{};
    std::array<WORD,4> s{};
    WORD ip=0,flags=0;
};
Registers readRegs(std::istream& in) {
    Registers r;
    for(auto& x:r.g)x=u16(in);
    for(auto& x:r.s)x=u16(in);
    r.ip=u16(in);r.flags=u16(in);
    return r;
}
void loadRam(std::istream& in,Main8088Bus& bus) {
    const U32 count=u32(in);
    if(count>1000000)throw std::runtime_error("unreasonable memory entry count");
    for(U32 i=0;i<count;++i){
        const U32 address=u32(in);
        const U8 byte=u8(in);
        if(address>=0x100000)throw std::runtime_error("invalid RAM address");
        bus.writeMemory(address,byte);
    }
}
struct MemByte {U32 addr;U8 value;};
bool checkOne(std::istream& in,Main8088Board& board,U32 testcase,
              unsigned& failures,unsigned& unsupported,unsigned& passed) {
    const Registers initial=readRegs(in),expected=readRegs(in);
    const U16 flagMask=u16(in);
    const U8 isRepeat=u8(in);
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    auto& bus=board.bus();
    bus.clearConformanceRam();
    loadRam(in,bus);
    const U32 expectedCount=u32(in);
    if(expectedCount>1000000)throw std::runtime_error("unreasonable final RAM count");
    std::vector<MemByte> memory;
    memory.reserve(expectedCount);
    for(U32 i=0;i<expectedCount;++i){
        const U32 a=u32(in);
        const U8 v=u8(in);
        if(a>=0x100000)throw std::runtime_error("invalid final RAM address");
        memory.push_back({a,v});
    }
    cpu.setConformanceState(initial.g,initial.s,initial.ip,initial.flags);
    try {
        // REP string instructions execute one micro-iteration per executeOne.
        // Other instructions, including branches back to their own PC, run once.
        unsigned executed=0;
        const U32 start=cpu.pc();
        do {
            cpu.executeOne();
            ++executed;
            if(executed>1000)throw std::runtime_error("REP exceeded 1000 iterations");
        } while(isRepeat && cpu.pc()==start && !cpu.halted());
    } catch(const std::exception& e) {
        ++unsupported;
        if(unsupported<=8)std::cout<<"UNSUPPORTED #"<<testcase<<": "<<e.what()<<"\n";
        return false;
    }
    const char* general[]={"AX","BX","CX","DX","SP","BP","SI","DI"};
    const char* segment[]={"ES","CS","SS","DS"};
    std::string problem;
    for(unsigned i=0;i<8;++i)if(cpu.reg(i)!=expected.g[i]){
        problem=std::string(general[i])+" "+hex(cpu.reg(i))+" != "+hex(expected.g[i]);break;
    }
    if(problem.empty())for(unsigned i=0;i<4;++i)
        if(cpu.segment(i)!=expected.s[i]) {
            problem=std::string(segment[i])+" "+hex(cpu.segment(i))+" != "+hex(expected.s[i]);break;
        }
    if(problem.empty() && cpu.ip()!=expected.ip)
        problem="IP "+hex(cpu.ip())+" != "+hex(expected.ip);
    // Defined arithmetic/control bits only, as filtered by metadata.json.
    constexpr U16 DEFINED=0x0fd5;
    const U16 mask=flagMask&DEFINED;
    if(problem.empty() && ((cpu.flags()^expected.flags)&mask))
        problem="FLAGS "+hex(cpu.flags()&mask)+" != "+hex(expected.flags&mask);
    if(problem.empty())for(const auto& m:memory) {
        const U8 actual=bus.readRam(m.addr);
        if(actual!=m.value) {
            problem="RAM["+hex(m.addr,5)+"] "+hex(actual,2)+" != "+hex(m.value,2);
            break;
        }
    }
    if(!problem.empty()) {
        ++failures;
        if(failures<=12)std::cout<<"FAIL #"<<testcase<<": "<<problem<<"\n";
    } else ++passed;
    return problem.empty();
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2){std::cerr<<"usage: fm11-8088-singlestep INPUT.bin\n";return 2;}
        std::ifstream in(argv[1],std::ios::binary);
        if(!in)throw std::runtime_error("cannot open test stream");
        const std::string magic="SST8088A";
        for(char c:magic)if(u8(in)!=U8(c))throw std::runtime_error("invalid test stream magic");
        const U32 total=u32(in);
        Main8088Board board(CpuType::I8088);
        board.bus().enableConformanceRam();
        unsigned failures=0,unsupported=0,passed=0;
        for(U32 i=0;i<total;++i)checkOne(in,board,i,failures,unsupported,passed);
        std::cout<<"TOTAL="<<total<<" PASS="<<passed<<" FAIL="<<failures
                 <<" UNSUPPORTED="<<unsupported<<"\n";
        // Nonzero exit status makes the runner suitable for CI, while a
        // --report-only option in the wrapper can gather all opcode results.
        return failures||unsupported ? 1:0;
    } catch(const std::exception& e){
        std::cerr<<"8088 conformance runner: "<<e.what()<<"\n";
        return 2;
    }
}
