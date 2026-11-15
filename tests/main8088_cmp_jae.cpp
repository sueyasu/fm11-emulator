// Regression for the FM-11 CP/M-86 CBIOS track bound check:
//     mov bx,[cur_track] / cmp bx,80 / jae invalid
// Execute the real 8088 opcode bytes, testing memory addressing, CF/ZF,
// and both taken and not-taken JAE paths on 8088 and NEC V20.
#include "Main8088Board.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
constexpr WORD CF=0x0001, ZF=0x0040;
constexpr std::uint32_t start=0x10000;
constexpr WORD trackOffset=0x0350;
constexpr WORD trackSegment=0x0400;
constexpr std::uint32_t trackPhysical=0x4000+trackOffset;

bool check(CpuType type, WORD track, bool useMemory, bool wideImmediate)
{
    Main8088Board board(type);
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10; // JMP 1000:0000
    board.bus().loadBootRom(rom);
    board.bus().writeMemory(trackPhysical,BYTE(track));
    board.bus().writeMemory(trackPhysical+1,BYTE(track>>8));

    std::vector<BYTE> code;
    if(useMemory) {
        // MOV AX,0400 / MOV DS,AX / MOV BX,[0350]
        code={0xb8,0x00,0x04,0x8e,0xd8,
              0x8b,0x1e,BYTE(trackOffset),BYTE(trackOffset>>8)};
    } else {
        // MOV BX,imm16
        code={0xbb,BYTE(track),BYTE(track>>8)};
    }
    if(wideImmediate) {
        // CMP BX,0050 (81 FB 50 00)
        code.insert(code.end(),{0x81,0xfb,0x50,0x00});
    } else {
        // CMP BX,+80 (83 FB 50). This is the NASM encoding expected
        // for the CBIOS cmp bx,80.
        code.insert(code.end(),{0x83,0xfb,0x50});
    }
    const WORD cmpEnd=WORD(code.size());
    // JAE +6 skips MOV AX,DEAD / HLT and lands at MOV AX,1234.
    code.insert(code.end(),{0x73,0x04,0xb8,0xad,0xde,0xf4,
                            0xb8,0x34,0x12,0xf4});
    for(std::size_t i=0;i<code.size();++i)
        board.bus().writeMemory(start+std::uint32_t(i),code[i]);

    board.reset();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    board.cpu().executeOne(); // reset far JMP
    if(cpu.pc()!=start)return false;
    for(int i=0;i<(useMemory?3:1);++i)board.cpu().executeOne();
    if(cpu.reg(3)!=track)return false;
    board.cpu().executeOne(); // CMP
    if(cpu.ip()!=cmpEnd)return false;
    const bool expectedAboveOrEqual=track>=80;
    const WORD expectedFlags=WORD((expectedAboveOrEqual?0:CF)|(track==80?ZF:0));
    if((cpu.flags()&(CF|ZF))!=expectedFlags) {
        std::cerr<<"CMP flags wrong: track="<<track
                 <<" flags="<<cpu.flags()<<" expected CF/ZF="<<expectedFlags<<"\n";
        return false;
    }
    board.cpu().executeOne(); // JAE
    const WORD expectedIp=WORD(cmpEnd+(expectedAboveOrEqual?6:2));
    if(cpu.ip()!=expectedIp) {
        std::cerr<<"JAE target wrong: track="<<track<<" ip="<<cpu.ip()
                 <<" expected="<<expectedIp<<"\n";
        return false;
    }
    board.cpu().executeOne(); // MOV AX
    if(cpu.reg(0)!=(expectedAboveOrEqual?0x1234:0xdead))return false;
    board.cpu().executeOne(); // HLT
    return cpu.halted();
}
}
int main()
{
    for(auto type:{CpuType::I8088,CpuType::NECV20}) {
        for(bool memory:{false,true}) {
            for(bool wide:{false,true}) {
                for(WORD track:{WORD(0),WORD(79),WORD(80),WORD(81),WORD(0x8000)}) {
                    if(!check(type,track,memory,wide)) {
                        std::cerr<<"FAIL main8088_cmp_jae: type="
                                 <<(type==CpuType::I8088?"8088":"V20")
                                 <<" track="<<track<<" memory="<<memory
                                 <<" imm16="<<wide<<"\n";
                        return 1;
                    }
                }
            }
        }
    }
    std::cout<<"PASS: 8088/V20 MOV BX,mem / CMP BX,80 / JAE"
             <<" (40 cases, register/memory, 83/81, both paths)\n";
    return 0;
}
