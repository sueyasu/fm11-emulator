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
void Main8088Board::Core::executeOne()
{
    if(halted_)return;
    const WORD opAddress=ip_;
    const BYTE opcode=fetch8();
    switch(opcode) {
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
