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
    // AL=7Fh; ADD AL,1 sets OF/SF, while preserving correct CF and PF.
    Main8088Board arithmetic(CpuType::I8088);
    arithmetic.bus().loadBootRom(fRom);
    const BYTE arithmeticCode[]={
        0xb0,0x7f,        // MOV AL,7Fh
        0x04,0x01,        // ADD AL,1 -> 80h, OF=1
        0x70,0x02,        // JO +2, skip HLT
        0xf4,0x90,
        0x3c,0x80,        // CMP AL,80h -> ZF
        0x75,0x02,        // JNE +2 (not taken)
        0x74,0x02,        // JE +2
        0xf4,0x90,
        0x2c,0x81,        // SUB AL,81h -> FFh, CF
        0x72,0x02,        // JC +2
        0xf4,0x90,
        0x34,0xff,        // XOR AL,FFh -> 00h, ZF/PF
        0x40,             // INC AX, CF preserved
        0xf4
    };
    for(unsigned i=0;i<sizeof(arithmeticCode);++i)
        arithmetic.bus().writeMemory(0x10000+i,arithmeticCode[i]);
    arithmetic.reset();arithmetic.cpu().executeOne();
    auto& ac=static_cast<Main8088Board::Core&>(arithmetic.cpu());
    arithmetic.cpu().executeOne();arithmetic.cpu().executeOne();
    if((ac.reg(0)&0xff)!=0x80 || (ac.flags()&0x0881)!=0x0880)return 21;
    arithmetic.cpu().executeOne();
    if(ac.ip()!=8)return 22;
    arithmetic.cpu().executeOne(); // CMP
    if((ac.flags()&0x0040)==0 || (ac.reg(0)&0xff)!=0x80)return 23;
    arithmetic.cpu().executeOne(); // JNE not taken
    arithmetic.cpu().executeOne(); // JE taken
    if(ac.ip()!=16)return 24;
    arithmetic.cpu().executeOne(); // SUB
    if((ac.flags()&1)==0 || (ac.reg(0)&0xff)!=0xff)return 25;
    arithmetic.cpu().executeOne(); // JC
    if(ac.ip()!=22)return 26;
    arithmetic.cpu().executeOne(); // XOR
    if((ac.flags()&0x44)!=0x44 || (ac.reg(0)&0xff)!=0)return 27;
    arithmetic.cpu().executeOne(); // INC
    if((ac.flags()&1)!=0)return 28; // XOR cleared carry, INC kept it
    arithmetic.cpu().executeOne(); // HLT
    if(!ac.halted())return 29;
    // Group 80/81/83 and ModR/M arithmetic, including CMP (no writeback).
    Main8088Board grouped(CpuType::I8088);
    grouped.bus().loadBootRom(fRom);
    const BYTE groupCode[]={
        0xbb,0x00,0x02,            // MOV BX,0200
        0xc7,0x07,0xff,0x00,       // MOV word [BX],00FF
        0x83,0x07,0x01,            // ADD word [BX],+1 -> 0100
        0x83,0x3f,0x01,            // CMP word [BX],+1 -> CF=0, ZF=0
        0x81,0x2f,0x01,0x01,       // SUB word [BX],0101 -> FFFF
        0x81,0x3f,0xff,0xff,       // CMP word [BX],FFFF -> ZF
        0x80,0x27,0x0f,            // AND byte [BX],0F -> 0F
        0xb0,0x01,                 // MOV AL,1
        0x00,0x07,                 // ADD byte [BX],AL -> 10
        0x02,0x07,                 // ADD AL,[BX] -> 11
        0x3a,0x07,                 // CMP AL,[BX] -> ZF=0
        0xf4
    };
    for(unsigned i=0;i<sizeof(groupCode);++i)
        grouped.bus().writeMemory(0x10000+i,groupCode[i]);
    grouped.reset();grouped.cpu().executeOne();
    auto& gc=static_cast<Main8088Board::Core&>(grouped.cpu());
    for(unsigned i=0;i<3;++i)grouped.cpu().executeOne();
    if(grouped.bus().readMemory(0x200)!=0 || grouped.bus().readMemory(0x201)!=1)return 30;
    grouped.cpu().executeOne();
    if(grouped.bus().readMemory(0x200)!=0 || grouped.bus().readMemory(0x201)!=1)return 31;
    grouped.cpu().executeOne();
    if(grouped.bus().readMemory(0x200)!=0xff || grouped.bus().readMemory(0x201)!=0xff)return 32;
    grouped.cpu().executeOne();
    if(!(gc.flags()&0x40))return 33;
    grouped.cpu().executeOne();
    if(grouped.bus().readMemory(0x200)!=0x0f || grouped.bus().readMemory(0x201)!=0xff)return 34;
    grouped.cpu().executeOne();
    grouped.cpu().executeOne();
    if(grouped.bus().readMemory(0x200)!=0x10)return 35;
    grouped.cpu().executeOne();
    if((gc.reg(0)&0xff)!=0x11)return 36;
    grouped.cpu().executeOne();
    if(gc.flags()&0x40)return 37;
    grouped.cpu().executeOne();
    if(!gc.halted())return 38;
    // Verify immediate and DX port addressing, byte/word widths and
    // the side effects of the FM-11 8088 board's I/O callbacks.
    Main8088Board io(CpuType::I8088);
    io.bus().loadBootRom(fRom);
    std::array<BYTE, 0x10000> ports{};
    unsigned reads=0, writes=0;
    io.bus().setSystemIo(
        [&](WORD port) -> BYTE { ++reads; return ports[port]; },
        [&](WORD port,BYTE value) { ++writes; ports[port]=value; });
    const BYTE ioCode[]={
        0xb8,0x34,0x12,       // MOV AX,1234h
        0xe7,0x30,            // OUT 30h,AX (unmapped)
        0xba,0x18,0xfd,       // MOV DX,FD18h
        0xef,                 // OUT DX,AX to FD18/FD19
        0xb8,0x00,0x00,       // MOV AX,0
        0xed,                 // IN AX,DX
        0xb0,0x5a,           // MOV AL,5Ah
        0xee,                 // OUT DX,AL
        0xec,                 // IN AL,DX
        0xe6,0x31,           // OUT 31h,AL (unmapped)
        0xe4,0x31,           // IN AL,31h -> FFh (unmapped)
        0xf4
    };
    for(unsigned i=0;i<sizeof(ioCode);++i)
        io.bus().writeMemory(0x10000+i,ioCode[i]);
    io.reset();
    io.cpu().executeOne(); // reset far jump
    io.cpu().executeOne(); // MOV AX
    io.cpu().executeOne(); // OUT imm8,AX unmapped
    if(writes!=0)return 39;
    io.cpu().executeOne(); // MOV DX
    io.cpu().executeOne(); // OUT DX,AX
    if(writes!=2 || ports[0xfd18]!=0x34 || ports[0xfd19]!=0x12)return 40;
    io.cpu().executeOne(); // MOV AX,0
    io.cpu().executeOne(); // IN AX,DX
    auto& ic=static_cast<Main8088Board::Core&>(io.cpu());
    if(ic.reg(0)!=0x1234 || reads!=2)return 41;
    io.cpu().executeOne(); // MOV AL,5A
    io.cpu().executeOne(); // OUT DX,AL
    io.cpu().executeOne(); // IN AL,DX
    if(ports[0xfd18]!=0x5a || (ic.reg(0)&0xff)!=0x5a)return 42;
    io.cpu().executeOne(); // OUT imm8,AL
    io.cpu().executeOne(); // IN AL,imm8 from unmapped port
    if((ic.reg(0)&0xff)!=0xff || writes!=3)return 43;
    io.cpu().executeOne();
    if(!ic.halted())return 44;

    // Immediate I/O opcodes must consume their 8-bit port operand.
    // Check the independent byte/word variants against unmapped ports.
    Main8088Board immediate(CpuType::NECV20);
    immediate.bus().loadBootRom(fRom);
    const BYTE immCode[]={0xe4,0x30,0xe5,0x32,0xe6,0x33,0xe7,0x44,0xf4};
    for(unsigned i=0;i<sizeof(immCode);++i)
        immediate.bus().writeMemory(0x10000+i,immCode[i]);
    immediate.reset();immediate.cpu().executeOne();
    for(unsigned i=0;i<4;++i)immediate.cpu().executeOne();
    auto& im=static_cast<Main8088Board::Core&>(immediate.cpu());
    if(im.reg(0)!=0xffff || im.ip()!=8)return 45;
    immediate.cpu().executeOne();
    if(!im.halted())return 46;
    // Program PIC IRQ0 vector 28h, then verify STI shadow and HLT wakeup.
    Main8088Board irqBoard(CpuType::I8088);
    irqBoard.bus().loadBootRom(fRom);
    const BYTE irqCode[]={
        0xbc,0x00,0x80, // MOV SP,8000
        0xfb,           // STI: shadow protects the next instruction
        0x90,           // NOP
        0xf4,           // HLT
        0x90,           // resume after interrupt, then another HLT
        0xf4
    };
    for(unsigned i=0;i<sizeof(irqCode);++i)
        irqBoard.bus().writeMemory(0x10000+i,irqCode[i]);
    // IRQ0 handler at 2000:0000: IRET.
    irqBoard.bus().writeMemory(0x20000,0xcf);
    irqBoard.bus().writeMemory(0x28*4,0x00);
    irqBoard.bus().writeMemory(0x28*4+1,0x00);
    irqBoard.bus().writeMemory(0x28*4+2,0x00);
    irqBoard.bus().writeMemory(0x28*4+3,0x20);
    irqBoard.reset();
    irqBoard.bus().writeIo(0,0x13); // ICW1 single PIC/ICW4
    irqBoard.bus().writeIo(1,0x28); // ICW2
    irqBoard.bus().writeIo(1,0x01); // ICW4
    irqBoard.bus().writeIo(1,0xfe); // IRQ0 only
    auto& ir=static_cast<Main8088Board::Core&>(irqBoard.cpu());
    irqBoard.cpu().executeOne(); // reset jump
    irqBoard.cpu().executeOne(); // MOV SP
    irqBoard.cpu().executeOne(); // STI
    irqBoard.bus().requestIrq(0);
    irqBoard.cpu().executeOne(); // NOP must run before IRQ
    if(ir.ip()!=5 || ir.cs()!=0x1000 || !irqBoard.bus().interruptPending())return 47;
    irqBoard.cpu().executeOne(); // enter IRQ0 handler, before HLT
    if(ir.pc()!=0x20000 || ir.reg(4)!=0x7ffa || (ir.flags()&0x200))return 48;
    if(irqBoard.bus().interruptPending())return 49;
    if(irqBoard.bus().readMemory(0x7ffa)!=5)return 50; // saved IP
    irqBoard.cpu().executeOne(); // IRET
    if(ir.ip()!=5 || ir.cs()!=0x1000 || !(ir.flags()&0x200))return 51;
    irqBoard.bus().writeIo(0,0x20); // EOI
    irqBoard.cpu().executeOne(); // HLT
    if(!ir.halted())return 52;
    irqBoard.bus().writeIo(1,0xff); // mask IRQ0
    irqBoard.bus().requestIrq(0);
    irqBoard.cpu().executeOne(); // masked IRQ cannot wake HLT
    if(!ir.halted() || ir.ip()!=6)return 53;
    irqBoard.bus().writeIo(1,0xfe); // unmask
    irqBoard.cpu().executeOne(); // wake and enter vector 28h
    if(ir.halted() || ir.pc()!=0x20000)return 54;
    irqBoard.cpu().executeOne(); // IRET to instruction after HLT
    if(ir.ip()!=6 || ir.cs()!=0x1000)return 55;
    irqBoard.cpu().executeOne(); // NOP after HLT
    if(ir.ip()!=7)return 56;
    return 0;
}
