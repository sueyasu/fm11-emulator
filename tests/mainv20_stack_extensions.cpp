#include "Main8088Board.h"
#include <stdexcept>

int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE program[]={
        0xbc,0x00,0x80,       // MOV SP,8000
        0xb8,0x34,0x12,       // MOV AX,1234
        0xbb,0x78,0x56,       // MOV BX,5678
        0x60,                 // PUSHA
        0xb8,0x00,0x00,       // MOV AX,0
        0xbb,0x00,0x00,       // MOV BX,0
        0x61,                 // POPA
        0x68,0xcd,0xab,       // PUSH ABCD
        0x6a,0xfe,            // PUSH -2
        0x5a,                 // POP DX => FFFE
        0x59,                 // POP CX => ABCD
        0xf4
    };
    for(unsigned i=0;i<sizeof(program);++i)
        board.bus().writeMemory(0x10000+i,program[i]);
    board.reset();board.cpu().executeOne();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    for(int i=0;i<3;++i)board.cpu().executeOne();
    const WORD initialFlags=cpu.flags();
    board.cpu().executeOne(); // PUSHA
    if(cpu.reg(4)!=0x7ff0)return 1;
    if(board.bus().readMemory(0x7ff6)!=0x00 ||
       board.bus().readMemory(0x7ff7)!=0x80)return 2; // original SP
    board.cpu().executeOne();board.cpu().executeOne(); // zero AX,BX
    board.cpu().executeOne(); // POPA
    if(cpu.reg(0)!=0x1234 || cpu.reg(3)!=0x5678 ||
       cpu.reg(4)!=0x8000 || cpu.flags()!=initialFlags)return 3;
    for(int i=0;i<4;++i)board.cpu().executeOne();
    if(cpu.reg(2)!=0xfffe || cpu.reg(1)!=0xabcd ||
       cpu.reg(4)!=0x8000 || cpu.flags()!=initialFlags)return 4;
    board.cpu().executeOne();
    if(!cpu.halted())return 5;
    // The 8088 must not accidentally accept the V20 extensions.
    Main8088Board intel(CpuType::I8088);
    intel.bus().loadBootRom(rom);
    intel.bus().writeMemory(0x10000,0x60);
    intel.reset();intel.cpu().executeOne();
    try {
        intel.cpu().executeOne();
    } catch(const std::runtime_error&) {
        return 0;
    }
    return 6;
}
