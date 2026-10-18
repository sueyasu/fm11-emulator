#include "Machine.h"
#include "Main8088Board.h"
#include <memory>

int main()
{
    Main8088Board board(CpuType::I8088);
    Main8088Bus::Rom rom{};
    // FFFF:0000 = FFFF0. Far jump to 1000:0000.
    rom[0xff0]=0xea;rom[0xff1]=0x00;rom[0xff2]=0x00;
    rom[0xff3]=0x00;rom[0xff4]=0x10;
    board.bus().loadBootRom(rom);
    board.bus().writeMemory(0x10000,0xb8); // MOV AX,1234h
    board.bus().writeMemory(0x10001,0x34);
    board.bus().writeMemory(0x10002,0x12);
    board.bus().writeMemory(0x10003,0x90); // NOP
    board.bus().writeMemory(0x10004,0xf4); // HLT
    board.reset();
    if(board.cpu().type()!=CpuType::I8088 || board.cpu().pc()!=0xffff0)return 1;
    board.cpu().executeOne();
    if(board.cpu().pc()!=0x10000)return 2;
    board.cpu().executeOne();
    if(static_cast<Main8088Board::Core&>(board.cpu()).reg(0)!=0x1234 || board.cpu().pc()!=0x10003)return 3;
    board.cpu().executeOne();
    board.cpu().executeOne();
    if(!static_cast<Main8088Board::Core&>(board.cpu()).halted() || board.cpu().pc()!=0x10005)return 4;
    board.cpu().executeOne();
    if(board.cpu().pc()!=0x10005)return 5;
    Main8088Board v20(CpuType::NECV20);
    if(v20.cpu().type()!=CpuType::NECV20 || v20.cpu().pc()!=0xffff0)return 6;
    Machine m;
    auto a=m.addBoard(std::make_unique<Main8088Board>(CpuType::I8088));
    auto b=m.addBoard(std::make_unique<Main8088Board>(CpuType::NECV20));
    m.setActiveBoard(b);
    if(a==b || m.activeBoard()->cpu().type()!=CpuType::NECV20)return 7;
    // Exercise near CALL/RET, stack order, PUSHF/POPF and 8-bit register moves.
    Main8088Board flow(CpuType::I8088);
    Main8088Bus::Rom fRom{};
    fRom[0xff0]=0xea; fRom[0xff1]=0x00;fRom[0xff2]=0x00;
    fRom[0xff3]=0x00;fRom[0xff4]=0x10;
    flow.bus().loadBootRom(fRom);
    const BYTE code[]={
      0xbc,0x00,0x80, // MOV SP,8000
      0xb0,0x34,      // MOV AL,34
      0xb4,0x12,      // MOV AH,12
      0x50,           // PUSH AX
      0x5b,           // POP BX
      0xe8,0x02,0x00, // CALL +2 (offset 000E)
      0xf4,           // HLT at 000C
      0x90,           // NOP at 000D
      0x9c,           // PUSHF at 000E
      0x9d,           // POPF at 000F
      0xc3            // RET at 0010
    };
    for(unsigned i=0;i<sizeof(code);++i)flow.bus().writeMemory(0x10000+i,code[i]);
    flow.reset();
    flow.cpu().executeOne(); // Far JMP
    for(unsigned i=0;i<5;++i)flow.cpu().executeOne(); // MOV SP/AL/AH, PUSH/POP
    auto& core=static_cast<Main8088Board::Core&>(flow.cpu());
    if(core.reg(0)!=0x1234 || core.reg(3)!=0x1234 || core.reg(4)!=0x8000)return 8;
    flow.cpu().executeOne(); // CALL
    if(core.ip()!=0x000e || core.reg(4)!=0x7ffe)return 9;
    if(flow.bus().readMemory(0x7ffe)!=0x0c || flow.bus().readMemory(0x7fff)!=0x00)return 10;
    flow.cpu().executeOne(); // PUSHF
    flow.cpu().executeOne(); // POPF
    flow.cpu().executeOne(); // RET
    if(core.ip()!=0x000c || core.reg(4)!=0x8000)return 11;
    flow.cpu().executeOne(); // HLT
    if(!core.halted())return 12;

    // ModR/M: direct address, register mode and BP-default SS.
    Main8088Board mov(CpuType::I8088);
    mov.bus().loadBootRom(fRom);
    // DS=1000, SS=2000, BX=0100, BP=0200, AX=1234.
    const BYTE movCode[]={
      0xb8,0x34,0x12,       // MOV AX,1234
      0xbb,0x00,0x01,       // MOV BX,0100
      0xbd,0x00,0x02,       // MOV BP,0200
      0xba,0x00,0x10,       // MOV DX,1000
      0x8e,0xda,            // MOV DS,DX
      0xba,0x00,0x20,       // MOV DX,2000
      0x8e,0xd2,            // MOV SS,DX
      0x89,0x07,            // MOV [BX],AX -> 10100
      0x8b,0x0f,            // MOV CX,[BX]
      0xc7,0x46,0xfe,0x78,0x56, // MOV [BP-2],5678 -> 201fe
      0x8b,0x56,0xfe,       // MOV DX,[BP-2]
      0x88,0xc4,            // MOV AH,AL
      0xc6,0x06,0x00,0x03,0x5a, // MOV byte [0300],5A -> 10300
      0x8a,0x1e,0x00,0x03, // MOV BL,[0300]
      0x89,0xc3,            // MOV BX,AX (reg mode)
      0xf4
    };
    for(unsigned i=0;i<sizeof(movCode);++i)mov.bus().writeMemory(0x10000+i,movCode[i]);
    mov.reset();mov.cpu().executeOne();
    auto& mc=static_cast<Main8088Board::Core&>(mov.cpu());
    for(unsigned i=0;i<8;++i)mov.cpu().executeOne(); // through MOV [BX],AX
    if(mov.bus().readMemory(0x10100)!=0x34 || mov.bus().readMemory(0x10101)!=0x12)return 13;
    mov.cpu().executeOne(); // MOV CX,[BX]
    if(mc.reg(1)!=0x1234)return 14;
    mov.cpu().executeOne(); // MOV [BP-2]
    if(mov.bus().readMemory(0x201fe)!=0x78 || mov.bus().readMemory(0x201ff)!=0x56)return 15;
    mov.cpu().executeOne(); // MOV DX,[BP-2]
    if(mc.reg(2)!=0x5678)return 16;
    mov.cpu().executeOne(); // MOV AH,AL
    if(mc.reg(0)!=0x3434)return 17;
    mov.cpu().executeOne();mov.cpu().executeOne(); // C6 and 8A
    if((mc.reg(3)&0xff)!=0x5a || mov.bus().readMemory(0x10300)!=0x5a)return 18;
    mov.cpu().executeOne(); // MOV BX,AX
    if(mc.reg(3)!=0x3434)return 19;
    mov.cpu().executeOne();
    if(!mc.halted())return 20;
    return 0;
}
