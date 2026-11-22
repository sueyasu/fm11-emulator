// Execute every Motorola MC6809 legal opcode once in MC6809 personality.
// Cross-check against the independent Appendix C CSV, not interpreter tables.
// Variable-cycle alternatives are checked separately by focused tests.
#include "Sim09.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
struct RamCpu : CSim09 {
    std::array<BYTE,65536> ram{};
    BYTE GetMemByte(WORD a) override {return ram[a];}
    void SetMemByte(WORD a,BYTE v) override {ram[a]=v;}
};
unsigned run(unsigned opcode) {
    RamCpu cpu;
    cpu.Set6309Enabled(false);
    cpu.ram[0xfffe]=0x10;cpu.ram[0xffff]=0x00;
    cpu.SimRESET();
    cpu.regs.r.w.X=0x2000;
    cpu.regs.r.w.Y=0x2000;
    cpu.regs.r.w.U=0x2000;
    cpu.regs.r.w.S=0x4000;
    unsigned at=0x1000;
    if(opcode>255)cpu.ram[at++]=BYTE(opcode>>8);
    cpu.ram[at++]=BYTE(opcode);
    // Zero-filled operand: indexed codes use 5-bit displacement 0,X
    // (one additional clock), branches take zero displacement, stack
    // masks are zero, and RTI uses E=0.
    cpu.SimONE();
    return cpu.LastInstructionCycles();
}
bool indexed(unsigned op) {
    return (op>=0x30 && op<=0x33) ||
           (op>=0x60 && op<=0x6f) ||
           (op>=0xa0 && op<=0xaf) ||
           (op>=0xe0 && op<=0xef) ||
           ((op>=0x1000 && op<0x1200) &&
            (((op&0xf0)==0xa0) || ((op&0xf0)==0xe0)));
}
}
int main(int argc,char** argv) {
    if(argc!=2){std::cerr<<"usage: cpu6809_opcode_cycles reference.csv\n";return 2;}
    std::ifstream in(argv[1]);
    if(!in){std::cerr<<"cannot read reference CSV\n";return 2;}
    std::string line;
    unsigned count=0,failed=0,baseCount=0,p10Count=0,p11Count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#' || line=="opcode,base_cycles")continue;
        const auto comma=line.find(',');
        if(comma==std::string::npos){std::cerr<<"malformed row\n";return 2;}
        try {
            const unsigned op=unsigned(std::stoul(line.substr(0,comma),nullptr,16));
            const unsigned base=unsigned(std::stoul(line.substr(comma+1)));
            unsigned expected=base;
            if(indexed(op))expected+=1;
            // With CC=0, the even condition codes are taken and incur
            // one additional clock. LBRN (21h) is always not taken.
            if(op>=0x1021 && op<=0x102f && !(op&1))expected+=1;
            const unsigned got=run(op);
            if(expected!=got) {
                if(failed<30)
                    std::cerr<<"FAIL opcode="<<std::hex<<op<<std::dec
                             <<" expected="<<expected<<" got="<<got<<"\n";
                ++failed;
            }
            ++count;
            if(op<0x100)++baseCount;
            else if(op<0x1100)++p10Count;
            else ++p11Count;
        } catch(const std::exception& e) {
            std::cerr<<"bad reference row: "<<line<<" "<<e.what()<<"\n";
            return 2;
        }
    }
    std::cout<<"MC6809 opcode execution: "<<count
             <<" legal, base="<<baseCount<<", page10="<<p10Count
             <<", page11="<<p11Count<<", failures="<<failed<<"\n";
    return count==268 && baseCount==221 && p10Count==38 &&
           p11Count==9 && failed==0 ? 0 : 1;
}
