#include "Main8088Board.h"
int main()
{
    Main8088Bus::Rom rom{};
    rom[0xff0]=0xea;rom[0xff1]=0;rom[0xff2]=0;
    rom[0xff3]=0;rom[0xff4]=0x10;
    Main8088Board board(CpuType::NECV20);
    board.bus().loadBootRom(rom);
    const BYTE native[]={
        0xbc,0x00,0x80, // native SP=8000
        0xb8,0x00,0x20,0x8e,0xd8, // DS=2000 for 8080 data
        0x0f,0xff,0x40, // BRKEM
        0xf4
    };
    for(unsigned i=0;i<sizeof(native);++i)
        board.bus().writeMemory(0x10000+i,native[i]);
    board.bus().writeMemory(0x100,0);
    board.bus().writeMemory(0x101,1);
    board.bus().writeMemory(0x102,0);
    board.bus().writeMemory(0x103,3); // CS=0300
    const BYTE code[]={
        0x21,0x00,0x02, // LXI H,0200
        0x06,0x5a,      // MVI B,5A
        0x78,           // MOV A,B
        0x77,           // MOV M,A
        0x3e,0x00,      // MVI A,00
        0x7e,           // MOV A,M
        0x32,0x01,0x02, // STA 0201
        0x3e,0x33,      // MVI A,33
        0x3a,0x01,0x02, // LDA 0201
        0x31,0x34,0x12, // LXI SP,1234 -> BP
        0xed,0xfd       // RETEM
    };
    for(unsigned i=0;i<sizeof(code);++i)
        board.bus().writeMemory(0x3100+i,code[i]);
    board.reset();board.cpu().executeOne();
    for(int i=0;i<5;++i)board.cpu().executeOne();
    auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
    if(!cpu.emulation8080())return 1;
    for(int i=0;i<10;++i)board.cpu().executeOne();
    if(board.bus().readMemory(0x20200)!=0x5a ||
       board.bus().readMemory(0x20201)!=0x5a ||
       (cpu.reg(0)&255)!=0x5a || cpu.reg(3)!=0x0200 ||
       cpu.reg(5)!=0x1234)return 2;
    board.cpu().executeOne(); // RETEM
    if(cpu.emulation8080() || cpu.reg(4)!=0x8000)return 3;
    board.cpu().executeOne(); // native HLT
    if(!cpu.halted())return 4;
    return 0;
}
