#include "MainZ80Board.h"
#include "Machine.h"
#include "SimZ80.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace {
bool run(MainZ80Board& z,const std::vector<BYTE>& program,
         const std::vector<std::uint64_t>& cycles,const char* name) {
    z.reset();
    for(std::size_t i=0;i<program.size();++i)
        z.writeMemory(static_cast<WORD>(i),program[i]);
    for(std::size_t i=0;i<cycles.size();++i) {
        const auto pc=z.cpu().pc();
        z.cpu().executeOne();
        const auto got=z.cpu().lastExecutionCycles();
        if(got!=cycles[i]) {
            std::cerr<<name<<" at instruction "<<i
                     <<" PC="<<std::hex<<pc<<std::dec
                     <<" expected "<<cycles[i]<<" got "<<got<<"\n";
            return false;
        }
    }
    return true;
}
}

class PortZ80 final : public CSimZ80 {
public:
    std::array<BYTE,65536> ram{};
    std::vector<BYTE> transmitted;
    BYTE GetMem(WORD address) override {return ram[address];}
    void SetMem(WORD address,BYTE value) override {ram[address]=value;}
    void SetIO(BYTE, BYTE data) override {transmitted.push_back(data);}
};

int main() {
    MainZ80Board z;
    if(!run(z,{0x00,0x06,0x02,0x10,0xfe},
               {4,7,13,8,4},"DJNZ"))return 1;
    // The last instruction is a NOP after DJNZ falls through.
    if(!run(z,{0x3e,0x00,0xaf,0x20,0x00,0x28,0x00},
               {7,4,7,12},"JR condition"))return 2;

    z.reset();
    z.writeMemory(0x10,0xc9); // RET
    z.writeMemory(0,0x31);z.writeMemory(1,0x00);z.writeMemory(2,0x20);
    z.writeMemory(3,0xcd);z.writeMemory(4,0x10);z.writeMemory(5,0x00);
    for(const auto expected:{10ull,17ull,10ull}) {
        z.cpu().executeOne();
        if(z.cpu().lastExecutionCycles()!=expected)return 3;
    }
    z.writeMemory(0x2000,0x81);
    if(!run(z,{0x21,0x00,0x20,0xcb,0x46,0xcb,0x06},
               {10,12,15},"CB memory"))return 4;
    if(z.readMemory(0x2000)!=0x03)return 5;

    z.writeMemory(0x2000,1);
    z.writeMemory(0x2001,2);
    z.writeMemory(0x2002,3);
    if(!run(z,{0x21,0x00,0x20,0x11,0x00,0x21,0x01,0x03,0x00,
               0xed,0xb0},{10,10,10,58},"LDIR"))return 6;
    if(z.readMemory(0x2100)!=1 || z.readMemory(0x2101)!=2 ||
       z.readMemory(0x2102)!=3)return 7;
    if(!run(z,{0x3e,0x02,0x21,0x00,0x21,0x01,0x03,0x00,0xed,0xb1},
               {7,10,10,37},"CPIR match second"))return 8;

    if(!run(z,{0xdd,0x21,0x00,0x20,0xdd,0x36,0x01,0x5a,
               0xdd,0xcb,0x01,0x46,0xdd,0xcb,0x01,0xc6},
               {14,19,20,23},"DD/CB indexed"))return 9;
    if(z.readMemory(0x2001)!=0x5b)return 10;

    if(!run(z,{0xfd,0x21,0x00,0x20,0xfd,0x36,0x01,0x22},
               {14,19},"FD indexed"))return 11;
    if(z.readMemory(0x2001)!=0x22)return 12;

    // The machine scheduler must receive the measured time from its Z80 card.
    Machine machine;
    auto card=std::make_unique<MainZ80Board>();
    auto* cpu=card.get();
    machine.addBoard(std::move(card));
    cpu->writeMemory(0,0x01);cpu->writeMemory(1,0x34);cpu->writeMemory(2,0x12);
    machine.reset();
    machine.executeOne();
    if(machine.lastExecutionCycles()!=10 || cpu->cpu().pc()!=3)return 13;

    // OUTD transfers exactly one descending byte, while OTDR transfers
    // B descending bytes. The old interpreter fell through from OTDR to
    // OUTD and incorrectly repeated OUTD as well.
    PortZ80 port;
    port.ram[0]=0xed;port.ram[1]=0xab; // OUTD
    port.ram[0x2000]=0x5a;port.ram[0x1fff]=0x7c;
    port.regs.r.w.HL=0x2000;
    port.regs.r.b.B=2;
    port.regs.r.b.C=0x40;
    port.z80emu();
    if(port.transmitted!=std::vector<BYTE>{0x5a} ||
       port.regs.r.w.HL!=0x1fff || port.regs.r.b.B!=1 ||
       port.LastInstructionCycles()!=16)return 14;

    port.regs.PC=0;
    port.ram[1]=0xbb; // OTDR
    port.regs.r.w.HL=0x2000;
    port.regs.r.b.B=2;
    port.transmitted.clear();
    port.z80emu();
    if(port.transmitted!=std::vector<BYTE>{0x5a,0x7c} ||
       port.regs.r.w.HL!=0x1ffe || port.regs.r.b.B!=0 ||
       port.LastInstructionCycles()!=37)return 15;
    return 0;
}
