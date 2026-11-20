// Run CP/M-format 8080 CPU diagnostics on the actual V20 interpreter.
// No CP/M, Z80 core, firmware files or device timing are required.
#include "Main8088Board.h"
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t base=0x20000;
constexpr std::uint64_t defaultLimit=20000000000ULL;
struct Result {
    std::string output;
    std::uint64_t instructions=0;
};

Result run(const std::vector<BYTE>& image, std::uint64_t limit,
           std::ostream* console=nullptr)
{
    // Leave the top 256 bytes for the initial CP/M stack.
    if(image.empty() || image.size()>0xfe00)
        throw std::runtime_error("COM size must be 1..65024 bytes");
    Main8088Board board(CpuType::NECV20);
    Main8088Bus::Rom rom{};
    // Reset -> 1000:0000, native setup, then BRKEM vector 40h.
    rom[0xff0]=0xea; rom[0xff3]=0x00; rom[0xff4]=0x10;
    board.bus().loadBootRom(rom);
    board.reset();
    auto& bus=board.bus();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    const BYTE native[]={
        0xbc,0x00,0x80,       // MOV SP,8000h (SS=0 after reset)
        0xb8,0x00,0x20,       // MOV AX,2000h
        0x8e,0xd8,            // MOV DS,AX
        0xbd,0xfe,0xff,       // MOV BP,FFFEh (8080 SP)
        0x31,0xc0,            // XOR AX,AX
        0x0f,0xff,0x40,       // BRKEM 40h
        0xf4
    };
    for(unsigned i=0;i<sizeof(native);++i)bus.writeMemory(0x10000+i,native[i]);
    bus.writeMemory(0x100,0x00); bus.writeMemory(0x101,0x01);
    bus.writeMemory(0x102,0x00); bus.writeMemory(0x103,0x20);
    // A return from the COM entry point goes to 0000h. BDOS calls
    // execute a real 8080 RET after the host supplies console output.
    bus.writeMemory(base,0x76);
    // CP/M programs sometimes inspect the BDOS address at 0006h.
    // Use a conventional JMP vector to a stub above the TPA instead.
    bus.writeMemory(base+5,0xc3);
    bus.writeMemory(base+6,0x00);
    bus.writeMemory(base+7,0xff);
    bus.writeMemory(base+0xff00,0xc9);
    bus.writeMemory(base+0xfffe,0);
    bus.writeMemory(base+0xffff,0);
    for(std::size_t i=0;i<image.size();++i)bus.writeMemory(base+0x100+i,image[i]);
    for(unsigned i=0;i<16 && !cpu.emulation8080();++i)cpu.executeOne();
    if(!cpu.emulation8080() || cpu.cs()!=0x2000 || cpu.ip()!=0x100 ||
       cpu.segment(3)!=0x2000 || cpu.reg(5)!=0xfffe)
        throw std::runtime_error("BRKEM bootstrap failed");

    Result result;
    auto emit=[&](BYTE ch) {
        if(console) {console->put(static_cast<char>(ch)); console->flush();}
        else result.output.push_back(static_cast<char>(ch));
    };
    for(;result.instructions<limit;) {
        if(!cpu.emulation8080() || cpu.cs()!=0x2000 || cpu.segment(3)!=0x2000)
            throw std::runtime_error("unexpected departure from 8080 COM context");
        if(cpu.ip()==0)return result; // JMP 0 / RET to CP/M warm boot
        if(cpu.ip()==0xff00) {
            const BYTE function=BYTE(cpu.reg(1));
            if(function==0)return result;
            if(function==2)emit(BYTE(cpu.reg(2)));
            else if(function==9) {
                WORD p=cpu.reg(2);
                bool terminated=false;
                for(unsigned n=0;n<65536;++n,++p) {
                    const BYTE ch=bus.readMemory(base+p);
                    if(ch=='$'){terminated=true;break;}
                    emit(ch);
                }
                if(!terminated)throw std::runtime_error("unterminated BDOS 9 string");
            } else throw std::runtime_error("unsupported BDOS function "+std::to_string(function));
        }
        const WORD ip=cpu.ip();
        try {cpu.executeOne();}
        catch(const std::exception& e) {
            std::cerr<<"8080 PC="<<std::hex<<std::setw(4)<<std::setfill('0')
                     <<ip<<" opcode="<<unsigned(bus.readMemory(base+ip))
                     <<std::dec<<"\n";
            throw;
        }
        ++result.instructions;
        if(cpu.halted())throw std::runtime_error("unexpected 8080 HLT");
    }
    std::cerr<<"PC="<<std::hex<<cpu.ip()<<" A="<<(cpu.reg(0)&255)
             <<" BC="<<cpu.reg(1)<<" DE="<<cpu.reg(2)<<" HL="<<cpu.reg(3)
             <<" SP="<<cpu.reg(5)<<" FLAGS="<<cpu.flags()<<std::dec<<"\n";
    throw std::runtime_error("instruction limit exceeded");
}

void selfTest()
{
    // LXI D,0110 / MVI C,9 / CALL 5 / MVI E,'!' /
    // MVI C,2 / CALL 5 / RET / "OK$".
    const std::vector<BYTE> program={
        0x11,0x10,0x01,0x0e,0x09,0xcd,0x05,0x00,
        0x1e,0x21,0x0e,0x02,0xcd,0x05,0x00,0xc9,
        'O','K','$'
    };
    if(run(program,100).output!="OK!")throw std::runtime_error("BDOS output smoke failed");
    if(run({0x0e,0x00,0xcd,0x05,0x00},100).instructions!=3)
        throw std::runtime_error("BDOS 0 smoke failed");
    if(run({0xc3,0,0},100).instructions!=1)
        throw std::runtime_error("warm boot smoke failed");
    auto expectError=[](const std::vector<BYTE>& code,const std::string& expected) {
        try {run(code,10);}
        catch(const std::runtime_error& e) {
            if(std::string(e.what()).find(expected)!=std::string::npos)return;
            throw;
        }
        throw std::runtime_error("missing expected error: "+expected);
    };
    expectError({0xc3,0,1},"instruction limit");
    expectError({0x76},"HLT");
    expectError({0x0e,0xff,0xcd,5,0},"unsupported BDOS");
    std::cout<<"PASS: V20 8080 COM bootstrap, BDOS output, return and error paths\n";
}
}

int main(int argc,char** argv)
{
    try {
        if(argc==2 && std::string(argv[1])=="--self-test"){selfTest();return 0;}
        if(argc==2 && std::string(argv[1])=="--help"){
            std::cout<<"usage: fm11-v20-8080-diag [--max-instructions N] [--expect TEXT] FILE.COM\n";
            return 0;
        }
        std::uint64_t limit=defaultLimit;
        std::string path,expected;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--max-instructions" && i+1<argc) {
                const std::string n=argv[++i];
                std::size_t used=0;
                if(n.empty() || n.find_first_not_of("0123456789")!=std::string::npos)
                    throw std::runtime_error("invalid instruction limit");
                limit=std::stoull(n,&used);
                if(!limit || used!=n.size())throw std::runtime_error("invalid instruction limit");
            } else if(arg=="--expect" && i+1<argc) {
                expected=argv[++i];
                if(expected.empty())throw std::runtime_error("empty expected output");
            } else if(!arg.empty() && arg[0]!='-' && path.empty())path=arg;
            else throw std::runtime_error("invalid argument: "+arg);
        }
        if(path.empty())throw std::runtime_error("COM file required (see --help)");
        std::ifstream input(path,std::ios::binary);
        if(!input)throw std::runtime_error("cannot open "+path);
        const std::vector<BYTE> image((std::istreambuf_iterator<char>(input)),
                                      std::istreambuf_iterator<char>());
        const auto result=run(image,limit,expected.empty()?&std::cout:nullptr);
        if(!expected.empty())std::cout<<result.output;
        std::cerr<<"\nCompleted: "<<result.instructions<<" 8080 instructions\n";
        if(!expected.empty() && result.output.find(expected)==std::string::npos) {
            std::cerr<<"Expected output not found: "<<expected<<"\n";
            return 1;
        }
        // A normal exit alone does not establish that a diagnostic passed.
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"V20 8080 diagnostic: "<<e.what()<<"\n";
        return 2;
    }
}
