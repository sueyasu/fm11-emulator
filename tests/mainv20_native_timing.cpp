#include "Main8088Board.h"
#include <array>
#include <initializer_list>
#include <iostream>

// NEC uPD70108 native-mode execution clocks, Table 2-8.
// No bus event tracing, prefetch instrumentation or timing benchmarks.
namespace {
struct Case {
    const char* name;
    std::initializer_list<BYTE> code;
    unsigned clocks;
    WORD flags=2;
    WORD cx=2;
};
bool run(const Case& tc) {
    Main8088Board board(CpuType::NECV20);
    unsigned address=0x10000;
    for(BYTE b:tc.code)board.bus().writeMemory(address++,b);
    board.bus().writeMemory(0x0200,0x03);
    board.bus().writeMemory(0x0201,0x00);
    board.bus().writeMemory(0x0202,0x00);
    board.bus().writeMemory(0x0203,0x10);
    board.bus().writeMemory(0x8000,0x00);
    board.bus().writeMemory(0x8001,0x00);
    board.bus().writeMemory(0x8002,0x00);
    board.bus().writeMemory(0x8003,0x10);
    std::array<WORD,8> regs{};
    regs[0]=3;
    regs[1]=tc.cx;
    regs[3]=0x0200;
    regs[4]=0x8000;
    const std::array<WORD,4> seg{0,0x1000,0,0};
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    cpu.setConformanceState(regs,seg,0,tc.flags);
    cpu.executeOne();
    if(cpu.lastExecutionCycles()!=tc.clocks) {
        std::cerr<<tc.name<<" clocks="<<cpu.lastExecutionCycles()
                 <<" expected="<<tc.clocks<<"\n";
        return false;
    }
    return true;
}
}
int main() {
    const Case cases[]={
        {"NOP",{0x90},3},
        {"INC AX",{0x40},2},
        {"DEC AX",{0x48},2},
        {"PUSH AX",{0x50},12},
        {"POP AX",{0x58},12},
        {"PUSHF",{0x9c},12},
        {"POPF",{0x9d},12},
        {"DAA",{0x27},7},
        {"DAS",{0x2f},7},
        {"AAA",{0x37},3},
        {"AAS",{0x3f},3},
        {"SAHF",{0x9e},3},
        {"LAHF",{0x9f},2},
        {"CBW",{0x98},2},
        {"CWD",{0x99},4},
        {"ADD reg8",{0x00,0xc0},2},
        {"ADD mem8",{0x00,0x07},16},
        {"ADD mem16",{0x01,0x07},24},
        {"ADD reg-from-mem8",{0x02,0x07},11},
        {"ADD reg-from-mem16",{0x03,0x07},15},
        {"CMP reg",{0x38,0xc0},2},
        {"CMP mem8",{0x38,0x07},11},
        {"CMP mem16",{0x39,0x07},15},
        {"CMP reg-from-mem8",{0x3a,0x07},11},
        {"SUB mem8",{0x28,0x07},16},
        {"OR mem16",{0x09,0x07},24},
        {"ALU imm AL",{0x04,0x01},4},
        {"ADD imm reg8",{0x80,0xc0,0x01},4},
        {"ADD imm mem8",{0x80,0x07,0x01},18},
        {"ADD imm mem16",{0x81,0x07,0x01,0x00},26},
        {"CMP imm mem8",{0x80,0x3f,0x01},13},
        {"CMP imm mem16",{0x81,0x3f,0x01,0x00},17},
        {"TEST byte reg",{0x84,0xc0},2},
        {"TEST word reg",{0x85,0xc0},2},
        {"TEST byte mem",{0x84,0x07},10},
        {"TEST word mem",{0x85,0x07},14},
        {"TEST imm byte reg",{0xf6,0xc0,1},4},
        {"TEST imm word reg",{0xf7,0xc0,1,0},4},
        {"TEST imm byte mem",{0xf6,0x07,1},11},
        {"TEST imm word mem",{0xf7,0x07,1,0},15},
        {"MOV reg8",{0x88,0xc0},2},
        {"MOV mem8<-reg",{0x88,0x07},9},
        {"MOV mem16<-reg",{0x89,0x07},13},
        {"MOV reg<-mem8",{0x8a,0x07},11},
        {"MOV reg<-mem16",{0x8b,0x07},15},
        {"MOV imm reg",{0xb8,0x01,0x00},4},
        {"MOV imm mem8",{0xc6,0x07,0x01},11},
        {"MOV imm mem16",{0xc7,0x07,0x01,0x00},15},
        {"MOV acc<-moffs8",{0xa0,0x00,0x02},10},
        {"MOV acc<-moffs16",{0xa1,0x00,0x02},14},
        {"MOV moffs8<-acc",{0xa2,0x00,0x02},9},
        {"MOV moffs16<-acc",{0xa3,0x00,0x02},13},
        {"MOV Sreg<-mem",{0x8e,0x1f},15},
        {"MOV mem<-Sreg",{0x8c,0x07},14},
        {"LEA",{0x8d,0x07},4},
        {"LES",{0xc4,0x07},26},
        {"LDS",{0xc5,0x07},26},
        {"INC mem8",{0xfe,0x07},16},
        {"DEC mem16",{0xff,0x0f},24},
        {"NOT reg8",{0xf6,0xd0},2},
        {"NEG mem8",{0xf6,0x1f},16},
        {"NOT mem16",{0xf7,0x17},24},
        {"JZ untaken",{0x74,0x00},4},
        {"JZ taken",{0x74,0x00},14,0x42},
        {"LOOP taken",{0xe2,0x00},13},
        {"LOOP not taken",{0xe2,0x00},5,2,1},
        {"LOOPE taken",{0xe1,0x00},14,0x42},
        {"LOOPE not taken",{0xe1,0x00},5},
        {"JCXZ taken",{0xe3,0x00},13,2,0},
        {"JCXZ not taken",{0xe3,0x00},5},
        {"IN AL,imm",{0xe4,0x20},9},
        {"IN AX,imm",{0xe5,0x20},13},
        {"IN AL,DX",{0xec},8},
        {"IN AX,DX",{0xed},12},
        {"OUT imm,AL",{0xe6,0x20},8},
        {"OUT imm,AX",{0xe7,0x20},12},
        {"OUT DX,AL",{0xee},8},
        {"OUT DX,AX",{0xef},12},
        {"JMP short",{0xeb,0x00},12},
        {"JMP near",{0xe9,0x00,0x00},13},
        {"JMP far",{0xea,0x00,0x00,0x00,0x10},15},
        {"CALL near",{0xe8,0x00,0x00},20},
        {"CALL far",{0x9a,0x00,0x00,0x00,0x10},29},
        {"RET near",{0xc3},19},
        {"RET far",{0xcb},29},
        {"RET near imm",{0xc2,0x02,0x00},24},
        {"RET far imm",{0xca,0x02,0x00},32},
        {"CLC",{0xf8},2},
        {"STC",{0xf9},2},
        {"HLT",{0xf4},2}
    };
    for(const auto& tc:cases)if(!run(tc))return 1;
    return 0;
}
