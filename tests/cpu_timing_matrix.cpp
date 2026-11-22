// Stage 1-A: data-driven single-instruction cycle regression.
// CSV: cpu,name,bytes,expected,status,reference
// Values marked baseline describe current tests, not independently verified datasheet timing.
#include "Main6809Board.h"
#include "Main8088Board.h"
#include "MainZ80Board.h"
#include "Sim09.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static std::vector<BYTE> decode(const std::string& hex) {
    if(hex.empty() || (hex.size()%2))throw std::runtime_error("invalid opcode hex");
    std::vector<BYTE> out;
    for(std::size_t i=0;i<hex.size();i+=2) {
        auto digit=[](char c)->int {
            if(c>='0' && c<='9')return c-'0';
            if(c>='a' && c<='f')return c-'a'+10;
            if(c>='A' && c<='F')return c-'A'+10;
            throw std::runtime_error("invalid hex digit");
        };
        out.push_back(BYTE((digit(hex[i])<<4)|digit(hex[i+1])));
    }
    return out;
}

static unsigned run(const std::string& kind,const std::vector<BYTE>& code) {
    if(kind=="6809" || kind=="6309" || kind=="6309-native") {
        Main6809Board board(kind=="6809"?CpuType::MC6809:CpuType::HD6309);
        Main6809Board::BootRomImage rom{};
        rom[0xffe]=0x10;
        rom[0xfff]=0x00;
        board.loadBootRom(rom);
        board.reset();
        for(std::size_t i=0;i<code.size();++i)
            board.writeCpu(WORD(0x1000+i),code[i]);
        if(kind=="6309-native")
            dynamic_cast<CSim09&>(board.cpu()).regs.MD=1;
        board.cpu().executeOne();
        return board.cpu().lastExecutionCycles();
    }
    if(kind=="z80") {
        MainZ80Board board;
        board.reset();
        for(std::size_t i=0;i<code.size();++i)
            board.writeMemory(WORD(i),code[i]);
        board.cpu().executeOne();
        return board.cpu().lastExecutionCycles();
    }
    if(kind=="8088" || kind=="v20") {
        Main8088Board board(kind=="8088"?CpuType::I8088:CpuType::NECV20);
        Main8088Bus::Rom rom{};
        rom.fill(0x90);
        if(code.size()>16)throw std::runtime_error("8088 test instruction too long");
        for(std::size_t i=0;i<code.size();++i)
            rom[0xff0+i]=code[i];
        board.bus().loadBootRom(rom);
        board.reset();
        board.cpu().executeOne();
        return board.cpu().lastExecutionCycles();
    }
    throw std::runtime_error("unknown CPU: "+kind);
}

int main(int argc,char** argv) {
    if(argc!=2) {std::cerr<<"usage: cpu_timing_matrix CASES.csv\n";return 2;}
    std::ifstream in(argv[1]);
    if(!in) {std::cerr<<"cannot open "<<argv[1]<<"\n";return 2;}
    std::string line;
    unsigned count=0, failures=0, lineno=0;
    while(std::getline(in,line)) {
        ++lineno;
        if(line.empty() || line[0]=='#' ||
           line.rfind("cpu,",0)==0)continue;
        try {
            std::stringstream stream(line);
            std::vector<std::string> cols;
            std::string part;
            while(std::getline(stream,part,','))cols.push_back(part);
            if(cols.size()!=6)throw std::runtime_error("expected six CSV columns");
            const auto& cpu=cols[0];
            const auto& name=cols[1];
            const auto& status=cols[4];
            if(status!="baseline" && status!="verified")
                throw std::runtime_error("status must be baseline or verified");
            if(cols[5].empty())throw std::runtime_error("missing reference");
            const auto expected=std::stoul(cols[3],nullptr,10);
            const auto actual=run(cpu,decode(cols[2]));
            ++count;
            if(actual!=expected) {
                ++failures;
                std::cerr<<"FAIL "<<cpu<<" "<<name<<" expected="<<expected
                         <<" actual="<<actual<<" status="<<status<<"\n";
            } else {
                std::cout<<"PASS "<<cpu<<" "<<name<<" cycles="<<actual
                         <<" status="<<status<<"\n";
            }
        } catch(const std::exception& e) {
            ++failures;
            std::cerr<<"FAIL line "<<lineno<<": "<<e.what()<<"\n";
        }
    }
    if(!count) {std::cerr<<"no cases\n";return 2;}
    std::cout<<"Timing matrix: "<<count<<" cases, "<<failures<<" failures\n";
    return failures?1:0;
}
