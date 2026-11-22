#include "Main8088Board.h"
#include <initializer_list>
#include <iostream>

// V20 8080-mode instruction-level timing estimate regression.
// The expected clocks are from the documented microcode-family
// approximation, not independently measured hardware timings.
namespace {
struct Case {
    const char* label;
    std::initializer_list<BYTE> code;
    unsigned clocks;
    WORD expectedIp=0;
};
bool run(const Case& tc) {
    Main8088Board board(CpuType::NECV20);
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea; rom[0xff1]=0; rom[0xff2]=0;
    rom[0xff3]=0; rom[0xff4]=0x10;
    board.bus().loadBootRom(rom);
    const BYTE native[]={
        0xbc,0x00,0x80,      // SP=8000h for native frame
        0xb8,0x00,0x20,      // AX=2000h
        0x8e,0xd8,           // DS=2000h
        0xbd,0x00,0x40,      // BP=4000h: 8080 stack
        0x0f,0xff,0x40,0xf4  // BRKEM vector 40h
    };
    for(unsigned i=0;i<sizeof(native);++i)
        board.bus().writeMemory(0x10000+i,native[i]);
    // Vector 40h -> CS:IP=0300:0100, so code executes in 8080 mode.
    board.bus().writeMemory(0x100,0x00);
    board.bus().writeMemory(0x101,0x01);
    board.bus().writeMemory(0x102,0x00);
    board.bus().writeMemory(0x103,0x03);
    unsigned length=0;
    for(BYTE b:tc.code)board.bus().writeMemory(0x3100+length++,b);
    // Memory operands and taken RET use this 8080 data stack.
    board.bus().writeMemory(0x20000,0x27);
    board.bus().writeMemory(0x24000,0x30);
    board.bus().writeMemory(0x24001,0x01);
    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<6;++i)cpu.executeOne(); // reset far JMP and BRKEM
    if(!cpu.emulation8080() || cpu.ip()!=0x100) {
        std::cerr<<"BRKEM bootstrap failed in "<<tc.label<<"\n";
        return false;
    }
    cpu.executeOne();
    const WORD expectedIp=(tc.expectedIp || tc.code.begin()[0]==0xe9 ||
        tc.code.begin()[0]==0xc7)?tc.expectedIp:WORD(0x100+length);
    if(cpu.lastExecutionCycles()!=tc.clocks || cpu.ip()!=expectedIp) {
        std::cerr<<tc.label<<" cycles="<<cpu.lastExecutionCycles()
                 <<" expected="<<tc.clocks
                 <<" IP="<<cpu.ip()<<" expected="<<expectedIp<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    const Case cases[]={
        {"NOP",{0x00},3},
        {"NOP alias",{0x08},3},
        {"MVI B",{0x06,0x12},5},
        {"LXI H",{0x21,0x34,0x12},7},
        {"MOV B,C",{0x41},5},
        {"MOV B,M",{0x46},9},
        {"MOV M,B",{0x70},9},
        {"ADD B",{0x80},5},
        {"ADD M",{0x86},9},
        {"ADI",{0xc6,0x01},5},
        {"INR B",{0x04},5},
        {"INR M",{0x34},13},
        {"DCR M",{0x35},13},
        {"MVI M",{0x36,0x01},10},
        {"INX H",{0x23},4},
        {"DCX H",{0x2b},4},
        {"DAD H",{0x29},5},
        {"LDA",{0x3a,0,0},13},
        {"STA",{0x32,0,0},13},
        {"LHLD",{0x2a,0,0},18},
        {"SHLD",{0x22,0,0},18},
        {"LDAX B",{0x0a},9},
        {"STAX B",{0x02},9},
        {"RLC",{0x07},6},
        {"DAA",{0x27},6},
        {"CMA",{0x2f},4},
        {"STC",{0x37},3},
        {"CMC",{0x3f},3},
        {"XCHG",{0xeb},5},
        {"XTHL",{0xe3},20},
        {"SPHL",{0xf9},3},
        {"PCHL",{0xe9},8,0x0000},
        {"PUSH B",{0xc5},15},
        {"POP B",{0xc1},15},
        {"JNZ taken",{0xc2,0x20,0x01},13,0x0120},
        {"JZ not taken",{0xca,0x20,0x01},8},
        {"CNZ taken",{0xc4,0x20,0x01},22,0x0120},
        {"CZ not taken",{0xcc,0x20,0x01},8},
        {"RNZ taken",{0xc0},17,0x0130},
        {"RZ not taken",{0xc8},6},
        {"RST 0",{0xc7},18,0x0000},
        {"JMP",{0xc3,0x20,0x01},12,0x0120},
        {"CALL",{0xcd,0x20,0x01},22,0x0120},
        {"RET",{0xc9},17,0x0130},
        {"IN",{0xdb,0x40},12},
        {"OUT",{0xd3,0x40},12},
        {"EI",{0xfb},3},
        {"DI",{0xf3},3},
        {"HLT",{0x76},3}
    };
    for(const auto& tc:cases) {
        // PCHL and RST0 intentionally use IP=0000h; check clocks
        // but allow zero in the default-value IP sentinel.
        if(!run(tc))return 1;
    }
    return 0;
}
