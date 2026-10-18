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
    emulation8080_=false;
    interruptModeStack_.clear();
    irqInhibit_=0;
    segmentOverride_=-1;
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
    bus_.writeMemory(physical(segments_[2],WORD(regs_[4]+1)),BYTE(value>>8));
}
WORD Main8088Board::Core::pop16()
{
    const auto addr=physical(segments_[2],regs_[4]);
    const WORD value=WORD(bus_.readMemory(addr)|(WORD(bus_.readMemory(physical(segments_[2],WORD(regs_[4]+1))))<<8));
    regs_[4]=WORD(regs_[4]+2);
    return value;
}
void Main8088Board::Core::interrupt(BYTE vector)
{
    interruptModeStack_.push_back(emulation8080_);
    emulation8080_=false; // NEC interrupt handlers execute native V20 code.
    push16(flags_);
    push16(segments_[1]);
    push16(ip_);
    flags_ &= WORD(~0x0300);
    const std::uint32_t address=std::uint32_t(vector)*4;
    ip_=WORD(bus_.readMemory(address)|(WORD(bus_.readMemory(address+1))<<8));
    segments_[1]=WORD(bus_.readMemory(address+2)|(WORD(bus_.readMemory(address+3))<<8));
    halted_=false;
}
void Main8088Board::Core::irq()
{
    // External IRQ requests enter the PIC. Delivery takes place at the
    // next instruction boundary, using the PIC-programmed vector.
    bus_.requestIrq(0);
}
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
    const unsigned segment=segmentOverride_>=0 ? unsigned(segmentOverride_) : (stack?2u:3u);
    op.address=physical(segments_[segment],base);
    op.offset=base;
    op.segment=BYTE(segment);
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
namespace {
constexpr WORD CF=0x0001, PF=0x0004, AF=0x0010, ZF=0x0040;
constexpr WORD SF=0x0080, OF=0x0800;
bool parityEven(BYTE value) {
    value ^= BYTE(value>>4);
    value ^= BYTE(value>>2);
    value ^= BYTE(value>>1);
    return (value&1)==0;
}
}
void Main8088Board::Core::setArithmeticFlags(unsigned lhs,unsigned rhs,
                                              unsigned result,unsigned bits,bool subtract)
{
    const unsigned mask=bits==8?0xffu:0xffffu;
    const unsigned sign=bits==8?0x80u:0x8000u;
    const unsigned out=result&mask;
    flags_ &= WORD(~(CF|PF|AF|ZF|SF|OF));
    if(subtract ? lhs<rhs : result>mask) flags_|=CF;
    if(((lhs^rhs^out)&0x10u)!=0)flags_|=AF;
    if(out==0)flags_|=ZF;
    if((out&sign)!=0)flags_|=SF;
    if(parityEven(BYTE(out)))flags_|=PF;
    if(subtract ? (((lhs^rhs)&(lhs^out)&sign)!=0)
                : (((~(lhs^rhs))&(lhs^out)&sign)!=0))flags_|=OF;
}
void Main8088Board::Core::setLogicFlags(unsigned result,unsigned bits)
{
    const unsigned sign=bits==8?0x80u:0x8000u;
    const unsigned mask=bits==8?0xffu:0xffffu;
    const unsigned out=result&mask;
    flags_ &= WORD(~(CF|PF|AF|ZF|SF|OF));
    if(out==0)flags_|=ZF;
    if(out&sign)flags_|=SF;
    if(parityEven(BYTE(out)))flags_|=PF;
}
unsigned Main8088Board::Core::alu(unsigned operation,unsigned lhs,unsigned rhs,unsigned bits)
{
    const unsigned mask=bits==8?0xffu:0xffffu;
    lhs&=mask;rhs&=mask;
    unsigned result=lhs;
    switch(operation&7) {
    case 0: // ADD
        result=lhs+rhs;
        setArithmeticFlags(lhs,rhs,result,bits,false);
        break;
    case 1: // OR
        result=lhs|rhs;setLogicFlags(result,bits);break;
    case 2: { // ADC
        const unsigned carry=(flags_&CF)?1u:0u;
        result=lhs+rhs+carry;
        setArithmeticFlags(lhs,rhs+carry,result,bits,false);
        // AF must use the original operand and the incoming carry.
        break;
    }
    case 3: { // SBB
        const unsigned carry=(flags_&CF)?1u:0u;
        result=(lhs-rhs-carry)&mask;
        setArithmeticFlags(lhs,rhs+carry,result,bits,true);
        break;
    }
    case 4:result=lhs&rhs;setLogicFlags(result,bits);break;
    case 5:result=(lhs-rhs)&mask;setArithmeticFlags(lhs,rhs,result,bits,true);break;
    case 6:result=lhs^rhs;setLogicFlags(result,bits);break;
    case 7:result=(lhs-rhs)&mask;setArithmeticFlags(lhs,rhs,result,bits,true);break;
    }
    return result&mask;
}
unsigned Main8088Board::Core::readOperand(const Operand& op,unsigned bits) const
{
    return bits==8?read8(op):read16(op);
}
void Main8088Board::Core::writeOperand(const Operand& op,unsigned bits,unsigned value)
{
    if(bits==8)write8(op,BYTE(value));
    else write16(op,WORD(value));
}
unsigned Main8088Board::Core::incDec(unsigned lhs,unsigned bits,bool decrement)
{
    const unsigned mask=bits==8?0xffu:0xffffu;
    const unsigned result=decrement?((lhs-1u)&mask):(lhs+1u);
    const bool carry=(flags_&CF)!=0;
    setArithmeticFlags(lhs,1,result,bits,decrement);
    if(carry)flags_|=CF;else flags_&=WORD(~CF);
    return result&mask;
}
unsigned Main8088Board::Core::getReg(unsigned index,unsigned bits) const
{
    return bits==8?getReg8(index):regs_[index];
}
void Main8088Board::Core::setReg(unsigned index,unsigned bits,unsigned value)
{
    if(bits==8)setReg8(index,BYTE(value));
    else regs_[index]=WORD(value);
}
void Main8088Board::Core::executeAluModRm(BYTE opcode)
{
    const auto op=decodeModRm();
    const unsigned bits=(opcode&1)?16u:8u;
    const bool destinationReg=(opcode&2)!=0;
    const unsigned lhs=destinationReg?getReg(op.reg,bits):readOperand(op,bits);
    const unsigned rhs=destinationReg?readOperand(op,bits):getReg(op.reg,bits);
    const unsigned operation=(opcode>>3)&7;
    const unsigned result=alu(operation,lhs,rhs,bits);
    if(operation!=7) {
        if(destinationReg)setReg(op.reg,bits,result);
        else writeOperand(op,bits,result);
    }
}
unsigned Main8088Board::Core::shiftRotate(unsigned operation,unsigned value,
                                               unsigned bits,unsigned count)
{
    const unsigned mask=bits==8?0xffu:0xffffu;
    const unsigned sign=bits==8?0x80u:0x8000u;
    value&=mask;
    if(count==0)return value; // An 8088 does not mask CL; zero leaves flags intact.
    const unsigned original=value;
    bool carry=(flags_&CF)!=0;
    for(unsigned n=0;n<count;++n) {
        switch(operation&7) {
        case 0: // ROL
            carry=(value&sign)!=0;
            value=((value<<1)|unsigned(carry))&mask;
            break;
        case 1: // ROR
            carry=(value&1)!=0;
            value=(value>>1)|(carry?sign:0);
            break;
        case 2: { // RCL
            const bool next=(value&sign)!=0;
            value=((value<<1)|unsigned(carry))&mask;
            carry=next;
            break;
        }
        case 3: { // RCR
            const bool next=(value&1)!=0;
            value=(value>>1)|(carry?sign:0);
            carry=next;
            break;
        }
        case 4: case 6: // SHL/SAL
            carry=(value&sign)!=0;
            value=(value<<1)&mask;
            break;
        case 5: // SHR
            carry=(value&1)!=0;
            value>>=1;
            break;
        case 7: // SAR
            carry=(value&1)!=0;
            value=(value>>1)|(value&sign);
            break;
        }
    }
    flags_=WORD((flags_&~CF)|(carry?CF:0));
    if(count==1) {
        bool overflow=false;
        switch(operation&7) {
        case 0: case 2: case 4: case 6:
            overflow=((value&sign)!=0)!=carry;
            break;
        case 1: case 3:
            overflow=((value&sign)!=0)!=((value&(sign>>1))!=0);
            break;
        case 5: overflow=(original&sign)!=0;break;
        case 7: overflow=false;break;
        }
        flags_=WORD((flags_&~OF)|(overflow?OF:0));
    } // OF undefined for count > 1; preserve its prior value.
    if((operation&7)>=4) {
        // Shift result updates SF/ZF/PF. AF is undefined, left untouched.
        flags_&=WORD(~(SF|ZF|PF));
        if(value&sign)flags_|=SF;
        if(value==0)flags_|=ZF;
        if(parityEven(BYTE(value)))flags_|=PF;
    }
    return value;
}
bool Main8088Board::Core::jumpCondition(unsigned code) const
{
    const bool cf=(flags_&CF)!=0,zf=(flags_&ZF)!=0;
    const bool sf=(flags_&SF)!=0,of=(flags_&OF)!=0,pf=(flags_&PF)!=0;
    switch(code&15) {
    case 0:return of;case 1:return !of;
    case 2:return cf;case 3:return !cf;
    case 4:return zf;case 5:return !zf;
    case 6:return cf||zf;case 7:return !cf&&!zf;
    case 8:return sf;case 9:return !sf;
    case 10:return pf;case 11:return !pf;
    case 12:return sf!=of;case 13:return sf==of;
    case 14:return zf||(sf!=of);case 15:return !zf&&(sf==of);
    }
    return false;
}
void Main8088Board::Core::executeOne()
{
    // 8088 recognizes maskable IRQs at instruction boundaries. STI and
    // MOV SS defer recognition until the following instruction completes.
    const bool inhibited=irqInhibit_!=0;
    if(irqInhibit_!=0)--irqInhibit_;
    if(!inhibited && (flags_&0x0200) && bus_.interruptPending()) {
        interrupt(bus_.acknowledgeInterrupt());
        return;
    }
    if(halted_)return;
    if(emulation8080_) {
        const BYTE instruction=fetch8();
        if(instruction==0x00) return; // 8080 NOP
        if(instruction==0xed) {
            const BYTE subopcode=fetch8();
            if(subopcode==0xfd) { // RETEM: restore the native frame
                ip_=pop16();
                segments_[1]=pop16();
                flags_=pop16();
                emulation8080_=false;
                if(!interruptModeStack_.empty())interruptModeStack_.pop_back();
                return;
            }
            throw std::runtime_error("NEC V20 8080 mode instruction not implemented");
        }
        if(instruction==0x76) {halted_=true;return;} // 8080 HLT
        // NEC maps A=AL, B/C=CH/CL, D/E=DH/DL, H/L=BH/BL,
        // and the 8080 stack pointer to BP. The 8080 data segment is DS.
        const auto readReg=[&](unsigned index)->BYTE {
            switch(index&7u) {
            case 0:return getReg8(5); // B=CH
            case 1:return getReg8(1); // C=CL
            case 2:return getReg8(6); // D=DH
            case 3:return getReg8(2); // E=DL
            case 4:return getReg8(7); // H=BH
            case 5:return getReg8(3); // L=BL
            case 6:return bus_.readMemory(physical(segments_[3],regs_[3]));
            default:return getReg8(0); // A=AL
            }
        };
        const auto writeReg=[&](unsigned index,BYTE value) {
            switch(index&7u) {
            case 0:setReg8(5,value);break;
            case 1:setReg8(1,value);break;
            case 2:setReg8(6,value);break;
            case 3:setReg8(2,value);break;
            case 4:setReg8(7,value);break;
            case 5:setReg8(3,value);break;
            case 6:bus_.writeMemory(physical(segments_[3],regs_[3]),value);break;
            default:setReg8(0,value);break;
            }
        };
        // 8080 arithmetic flags map S/Z/AC/P/C to SF/ZF/AF/PF/CF.
        // The 8080 does not define the x86 overflow flag.
        const auto update8080=[&](unsigned lhs,unsigned rhs,unsigned result,
                              bool subtract,bool carryIn) {
            const BYTE out=BYTE(result);
            flags_=WORD(flags_&~(CF|PF|AF|ZF|SF));
            if(subtract ? lhs<(rhs+unsigned(carryIn))
                        : result>255u)flags_|=CF;
            if(((lhs^rhs^out)&0x10u)!=0 ||
               (carryIn && ((lhs^rhs^out)&0x10u)==0 &&
                ((lhs&15u)+(subtract ? 0u : (rhs&15u))+unsigned(carryIn)>15u)))
                flags_|=AF;
            if(out==0)flags_|=ZF;
            if(out&0x80)flags_|=SF;
            if(parityEven(out))flags_|=PF;
        };
        const auto arithmetic=[&](unsigned op,BYTE rhs) {
            const unsigned lhs=getReg8(0);
            const unsigned incoming=(flags_&CF)?1u:0u;
            unsigned out=lhs;
            switch(op) {
            case 0:case 1: { // ADD / ADC
                const unsigned carry=op==1?incoming:0u;
                out=lhs+rhs+carry;
                update8080(lhs,rhs,out,false,carry!=0);
                break;
            }
            case 2:case 3:case 7: { // SUB / SBB / CMP
                const unsigned borrow=op==3?incoming:0u;
                out=(lhs-rhs-borrow)&255u;
                update8080(lhs,rhs,out,true,borrow!=0);
                break;
            }
            case 4: // ANA
                out=lhs&rhs;
                flags_=WORD(flags_&~(CF|PF|AF|ZF|SF));
                flags_|=AF; // 8080 ANA sets auxiliary carry
                if(out==0)flags_|=ZF;
                if(out&0x80)flags_|=SF;
                if(parityEven(BYTE(out)))flags_|=PF;
                break;
            case 5:case 6: // XRA / ORA
                out=op==5?(lhs^rhs):(lhs|rhs);
                flags_=WORD(flags_&~(CF|PF|AF|ZF|SF));
                if(out==0)flags_|=ZF;
                if(out&0x80)flags_|=SF;
                if(parityEven(BYTE(out)))flags_|=PF;
                break;
            }
            if(op!=7)setReg8(0,BYTE(out));
        };
        if((instruction&0xc0)==0x80) {
            arithmetic((instruction>>3)&7u,readReg(instruction&7u));
            return;
        }
        if((instruction&0xc7)==0x04 || (instruction&0xc7)==0x05) {
            const unsigned index=(instruction>>3)&7u;
            const BYTE lhs=readReg(index);
            const bool decrement=(instruction&1u)!=0;
            const unsigned result=decrement ? unsigned(BYTE(lhs-1u))
                                            : unsigned(BYTE(lhs+1u));
            const bool oldCarry=(flags_&CF)!=0;
            update8080(lhs,1u,result,decrement,false);
            if(oldCarry)flags_|=CF;
            else flags_&=WORD(~CF);
            writeReg(index,BYTE(result));
            return;
        }
        if((instruction&0xc0)==0x40) { // MOV r,r / MOV r,M / MOV M,r
            writeReg((instruction>>3)&7u,readReg(instruction&7u));
            return;
        }
        if((instruction&0xc7)==0x06) { // MVI r,imm8
            const BYTE value=fetch8();
            writeReg((instruction>>3)&7u,value);
            return;
        }
        if((instruction&0xcf)==0x01) { // LXI B,D,H,SP
            const WORD value=fetch16();
            switch((instruction>>4)&3u) {
            case 0:regs_[1]=value;break; // BC=CX
            case 1:regs_[2]=value;break; // DE=DX
            case 2:regs_[3]=value;break; // HL=BX
            case 3:regs_[5]=value;break; // SP=BP
            }
            return;
        }
        // 8080 stack lives at DS:BP; the native-mode interrupt frame
        // remains on SS:SP so RETEM can restore it independently.
        const auto push8080=[&](WORD value) {
            regs_[5]=WORD(regs_[5]-2);
            bus_.writeMemory(physical(segments_[3],regs_[5]),BYTE(value));
            bus_.writeMemory(physical(segments_[3],WORD(regs_[5]+1)),BYTE(value>>8));
        };
        const auto pop8080=[&]()->WORD {
            const WORD lo=bus_.readMemory(physical(segments_[3],regs_[5]));
            const WORD hi=bus_.readMemory(physical(segments_[3],WORD(regs_[5]+1)));
            regs_[5]=WORD(regs_[5]+2);
            return WORD(lo|(hi<<8));
        };
        const auto condition8080=[&](unsigned cc)->bool {
            switch(cc&7u) {
            case 0:return (flags_&ZF)==0; // NZ
            case 1:return (flags_&ZF)!=0; // Z
            case 2:return (flags_&CF)==0; // NC
            case 3:return (flags_&CF)!=0; // C
            case 4:return (flags_&PF)==0; // PO
            case 5:return (flags_&PF)!=0; // PE
            case 6:return (flags_&SF)==0; // P
            default:return (flags_&SF)!=0; // M
            }
        };
        if((instruction&0xc7)==0xc2) { // Jcc addr
            const WORD target=fetch16();
            if(condition8080(instruction>>3))ip_=target;
            return;
        }
        if((instruction&0xc7)==0xc4) { // Ccc addr
            const WORD target=fetch16();
            if(condition8080(instruction>>3)) {
                push8080(ip_);
                ip_=target;
            }
            return;
        }
        if((instruction&0xc7)==0xc0) { // Rcc
            if(condition8080(instruction>>3))ip_=pop8080();
            return;
        }
        if((instruction&0xc7)==0xc7) { // RST n
            push8080(ip_);
            ip_=WORD(instruction&0x38);
            return;
        }
        if((instruction&0xcf)==0xc5) { // PUSH B/D/H/PSW
            const unsigned pair=(instruction>>4)&3u;
            WORD value=0;
            if(pair==0)value=regs_[1];
            else if(pair==1)value=regs_[2];
            else if(pair==2)value=regs_[3];
            else {
                const BYTE f=BYTE((flags_&(SF|ZF|AF|PF|CF))|0x02);
                value=WORD((WORD(getReg8(0))<<8)|f);
            }
            push8080(value);
            return;
        }
        if((instruction&0xcf)==0xc1) { // POP B/D/H/PSW
            const unsigned pair=(instruction>>4)&3u;
            const WORD value=pop8080();
            if(pair==0)regs_[1]=value;
            else if(pair==1)regs_[2]=value;
            else if(pair==2)regs_[3]=value;
            else {
                setReg8(0,BYTE(value>>8));
                flags_=WORD((flags_&~(SF|ZF|AF|PF|CF))|
                            (value&(SF|ZF|AF|PF|CF)));
            }
            return;
        }
        if((instruction&0xcf)==0x03 || (instruction&0xcf)==0x0b) {
            const unsigned pair=(instruction>>4)&3u;
            WORD& target=regs_[pair==3?5u:(pair==2?3u:pair==1?2u:1u)];
            target=WORD(target+((instruction&8u)?-1:1)); // INX/DCX
            return;
        }
        if((instruction&0xcf)==0x09) { // DAD rp: HL += rp; only CY changes
            const unsigned pair=(instruction>>4)&3u;
            const WORD rhs=regs_[pair==3?5u:(pair==2?3u:pair==1?2u:1u)];
            const unsigned result=unsigned(regs_[3])+rhs;
            regs_[3]=WORD(result);
            flags_=WORD((flags_&~CF)|((result>0xffffu)?CF:0));
            return;
        }
        switch(instruction) {
        case 0xeb: { // XCHG D,H
            const WORD temp=regs_[2];
            regs_[2]=regs_[3];
            regs_[3]=temp;
            return;
        }
        case 0xe3: { // XTHL: exchange HL with word at 8080 SP
            const WORD offset=regs_[5];
            const BYTE lo=bus_.readMemory(physical(segments_[3],offset));
            const BYTE hi=bus_.readMemory(physical(segments_[3],WORD(offset+1)));
            bus_.writeMemory(physical(segments_[3],offset),BYTE(regs_[3]));
            bus_.writeMemory(physical(segments_[3],WORD(offset+1)),BYTE(regs_[3]>>8));
            regs_[3]=WORD(lo|(WORD(hi)<<8));
            return;
        }
        case 0x07: case 0x0f: case 0x17: case 0x1f: {
            const BYTE value=getReg8(0);
            const bool oldCarry=(flags_&CF)!=0;
            bool carry=false;
            BYTE result=0;
            switch(instruction) {
            case 0x07: carry=(value&0x80)!=0;result=BYTE((value<<1)|BYTE(carry));break; // RLC
            case 0x0f: carry=(value&1)!=0;result=BYTE((value>>1)|(carry?0x80:0));break; // RRC
            case 0x17: carry=(value&0x80)!=0;result=BYTE((value<<1)|BYTE(oldCarry));break; // RAL
            default: carry=(value&1)!=0;result=BYTE((value>>1)|(oldCarry?0x80:0));break; // RAR
            }
            setReg8(0,result);
            flags_=WORD((flags_&~CF)|(carry?CF:0));
            return;
        }
        case 0x27: { // 8080 DAA: decimal-adjust AL after BCD addition
            const unsigned original=getReg8(0);
            const bool oldCarry=(flags_&CF)!=0;
            const bool oldAux=(flags_&AF)!=0;
            unsigned correction=0;
            if((original&0x0f)>9 || oldAux)correction|=0x06;
            const bool highAdjust=original>0x99 || oldCarry;
            if(highAdjust)correction|=0x60;
            const unsigned sum=original+correction;
            const BYTE adjusted=BYTE(sum);
            flags_=WORD(flags_&~(CF|AF|ZF|SF|PF));
            if(highAdjust)flags_|=CF;
            if(((original^sum^correction)&0x10u)!=0)flags_|=AF;
            if(adjusted==0)flags_|=ZF;
            if(adjusted&0x80)flags_|=SF;
            if(parityEven(adjusted))flags_|=PF;
            setReg8(0,adjusted);
            return;
        }
        case 0x2f: setReg8(0,BYTE(~getReg8(0)));return; // CMA
        case 0x37: flags_|=CF;return; // STC
        case 0x3f: flags_^=CF;return; // CMC
        case 0xdb: { // IN port: 8080 immediate 8-bit port, result in A
            const BYTE port=fetch8();
            setReg8(0,bus_.readIo(port));
            return;
        }
        case 0xd3: { // OUT port: 8080 immediate 8-bit port, value from A
            const BYTE port=fetch8();
            bus_.writeIo(port,getReg8(0));
            return;
        }
        case 0xfb: flags_|=0x0200;irqInhibit_=1;return; // EI: next instruction completes before IRQ
        case 0xf3: flags_&=WORD(~0x0200);return; // DI
        case 0xc3: ip_=fetch16();return; // JMP
        case 0xcd: { // CALL
            const WORD target=fetch16();
            push8080(ip_);
            ip_=target;
            return;
        }
        case 0xc9: ip_=pop8080();return; // RET
        case 0xe9: ip_=regs_[3];return; // PCHL
        case 0xf9: regs_[5]=regs_[3];return; // SPHL
        case 0xc6: arithmetic(0,fetch8());return;
        case 0xce: arithmetic(1,fetch8());return;
        case 0xd6: arithmetic(2,fetch8());return;
        case 0xde: arithmetic(3,fetch8());return;
        case 0xe6: arithmetic(4,fetch8());return;
        case 0xee: arithmetic(5,fetch8());return;
        case 0xf6: arithmetic(6,fetch8());return;
        case 0xfe: arithmetic(7,fetch8());return;
        case 0x02: case 0x12: { // STAX B / STAX D
            const WORD off=regs_[instruction==0x02?1u:2u];
            bus_.writeMemory(physical(segments_[3],off),getReg8(0));
            return;
        }
        case 0x0a: case 0x1a: { // LDAX B / LDAX D
            const WORD off=regs_[instruction==0x0a?1u:2u];
            setReg8(0,bus_.readMemory(physical(segments_[3],off)));
            return;
        }
        case 0x22: { // SHLD addr
            const WORD off=fetch16();
            bus_.writeMemory(physical(segments_[3],off),BYTE(regs_[3]));
            bus_.writeMemory(physical(segments_[3],WORD(off+1)),BYTE(regs_[3]>>8));
            return;
        }
        case 0x2a: { // LHLD addr
            const WORD off=fetch16();
            const BYTE lo=bus_.readMemory(physical(segments_[3],off));
            const BYTE hi=bus_.readMemory(physical(segments_[3],WORD(off+1)));
            regs_[3]=WORD(lo|(WORD(hi)<<8));
            return;
        }
        case 0x32: // STA addr
            bus_.writeMemory(physical(segments_[3],fetch16()),getReg8(0));
            return;
        case 0x3a: // LDA addr
            setReg8(0,bus_.readMemory(physical(segments_[3],fetch16())));
            return;
        default:
            throw std::runtime_error("NEC V20 8080 mode instruction not implemented");
        }
    }
    const WORD opAddress=ip_;
    segmentOverride_=-1;
    BYTE repeat=0;
    bool lockPrefix=false;
    BYTE opcode=0;
    unsigned prefixCount=0;
    while(true) {
        opcode=fetch8();
        if(opcode==0xf2 || opcode==0xf3)repeat=opcode;
        else if((opcode==0x64 || opcode==0x65) && variant_==CpuType::NECV20)
            repeat=opcode;
        else if(opcode==0xf0)lockPrefix=true;
        else if(opcode==0x26)segmentOverride_=0;
        else if(opcode==0x2e)segmentOverride_=1;
        else if(opcode==0x36)segmentOverride_=2;
        else if(opcode==0x3e)segmentOverride_=3;
        else break;
        if(++prefixCount>=15)throw std::runtime_error("too many 8088 prefixes");
    }
    // The emulated bus executes one instruction at a time; LOCK imposes
    // no additional atomicity until concurrent bus masters are modeled.
    (void)lockPrefix;
    const bool ioString=variant_==CpuType::NECV20 && opcode>=0x6c && opcode<=0x6f;
    const bool stringOp=ioString || (opcode>=0xa4 && opcode<=0xa7) || (opcode>=0xaa && opcode<=0xaf);
    if(repeat && !stringOp)
        throw std::runtime_error("unsupported REP opcode");
    if((repeat==0x64 || repeat==0x65) &&
       !(opcode==0xa6 || opcode==0xa7 || opcode==0xae || opcode==0xaf))
        throw std::runtime_error("REPC/REPNC requires a comparison string opcode");
    switch(opcode) {
    case 0x6c: case 0x6d: case 0x6e: case 0x6f: {
        if(variant_!=CpuType::NECV20)
            throw std::runtime_error("opcode requires NEC V20");
        const bool word=(opcode&1)!=0;
        const bool input=(opcode&2)==0;
        const WORD delta=WORD((flags_&0x0400) ? (word?-2:-1) : (word?2:1));
        if(repeat && regs_[1]==0)break;
        const WORD port=regs_[2];
        if(input) {
            const WORD value=word
                ? WORD(bus_.readIo(port)|(WORD(bus_.readIo(WORD(port+1)))<<8))
                : bus_.readIo(port);
            const WORD offset=regs_[7];
            bus_.writeMemory(physical(segments_[0],offset),BYTE(value));
            if(word)bus_.writeMemory(physical(segments_[0],WORD(offset+1)),BYTE(value>>8));
            regs_[7]=WORD(offset+delta);
        } else {
            const WORD offset=regs_[6];
            const WORD segment=segments_[segmentOverride_>=0
                ? unsigned(segmentOverride_):3u];
            const WORD value=word
                ? WORD(bus_.readMemory(physical(segment,offset)) |
                    (WORD(bus_.readMemory(physical(segment,WORD(offset+1))))<<8))
                : bus_.readMemory(physical(segment,offset));
            bus_.writeIo(port,BYTE(value));
            if(word)bus_.writeIo(WORD(port+1),BYTE(value>>8));
            regs_[6]=WORD(offset+delta);
        }
        if(repeat) {
            regs_[1]=WORD(regs_[1]-1);
            if(regs_[1]!=0)ip_=opAddress;
        }
        break;
    }
    case 0xa4: case 0xa5: case 0xa6: case 0xa7:
    case 0xaa: case 0xab: case 0xac: case 0xad:
    case 0xae: case 0xaf: {
        const bool word=(opcode&1)!=0;
        const WORD delta=WORD((flags_&0x0400)?(word?-2:-1):(word?2:1));
        if(repeat && regs_[1]==0)break;
        const bool mov=opcode==0xa4 || opcode==0xa5;
        const bool cmp=opcode==0xa6 || opcode==0xa7;
        const bool stos=opcode==0xaa || opcode==0xab;
        const bool lods=opcode==0xac || opcode==0xad;
        const bool scas=opcode==0xae || opcode==0xaf;
        const auto readString=[&](WORD segment,WORD offset)->WORD {
            const BYTE lo=bus_.readMemory(physical(segment,offset));
            if(!word)return lo;
            return WORD(lo|(WORD(bus_.readMemory(physical(segment,WORD(offset+1))))<<8));
        };
        const auto writeString=[&](WORD segment,WORD offset,WORD value) {
            bus_.writeMemory(physical(segment,offset),BYTE(value));
            if(word)bus_.writeMemory(physical(segment,WORD(offset+1)),BYTE(value>>8));
        };
        WORD source=0,destination=0;
        if(mov || cmp || lods) {
            source=readString(segments_[segmentOverride_>=0?unsigned(segmentOverride_):3u],regs_[6]);
            regs_[6]=WORD(regs_[6]+delta);
        }
        if(cmp || scas)destination=readString(segments_[0],regs_[7]);
        if(mov)writeString(segments_[0],regs_[7],source);
        if(stos)writeString(segments_[0],regs_[7],regs_[0]);
        if(lods)regs_[0]=word?source:WORD((regs_[0]&0xff00)|BYTE(source));
        if(cmp)alu(7,source,destination,word?16:8);
        if(scas)alu(7,word?regs_[0]:BYTE(regs_[0]),destination,word?16:8);
        if(mov || cmp || stos || scas)regs_[7]=WORD(regs_[7]+delta);
        if(repeat) {
            regs_[1]=WORD(regs_[1]-1);
            const bool carryRepeat=repeat==0x64 || repeat==0x65;
            const bool continueRepeat=carryRepeat
                ? (((flags_&CF)!=0)==(repeat==0x65))
                : ((!cmp && !scas) ||
                   (((flags_&ZF)!=0)==(repeat==0xf3)));
            if(regs_[1]!=0 && continueRepeat)ip_=opAddress;
        }
        break;
    }
    // Arithmetic/logic immediate-to-accumulator encodings have the pattern
    // 00xxx10w: AL/AX selected by w, operation selected by bits 5..3.
    case 0x04: case 0x05: case 0x0c: case 0x0d:
    case 0x14: case 0x15: case 0x1c: case 0x1d:
    case 0x24: case 0x25: case 0x2c: case 0x2d:
    case 0x34: case 0x35: case 0x3c: case 0x3d: {
        const bool word=(opcode&1)!=0;
        const unsigned immediate=word?fetch16():fetch8();
        const unsigned lhs=word?regs_[0]:getReg8(0);
        const unsigned operation=(opcode>>3)&7;
        const unsigned value=alu(operation,lhs,immediate,word?16:8);
        if(operation!=7) {
            if(word)regs_[0]=WORD(value);
            else setReg8(0,BYTE(value));
        }
        break;
    }
    case 0x27: case 0x2f: { // DAA/DAS: decimal adjust AL
        const unsigned original=getReg8(0);
        const bool oldAf=(flags_&AF)!=0, oldCf=(flags_&CF)!=0;
        const bool subtract=opcode==0x2f;
        const bool adjustLow=(original&15)>9 || oldAf;
        unsigned result=original;
        if(adjustLow)result=subtract?(result-6u)&255u:(result+6u)&255u;
        // Intel's 8086 DAA/DAS high-digit adjustment is based on
        // the original AL, not the value after the low-digit correction.
        const bool adjustHigh=original>0x99 || oldCf;
        if(adjustHigh)result=subtract?(result-0x60u)&255u:(result+0x60u)&255u;
        flags_=WORD(flags_&~(CF|AF|SF|ZF|PF));
        if(adjustLow)flags_|=AF;
        if(adjustHigh)flags_|=CF;
        if(result&0x80)flags_|=SF;
        if(result==0)flags_|=ZF;
        if(parityEven(BYTE(result)))flags_|=PF;
        setReg8(0,BYTE(result));
        break;
    }
    case 0x37: case 0x3f: { // AAA/AAS: unpacked decimal correction
        const bool adjust=(getReg8(0)&15)>9 || (flags_&AF);
        if(adjust) {
            regs_[0]=WORD(regs_[0]+(opcode==0x37?0x0106u:0xfefau));
            flags_|=WORD(AF|CF);
        } else flags_&=WORD(~(AF|CF));
        setReg8(0,BYTE(getReg8(0)&15));
        break;
    }
    case 0xd4: { // AAM imm8 (normally 0Ah); divisor zero faults
        const unsigned base=fetch8();
        if(base==0){interrupt(0);break;}
        const unsigned original=getReg8(0);
        regs_[0]=WORD(((original/base)<<8)|(original%base));
        flags_=WORD(flags_&~(SF|ZF|PF));
        if(getReg8(0)&0x80)flags_|=SF;
        if(getReg8(0)==0)flags_|=ZF;
        if(parityEven(getReg8(0)))flags_|=PF;
        break;
    }
    case 0xd5: { // AAD imm8 (normally 0Ah)
        const unsigned base=fetch8();
        const unsigned result=(getReg8(4)*base+getReg8(0))&255u;
        regs_[0]=WORD(result); // AH = 0
        flags_=WORD(flags_&~(SF|ZF|PF));
        if(result&0x80)flags_|=SF;
        if(result==0)flags_|=ZF;
        if(parityEven(BYTE(result)))flags_|=PF;
        break;
    }
    case 0x8f: { // POP r/m16 (group /0)
        const auto op=decodeModRm();
        if(op.reg!=0)throw std::runtime_error("invalid 8F opcode");
        // Decode effective address before popping. SP then advances before
        // writing; POP [SP] is not an 8086 addressing form.
        write16(op,pop16());
        break;
    }
    case 0x9a: { // CALL ptr16:16, immediate offset followed by segment
        const WORD offset=fetch16();
        const WORD segment=fetch16();
        const WORD returnCs=segments_[1],returnIp=ip_;
        push16(returnCs);
        push16(returnIp);
        segments_[1]=segment;
        ip_=offset;
        break;
    }
    case 0x9b: break; // WAIT: no 8087 or TEST pin in this CPU-card model
    case 0xcc: interrupt(3);break; // INT3
    case 0xce: if(flags_&OF)interrupt(4);break; // INTO
    case 0x98: // CBW: sign-extend AL into AX (FLAGS unchanged)
        regs_[0]=WORD(static_cast<std::int16_t>(static_cast<std::int8_t>(BYTE(regs_[0]))));
        break;
    case 0x99: // CWD: sign-extend AX into DX:AX
        regs_[2]=(regs_[0]&0x8000)?0xffff:0;
        break;
    case 0xf8: flags_&=WORD(~0x0001);break; // CLC
    case 0xf9: flags_|=0x0001;break; // STC
    case 0xf5: flags_^=0x0001;break; // CMC
    case 0x9e: { // SAHF: AH -> SF,ZF,AF,PF,CF
        constexpr WORD mask=0x00d5;
        flags_=WORD((flags_&~mask)|(regs_[0]>>8&mask)|0x0002);
        break;
    }
    case 0x9f: // LAHF: SF,ZF,AF,PF,CF and reserved bit 1 -> AH
        setReg8(4,BYTE(flags_));
        break;
    case 0xe0: case 0xe1: case 0xe2: { // LOOPNE, LOOPE, LOOP
        const auto displacement=static_cast<std::int8_t>(fetch8());
        regs_[1]=WORD(regs_[1]-1);
        const bool zf=(flags_&0x0040)!=0;
        if(regs_[1]!=0 && (opcode==0xe2 || (opcode==0xe1?zf:!zf)))
            ip_=WORD(ip_+displacement);
        break;
    }
    case 0xe3: { // JCXZ, no decrement
        const auto displacement=static_cast<std::int8_t>(fetch8());
        if(regs_[1]==0)ip_=WORD(ip_+displacement);
        break;
    }
    case 0xd0: case 0xd1: case 0xd2: case 0xd3: {
        // Group 2: /0..7 ROL/ROR/RCL/RCR/SHL/SHR/SAL/SAR.
        const auto op=decodeModRm();
        const unsigned bits=(opcode&1)?16u:8u;
        const unsigned count=(opcode&2)?getReg8(1):1u; // CL or implicit 1
        if(count!=0) {
            const unsigned value=shiftRotate(op.reg,readOperand(op,bits),bits,count);
            writeOperand(op,bits,value);
        }
        break;
    }
    case 0xf6: case 0xf7: {
        // Group 3, width selected by opcode bit 0.
        const auto op=decodeModRm();
        const unsigned bits=(opcode&1)?16u:8u;
        const unsigned mask=bits==8?0xffu:0xffffu;
        const unsigned operand=readOperand(op,bits);
        switch(op.reg) {
        case 0: case 1: { // TEST (8086 also aliases /1)
            const unsigned immediate=bits==8?fetch8():fetch16();
            setLogicFlags(operand&immediate,bits);
            break;
        }
        case 2: writeOperand(op,bits,(~operand)&mask);break; // NOT
        case 3: { // NEG: zero minus operand
            const unsigned result=(-operand)&mask;
            setArithmeticFlags(0,operand,result,bits,true);
            writeOperand(op,bits,result);
            break;
        }
        case 4: { // MUL: unsigned
            if(bits==8) {
                const unsigned product=getReg8(0)*operand;
                regs_[0]=WORD(product);
                flags_=WORD((flags_&~(CF|OF))|((product>0xff)?(CF|OF):0));
            } else {
                const std::uint32_t product=std::uint32_t(regs_[0])*operand;
                regs_[0]=WORD(product);
                regs_[2]=WORD(product>>16);
                flags_=WORD((flags_&~(CF|OF))|((regs_[2]!=0)?(CF|OF):0));
            }
            break;
        }
        case 5: { // IMUL: signed
            if(bits==8) {
                const int product=int(static_cast<std::int8_t>(getReg8(0)))*
                                  int(static_cast<std::int8_t>(BYTE(operand)));
                regs_[0]=WORD(product);
                const bool overflow=product < -128 || product > 127;
                flags_=WORD((flags_&~(CF|OF))|(overflow?(CF|OF):0));
            } else {
                const std::int32_t product=
                    std::int32_t(static_cast<std::int16_t>(regs_[0]))*
                    std::int32_t(static_cast<std::int16_t>(WORD(operand)));
                regs_[0]=WORD(product);
                regs_[2]=WORD(std::uint32_t(product)>>16);
                const bool overflow=product < -32768 || product > 32767;
                flags_=WORD((flags_&~(CF|OF))|(overflow?(CF|OF):0));
            }
            break;
        }
        case 6: { // DIV
            if(operand==0){interrupt(0);break;}
            if(bits==8) {
                const unsigned quotient=regs_[0]/operand;
                if(quotient>0xff){interrupt(0);break;}
                const unsigned remainder=regs_[0]%operand;
                regs_[0]=WORD((remainder<<8)|quotient);
            } else {
                const std::uint32_t dividend=(std::uint32_t(regs_[2])<<16)|regs_[0];
                const std::uint32_t quotient=dividend/operand;
                if(quotient>0xffff){interrupt(0);break;}
                regs_[0]=WORD(quotient);
                regs_[2]=WORD(dividend%operand);
            }
            break;
        }
        case 7: { // IDIV; INT 0 on zero divisor or quotient overflow
            if(operand==0){interrupt(0);break;}
            if(bits==8) {
                const int dividend=static_cast<std::int16_t>(regs_[0]);
                const int divisor=static_cast<std::int8_t>(BYTE(operand));
                const int quotient=dividend/divisor;
                if(quotient < -128 || quotient > 127){interrupt(0);break;}
                const int remainder=dividend%divisor;
                regs_[0]=WORD((unsigned(BYTE(remainder))<<8)|BYTE(quotient));
            } else {
                const std::uint32_t raw=(std::uint32_t(regs_[2])<<16)|regs_[0];
                const std::int64_t dividend=static_cast<std::int32_t>(raw);
                const std::int64_t divisor=static_cast<std::int16_t>(WORD(operand));
                const std::int64_t quotient=dividend/divisor;
                if(quotient < -32768 || quotient > 32767){interrupt(0);break;}
                regs_[0]=WORD(quotient);
                regs_[2]=WORD(dividend%divisor);
            }
            break;
        }
        }
        break;
    }
    case 0xff: {
        // Group 5: /0 INC /1 DEC /2 near CALL /3 far CALL,
        //          /4 near JMP /5 far JMP /6 PUSH.
        const auto op=decodeModRm();
        if(op.reg==7)throw std::runtime_error("invalid FF group opcode");
        if(op.reg==0 || op.reg==1) {
            writeOperand(op,16,incDec(readOperand(op,16),16,op.reg==1));
        } else if(op.reg==3 || op.reg==5) {
            if(op.registerOperand)throw std::runtime_error("far indirect pointer requires memory");
            const WORD offset=read16(op);
            const WORD segment=WORD(bus_.readMemory(physical(segments_[op.segment],WORD(op.offset+2))) |
                (WORD(bus_.readMemory(physical(segments_[op.segment],WORD(op.offset+3))))<<8));
            if(op.reg==3) {
                const WORD returnCs=segments_[1],returnIp=ip_;
                push16(returnCs);
                push16(returnIp);
            }
            segments_[1]=segment;
            ip_=offset;
        } else {
            const WORD target=read16(op);
            if(op.reg==2) {
                push16(ip_);
                ip_=target;
            } else if(op.reg==4) {
                ip_=target;
            } else { // /6 PUSH r/m16
                push16(target);
            }
        }
        break;
    }
    case 0xfe: {
        const auto op=decodeModRm();
        if(op.reg>1)throw std::runtime_error("invalid FE opcode");
        writeOperand(op,8,incDec(readOperand(op,8),8,op.reg==1));
        break;
    }
    case 0x80: case 0x81: case 0x82: case 0x83: {
        const auto op=decodeModRm();
        const unsigned bits=(opcode==0x81 || opcode==0x83)?16u:8u;
        const unsigned immediate=opcode==0x81 ? fetch16() :
            opcode==0x83 ? WORD(static_cast<std::int8_t>(fetch8())) : fetch8();
        const unsigned result=alu(op.reg,readOperand(op,bits),immediate,bits);
        if(op.reg!=7)writeOperand(op,bits,result);
        break;
    }
    case 0xa0: { // MOV AL,moffs8 (DS)
        const WORD off=fetch16();
        setReg8(0,bus_.readMemory(physical(segments_[segmentOverride_>=0?unsigned(segmentOverride_):3u],off)));
        break;
    }
    case 0xa1: { // MOV AX,moffs16
        const WORD off=fetch16();
        const BYTE lo=bus_.readMemory(physical(segments_[segmentOverride_>=0?unsigned(segmentOverride_):3u],off));
        const BYTE hi=bus_.readMemory(physical(segments_[segmentOverride_>=0?unsigned(segmentOverride_):3u],WORD(off+1)));
        regs_[0]=WORD(lo|(WORD(hi)<<8));
        break;
    }
    case 0xa2: { // MOV moffs8,AL
        const WORD off=fetch16();
        bus_.writeMemory(physical(segments_[segmentOverride_>=0?unsigned(segmentOverride_):3u],off),getReg8(0));
        break;
    }
    case 0xa3: { // MOV moffs16,AX
        const WORD off=fetch16();
        bus_.writeMemory(physical(segments_[segmentOverride_>=0?unsigned(segmentOverride_):3u],off),BYTE(regs_[0]));
        bus_.writeMemory(physical(segments_[segmentOverride_>=0?unsigned(segmentOverride_):3u],WORD(off+1)),BYTE(regs_[0]>>8));
        break;
    }
    case 0xc4: case 0xc5: { // LES/LDS r16,m16:16
        const auto op=decodeModRm();
        if(op.registerOperand)throw std::runtime_error("LDS/LES requires memory pointer");
        const WORD off=read16(op);
        const WORD segOff=WORD(op.offset+2);
        const WORD segment=WORD(bus_.readMemory(physical(segments_[op.segment],segOff)) |
            (WORD(bus_.readMemory(physical(segments_[op.segment],WORD(segOff+1))))<<8));
        regs_[op.reg]=off;
        segments_[opcode==0xc4?0:3]=segment;
        break;
    }
    case 0xd7: { // XLAT: AL = byte at DS:[BX+AL], source segment overridable
        const WORD off=WORD(regs_[3]+getReg8(0));
        const unsigned seg=segmentOverride_>=0?unsigned(segmentOverride_):3u;
        setReg8(0,bus_.readMemory(physical(segments_[seg],off)));
        break;
    }
    case 0x8d: { // LEA r16,m - register operand is invalid
        const auto op=decodeModRm();
        if(op.registerOperand)throw std::runtime_error("invalid LEA register operand");
        regs_[op.reg]=op.offset;
        break;
    }
    case 0x84: case 0x85: { // TEST r/m,reg - flags only
        const auto op=decodeModRm();
        const unsigned result=opcode==0x84
            ? (read8(op)&getReg8(op.reg))
            : (read16(op)&regs_[op.reg]);
        setLogicFlags(result,opcode==0x84?8:16);
        break;
    }
    case 0xa8: setLogicFlags(getReg8(0)&fetch8(),8);break; // TEST AL,imm8
    case 0xa9: setLogicFlags(regs_[0]&fetch16(),16);break; // TEST AX,imm16
    case 0x86: case 0x87: { // XCHG r/m,reg
        const auto op=decodeModRm();
        if(opcode==0x86) {
            const BYTE old=read8(op);
            write8(op,getReg8(op.reg));
            setReg8(op.reg,old);
        } else {
            const WORD old=read16(op);
            write16(op,regs_[op.reg]);
            regs_[op.reg]=old;
        }
        break;
    }
    case 0x88: {auto op=decodeModRm();write8(op,getReg8(op.reg));break;} // MOV r/m8,r8
    case 0x89: {auto op=decodeModRm();write16(op,regs_[op.reg]);break;} // MOV r/m16,r16
    case 0x8a: {auto op=decodeModRm();setReg8(op.reg,read8(op));break;} // MOV r8,r/m8
    case 0x8b: {auto op=decodeModRm();regs_[op.reg]=read16(op);break;} // MOV r16,r/m16
    case 0xc6: {auto op=decodeModRm();if(op.reg!=0)throw std::runtime_error("invalid MOV C6 group");write8(op,fetch8());break;}
    case 0xc7: {auto op=decodeModRm();if(op.reg!=0)throw std::runtime_error("invalid MOV C7 group");write16(op,fetch16());break;}
    case 0x8c: {auto op=decodeModRm();if(op.reg>=4)throw std::runtime_error("invalid MOV segment");write16(op,segments_[op.reg]);break;}
    case 0x8e: {auto op=decodeModRm();if(op.reg>=4 || op.reg==1)throw std::runtime_error("invalid MOV segment");segments_[op.reg]=read16(op);if(op.reg==2)irqInhibit_=1;break;}
    // 8088 I/O instructions: immediate ports are zero-extended to 16 bits.
    // Word accesses use two successive byte ports (including FFFF->0000).
    case 0xe4: setReg8(0,bus_.readIo(fetch8()));break; // IN AL,imm8
    case 0xe5: { // IN AX,imm8
        const WORD port=fetch8();
        regs_[0]=WORD(bus_.readIo(port) |
                      (WORD(bus_.readIo(WORD(port+1)))<<8));
        break;
    }
    case 0xec: setReg8(0,bus_.readIo(regs_[2]));break; // IN AL,DX
    case 0xed: { // IN AX,DX
        const WORD port=regs_[2];
        regs_[0]=WORD(bus_.readIo(port) |
                      (WORD(bus_.readIo(WORD(port+1)))<<8));
        break;
    }
    case 0xe6: { // OUT imm8,AL
        const WORD port=fetch8();
        bus_.writeIo(port,getReg8(0));
        break;
    }
    case 0xe7: { // OUT imm8,AX
        const WORD port=fetch8();
        bus_.writeIo(port,BYTE(regs_[0]));
        bus_.writeIo(WORD(port+1),BYTE(regs_[0]>>8));
        break;
    }
    case 0xee: bus_.writeIo(regs_[2],getReg8(0));break; // OUT DX,AL
    case 0xef: { // OUT DX,AX
        const WORD port=regs_[2];
        bus_.writeIo(port,BYTE(regs_[0]));
        bus_.writeIo(WORD(port+1),BYTE(regs_[0]>>8));
        break;
    }
    case 0x90: // NOP
        break;
    case 0xf4: // HLT
        halted_=true;
        break;
    case 0xfa: flags_ &= WORD(~0x0200);break; // CLI
    case 0xfb: flags_ |= 0x0200;irqInhibit_=1;break; // STI
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
        if(!interruptModeStack_.empty()) {
            emulation8080_=interruptModeStack_.back();
            interruptModeStack_.pop_back();
        }
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
    case 0x0f: { // POP CS on 8088; V20 bit manipulation escape
        if(variant_==CpuType::I8088) {
            segments_[1]=pop16();
            break;
        }
        const BYTE ext=fetch8();
        if(ext==0xff) { // BRKEM imm8: INT-style transfer into 8080 mode
            const BYTE vector=fetch8();
            interrupt(vector);
            emulation8080_=true;
            break;
        }
        if(ext==0x31 || ext==0x33 || ext==0x39 || ext==0x3b) {
            // NEC V20 INS/EXT. The ModR/M byte encodes 8-bit registers,
            // not a memory effective address: r/m selects the start bit
            // register, reg selects the bit length register (/0 for imm4).
            const BYTE spec=fetch8();
            if((spec&0xc0)!=0xc0)
                throw std::runtime_error("NEC INS/EXT requires register operands");
            const unsigned startReg=spec&7u;
            const unsigned lengthReg=(spec>>3)&7u;
            const bool immediate=(ext&8u)!=0;
            if(immediate && lengthReg!=0)
                throw std::runtime_error("invalid NEC INS/EXT immediate encoding");
            const unsigned count=((immediate?fetch8():getReg8(lengthReg))&15u)+1u;
            const unsigned start=getReg8(startReg)&15u;
            const bool insert=(ext==0x31 || ext==0x39);
            const WORD seg=segments_[insert ? 0u :
                (segmentOverride_>=0 ? unsigned(segmentOverride_) : 3u)];
            const WORD origin=regs_[insert ? 7u : 6u];
            unsigned value=insert ? regs_[0] : 0u;
            for(unsigned bit=0;bit<count;++bit) {
                const unsigned position=start+bit;
                const WORD offset=WORD(origin+position/8u);
                const auto address=physical(seg,offset);
                const unsigned mask=1u<<(position&7u);
                const BYTE current=bus_.readMemory(address);
                if(insert) {
                    const BYTE changed=(value&(1u<<bit))
                        ? BYTE(current|mask) : BYTE(current&~mask);
                    bus_.writeMemory(address,changed);
                } else if(current&mask) {
                    value|=1u<<bit;
                }
            }
            if(!insert)regs_[0]=WORD(value);
            const unsigned end=start+count;
            regs_[insert ? 7u : 6u]=WORD(origin+end/8u);
            setReg8(startReg,BYTE(end&7u));
            // FLAGS are unaffected by INS/EXT.
            break;
        }
        if(ext==0x20 || ext==0x22 || ext==0x26) {
            // NEC V20 packed-BCD string operations: ADD4S, SUB4S, CMP4S.
            // CL counts digits, not bytes. SI, DI and CL do not change.
            // The source segment can be overridden, the destination stays ES.
            const unsigned digits=getReg8(1);
            const unsigned bytes=(digits+1u)/2u;
            const WORD sourceSegment=segments_[segmentOverride_>=0
                ? unsigned(segmentOverride_) : 3u];
            const WORD targetSegment=segments_[0];
            unsigned carry=0;
            bool allZero=true;
            for(unsigned index=0;index<bytes;++index) {
                const WORD sourceOffset=WORD(regs_[6]+index);
                const WORD targetOffset=WORD(regs_[7]+index);
                const BYTE src=bus_.readMemory(physical(sourceSegment,sourceOffset));
                const BYTE dst=bus_.readMemory(physical(targetSegment,targetOffset));
                unsigned out=0;
                for(unsigned shift=0;shift<=4;shift+=4) {
                    const unsigned a=(dst>>shift)&15u;
                    const unsigned b=(src>>shift)&15u;
                    unsigned digit=0;
                    if(ext==0x20) {
                        const unsigned sum=a+b+carry;
                        carry=sum>9u;
                        digit=sum>=10u?sum-10u:sum;
                    } else {
                        const int difference=int(a)-int(b)-int(carry);
                        carry=difference<0;
                        digit=difference<0 ? unsigned(difference+10)
                                            : unsigned(difference);
                    }
                    out|=(digit&15u)<<shift;
                }
                if(out!=0)allZero=false;
                if(ext!=0x26)
                    bus_.writeMemory(physical(targetSegment,targetOffset),BYTE(out));
            }
            // For odd CL, NEC operates on the entire final byte. Its upper
            // nibble and the resulting CF/ZF are not generally meaningful.
            // Only ZF/CF are defined for valid even-length BCD strings.
            if(bytes!=0) {
                flags_=WORD(flags_&~(CF|ZF));
                if(carry)flags_|=CF;
                if(allZero)flags_|=ZF;
            }
            break;
        }
        if(ext==0x28 || ext==0x2a) { // NEC ROL4 / ROR4, r/m8
            const auto operand=decodeModRm();
            if(operand.reg!=0)
                throw std::runtime_error("invalid NEC nibble-rotate ModR/M");
            const BYTE oldValue=read8(operand);
            const BYTE al=getReg8(0);
            const BYTE low=BYTE(al&0x0f);
            const BYTE newValue=ext==0x28
                ? BYTE((unsigned(oldValue)<<4)|low)
                : BYTE((unsigned(low)<<4)|(oldValue>>4));
            const BYTE newLow=ext==0x28 ? BYTE(oldValue>>4)
                                          : BYTE(oldValue&0x0f);
            // Save AL's low nibble first, since the r/m8 target may be AL.
            // NEC leaves AL's high nibble undefined; keep it unchanged.
            write8(operand,newValue);
            setReg8(0,BYTE((al&0xf0)|newLow));
            // Neither operation modifies FLAGS.
            break;
        }
        if(ext<0x10 || ext>0x1f)
            throw std::runtime_error("NEC V20 extended opcode not implemented");
        const unsigned operation=(ext>>1)&3; // TEST1, CLR1, SET1, NOT1
        const unsigned bits=(ext&1)?16u:8u;
        const auto operand=decodeModRm();
        if(operand.reg!=0)
            throw std::runtime_error("invalid NEC bit ModR/M extension");
        const unsigned bitIndex=((ext&8)?fetch8():getReg8(1))&(bits-1);
        const unsigned bitMask=1u<<bitIndex;
        const unsigned value=readOperand(operand,bits);
        if(operation==0) {
            // TEST1 affects ZF only, unlike the 8088 TEST instruction.
            if(value&bitMask)flags_&=WORD(~ZF);
            else flags_|=ZF;
        } else {
            const unsigned updated=operation==1 ? (value&~bitMask)
                : operation==2 ? (value|bitMask) : (value^bitMask);
            writeOperand(operand,bits,updated);
        }
        break;
    }
    case 0xd8: case 0xd9: case 0xda: case 0xdb:
    case 0xdc: case 0xdd: case 0xde: case 0xdf: {
        // ESC is an 8087 coprocessor opcode. Consume ModR/M and any
        // displacement, but with no 8087 attached do not access operands.
        // Decode only: the future 8087 adapter will need the full operation.
        (void)decodeModRm();
        break;
    }
    case 0x60: { // NEC V20: PUSHA (80186-compatible)
        if(variant_!=CpuType::NECV20)
            throw std::runtime_error("opcode requires NEC V20");
        const WORD originalSp=regs_[4];
        for(unsigned index=0;index<8;++index)
            push16(index==4?originalSp:regs_[index]);
        break;
    }
    case 0x61: { // NEC V20: POPA; discard saved SP
        if(variant_!=CpuType::NECV20)
            throw std::runtime_error("opcode requires NEC V20");
        for(int index=7;index>=0;--index) {
            const WORD value=pop16();
            if(index!=4)regs_[unsigned(index)]=value;
        }
        break;
    }
    case 0x68: // NEC V20: PUSH imm16
        if(variant_!=CpuType::NECV20)
            throw std::runtime_error("opcode requires NEC V20");
        push16(fetch16());
        break;
    case 0x6a: // NEC V20: PUSH sign-extended imm8
        if(variant_!=CpuType::NECV20)
            throw std::runtime_error("opcode requires NEC V20");
        push16(WORD(static_cast<std::int8_t>(fetch8())));
        break;
    case 0x69: case 0x6b: { // NEC V20: signed IMUL r16,r/m16,imm
        if(variant_!=CpuType::NECV20)
            throw std::runtime_error("opcode requires NEC V20");
        const auto operand=decodeModRm();
        const std::int32_t source=static_cast<std::int16_t>(read16(operand));
        const std::int32_t immediate=opcode==0x69
            ? static_cast<std::int16_t>(fetch16())
            : static_cast<std::int8_t>(fetch8());
        const std::int32_t result=source*immediate;
        regs_[operand.reg]=WORD(result);
        const bool overflow=result<-32768 || result>32767;
        flags_=WORD(flags_&~(CF|OF));
        if(overflow)flags_|=WORD(CF|OF);
        break;
    }
    case 0xc8: { // NEC V20: ENTER frame_size, nesting_level
        if(variant_!=CpuType::NECV20)
            throw std::runtime_error("opcode requires NEC V20");
        const WORD allocation=fetch16();
        const unsigned nesting=fetch8()&31u;
        push16(regs_[5]);
        const WORD frame=regs_[4];
        if(nesting>0) {
            WORD previous=regs_[5];
            for(unsigned i=1;i<nesting;++i) {
                previous=WORD(previous-2);
                const auto a=physical(segments_[2],previous);
                const WORD value=WORD(bus_.readMemory(a)|
                    (WORD(bus_.readMemory(physical(segments_[2],WORD(previous+1))))<<8));
                push16(value);
            }
            push16(frame);
        }
        regs_[5]=frame;
        regs_[4]=WORD(regs_[4]-allocation);
        break;
    }
    case 0xc9: // NEC V20: LEAVE
        if(variant_!=CpuType::NECV20)
            throw std::runtime_error("opcode requires NEC V20");
        regs_[4]=regs_[5];
        regs_[5]=pop16();
        break;
    case 0x06: push16(segments_[0]);break; // PUSH ES
    case 0x0e: push16(segments_[1]);break; // PUSH CS
    case 0x16: push16(segments_[2]);break; // PUSH SS
    case 0x1e: push16(segments_[3]);break; // PUSH DS
    case 0x07: segments_[0]=pop16();break;
    case 0x17: segments_[2]=pop16();irqInhibit_=1;break;
    case 0x1f: segments_[3]=pop16();break;
    default:
        if(opcode<=0x3b && (opcode&7)<=3 && (opcode&4)==0) {
            executeAluModRm(opcode);
            break;
        }
        if(opcode>=0x70 && opcode<=0x7f) {
            const auto displacement=static_cast<std::int8_t>(fetch8());
            if(jumpCondition(opcode&15)) ip_=WORD(ip_+displacement);
            break;
        }
        if(opcode>=0x40 && opcode<=0x4f) {
            const unsigned index=opcode&7;
            regs_[index]=WORD(incDec(regs_[index],16,(opcode&8)!=0));
            break;
        }
        if(opcode>=0x91 && opcode<=0x97) {
            const unsigned index=opcode-0x90;
            const WORD previous=regs_[0];
            regs_[0]=regs_[index];
            regs_[index]=previous;
            break;
        }
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
