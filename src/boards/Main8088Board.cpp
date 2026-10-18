#include "Main8088Board.h"

Main8088Board::Main8088Board(CpuType variant):core_(bus_,variant)
{
    if(variant!=CpuType::I8088 && variant!=CpuType::NECV20)
        throw std::invalid_argument("8088 board requires 8088 or V20");
}
std::string_view Main8088Board::name() const {return "FM-11 8088 CPU card";}
CpuCore& Main8088Board::cpu(){return core_;}
const CpuCore& Main8088Board::cpu() const{return core_;}
void Main8088Board::reset(){bus_.reset();core_.reset();}

Main8088Board::Core::Core(Main8088Bus& bus,CpuType variant):bus_(bus),variant_(variant){reset();}
void Main8088Board::Core::reset()
{
    regs_.fill(0);
    segments_.fill(0);
    // Match the original Sim86 reset state: CS=FFFFh, IP=0000h.
    segments_[1]=0xffff;
    ip_=0;
    flags_=0x0002;
    halted_=false;
}
std::uint32_t Main8088Board::Core::physical(WORD seg,WORD off) const
{
    return ((std::uint32_t(seg)<<4)+off)&0xfffff;
}
std::uint32_t Main8088Board::Core::pc() const
{
    return physical(segments_[1],ip_);
}
BYTE Main8088Board::Core::fetch8()
{
    const BYTE result=bus_.readMemory(pc());
    ++ip_;
    return result;
}
WORD Main8088Board::Core::fetch16()
{
    const WORD lo=fetch8();
    return WORD(lo|(WORD(fetch8())<<8));
}
void Main8088Board::Core::push16(WORD value)
{
    regs_[4]=WORD(regs_[4]-2);
    const auto addr=physical(segments_[2],regs_[4]);
    bus_.writeMemory(addr,BYTE(value));
    bus_.writeMemory((addr+1)&0xfffff,BYTE(value>>8));
}
WORD Main8088Board::Core::pop16()
{
    const auto addr=physical(segments_[2],regs_[4]);
    const WORD value=WORD(bus_.readMemory(addr)|(WORD(bus_.readMemory((addr+1)&0xfffff))<<8));
    regs_[4]=WORD(regs_[4]+2);
    return value;
}
void Main8088Board::Core::interrupt(BYTE vector)
{
    push16(flags_);
    push16(segments_[1]);
    push16(ip_);
    flags_ &= WORD(~0x0300);
    const std::uint32_t address=std::uint32_t(vector)*4;
    ip_=WORD(bus_.readMemory(address)|(WORD(bus_.readMemory(address+1))<<8));
    segments_[1]=WORD(bus_.readMemory(address+2)|(WORD(bus_.readMemory(address+3))<<8));
    halted_=false;
}
void Main8088Board::Core::irq(){if(flags_&0x0200)interrupt(8);}
void Main8088Board::Core::nmi(){interrupt(2);}
Main8088Board::Core::Operand Main8088Board::Core::decodeModRm()
{
    const BYTE modrm=fetch8();
    const unsigned mod=modrm>>6, rm=modrm&7;
    Operand op{};
    op.reg=BYTE((modrm>>3)&7);
    if(mod==3) {op.registerOperand=true;op.index=BYTE(rm);return op;}
    WORD base=0;
    bool stack=false;
    switch(rm) {
    case 0:base=WORD(regs_[3]+regs_[6]);break; // BX+SI
    case 1:base=WORD(regs_[3]+regs_[7]);break; // BX+DI
    case 2:base=WORD(regs_[5]+regs_[6]);stack=true;break; // BP+SI
    case 3:base=WORD(regs_[5]+regs_[7]);stack=true;break; // BP+DI
    case 4:base=regs_[6];break; // SI
    case 5:base=regs_[7];break; // DI
    case 6:
        if(mod==0) base=fetch16();
        else {base=regs_[5];stack=true;}
        break; // disp16 or BP
    case 7:base=regs_[3];break; // BX
    }
    if(mod==1) base=WORD(base+static_cast<std::int8_t>(fetch8()));
    if(mod==2) base=WORD(base+fetch16());
    op.address=physical(segments_[stack?2:3],base);
    op.offset=base;
    op.segment=BYTE(stack?2:3);
    return op;
}
BYTE Main8088Board::Core::getReg8(unsigned index) const
{
    const WORD value=regs_[index&3];
    return index<4 ? BYTE(value) : BYTE(value>>8);
}
void Main8088Board::Core::setReg8(unsigned index,BYTE value)
{
    WORD& r=regs_[index&3];
    r=index<4 ? WORD((r&0xff00)|value) : WORD((r&0x00ff)|(WORD(value)<<8));
}
BYTE Main8088Board::Core::read8(const Operand& op) const
{
    return op.registerOperand ? getReg8(op.index) : bus_.readMemory(op.address);
}
WORD Main8088Board::Core::read16(const Operand& op) const
{
    if(op.registerOperand)return regs_[op.index];
    const BYTE lo=bus_.readMemory(op.address);
    const BYTE hi=bus_.readMemory(physical(segments_[op.segment],WORD(op.offset+1)));
    return WORD(lo|(WORD(hi)<<8));
}
void Main8088Board::Core::write8(const Operand& op,BYTE value)
{
    if(op.registerOperand)setReg8(op.index,value);
    else bus_.writeMemory(op.address,value);
}
void Main8088Board::Core::write16(const Operand& op,WORD value)
{
    if(op.registerOperand){regs_[op.index]=value;return;}
    bus_.writeMemory(op.address,BYTE(value));
    bus_.writeMemory(physical(segments_[op.segment],WORD(op.offset+1)),BYTE(value>>8));
}
void Main8088Board::Core::executeOne()
{
    if(halted_)return;
    const WORD opAddress=ip_;
    const BYTE opcode=fetch8();
    switch(opcode) {
    case 0x88: {auto op=decodeModRm();write8(op,getReg8(op.reg));break;} // MOV r/m8,r8
    case 0x89: {auto op=decodeModRm();write16(op,regs_[op.reg]);break;} // MOV r/m16,r16
    case 0x8a: {auto op=decodeModRm();setReg8(op.reg,read8(op));break;} // MOV r8,r/m8
    case 0x8b: {auto op=decodeModRm();regs_[op.reg]=read16(op);break;} // MOV r16,r/m16
    case 0xc6: {auto op=decodeModRm();if(op.reg!=0)throw std::runtime_error("invalid MOV C6 group");write8(op,fetch8());break;}
    case 0xc7: {auto op=decodeModRm();if(op.reg!=0)throw std::runtime_error("invalid MOV C7 group");write16(op,fetch16());break;}
    case 0x8c: {auto op=decodeModRm();if(op.reg>=4)throw std::runtime_error("invalid MOV segment");write16(op,segments_[op.reg]);break;}
    case 0x8e: {auto op=decodeModRm();if(op.reg>=4 || op.reg==1)throw std::runtime_error("invalid MOV segment");segments_[op.reg]=read16(op);break;}
    case 0x90: // NOP
        break;
    case 0xf4: // HLT
        halted_=true;
        break;
    case 0xfa: flags_ &= WORD(~0x0200);break; // CLI
    case 0xfb: flags_ |= 0x0200;break; // STI (interrupt shadow deferred)
    case 0xfc: flags_ &= WORD(~0x0400);break; // CLD
    case 0xfd: flags_ |= 0x0400;break; // STD
    case 0xea: { // JMP far ptr16:16
        const WORD off=fetch16(),seg=fetch16();
        ip_=off;segments_[1]=seg;
        break;
    }
    case 0xeb: { // JMP rel8
        const auto disp=static_cast<std::int8_t>(fetch8());
        ip_=WORD(ip_+disp);
        break;
    }
    case 0xe9: { // JMP rel16
        const WORD disp=fetch16();
        ip_=WORD(ip_+disp);
        break;
    }
    case 0xcd: interrupt(fetch8());break; // INT imm8
    case 0xcf: // IRET
        ip_=pop16();
        segments_[1]=pop16();
        flags_=WORD(pop16()|0x0002);
        break;
    case 0x9c: push16(flags_);break; // PUSHF
    case 0x9d: flags_=WORD(pop16()|0x0002);break; // POPF
    case 0xe8: { // CALL rel16
        const WORD offset=fetch16();
        push16(ip_);
        ip_=WORD(ip_+offset);
        break;
    }
    case 0xc3: ip_=pop16();break; // RET near
    case 0xcb: ip_=pop16();segments_[1]=pop16();break; // RET far
    case 0xc2: { const WORD adjustment=fetch16(); ip_=pop16();regs_[4]=WORD(regs_[4]+adjustment);break; }
    case 0xca: { const WORD adjustment=fetch16(); ip_=pop16();segments_[1]=pop16();regs_[4]=WORD(regs_[4]+adjustment);break; }
    case 0x06: push16(segments_[0]);break; // PUSH ES
    case 0x0e: push16(segments_[1]);break; // PUSH CS
    case 0x16: push16(segments_[2]);break; // PUSH SS
    case 0x1e: push16(segments_[3]);break; // PUSH DS
    case 0x07: segments_[0]=pop16();break;
    case 0x17: segments_[2]=pop16();break;
    case 0x1f: segments_[3]=pop16();break;
    default:
        if(opcode>=0x50 && opcode<=0x57) {push16(regs_[opcode-0x50]);break;}
        if(opcode>=0x58 && opcode<=0x5f) {regs_[opcode-0x58]=pop16();break;}
        if(opcode>=0xb0 && opcode<=0xb7) {
            const BYTE v=fetch8();
            const unsigned i=opcode-0xb0;
            WORD& reg=regs_[i&3];
            reg=i<4 ? WORD((reg&0xff00)|v) : WORD((reg&0x00ff)|(WORD(v)<<8));
            break;
        }
        if(opcode>=0xb8 && opcode<=0xbf) {
            regs_[opcode-0xb8]=fetch16(); // MOV r16,imm16
            break;
        }
        ip_=opAddress;
        throw std::runtime_error("8088 opcode not implemented");
    }
}
