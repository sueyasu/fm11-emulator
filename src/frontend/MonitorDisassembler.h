#pragma once
#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>

namespace MonitorDisassembler {
enum class Architecture { M6809, HD6309, Z80, I8088, V20 };
struct Instruction {unsigned size;std::string mnemonic;};
using Reader=std::function<std::uint8_t(std::uint32_t)>;
inline std::string hex(unsigned v,unsigned digits=2) {
    std::ostringstream s;
    s<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(digits)<<v;
    return s.str();
}
// A conservative decoder: unknown instructions are emitted as single-byte
// data, never guessed lengths. Readers must not acknowledge memory-mapped I/O.
inline Instruction decode6809(const Reader& rd,unsigned pc,bool hd) {
    const auto b=[&](unsigned i){return unsigned(rd((pc+i)&0xffff));};
    const unsigned op=b(0);
    unsigned n=1;
    const unsigned page=(op==0x10||op==0x11)?op:0;
    const unsigned c=page?b(n++):op;
    auto done=[&](const std::string& s){return Instruction{n,s};};
    auto byte=[&](){return b(n++);};
    auto word=[&](){const auto x=(b(n)<<8)|b(n+1);n+=2;return x;};
    auto imm8=[&](){return "#$"+hex(byte());};
    auto imm16=[&](){return "#$"+hex(word(),4);};
    auto direct=[&](){return "$"+hex(byte());};
    auto extended=[&](){return "$"+hex(word(),4);};
    auto indexed=[&](){
        const unsigned p=byte(),mode=p&15;
        const char* regs[]={"X","Y","U","S"};
        const std::string r=regs[(p>>5)&3];
        if(!(p&0x80)) {
            const int offset=(p&31)<16?int(p&31):int(p&31)-32;
            return std::to_string(offset)+","+r;
        }
        std::string v;
        switch(mode) {
            case 0:v=","+r+"+";break;
            case 1:v=","+r+"++";break;
            case 2:v=",-"+r;break;
            case 3:v=",--"+r;break;
            case 4:v=","+r;break;
            case 5:v="B,"+r;break;
            case 6:v="A,"+r;break;
            case 8:v="$"+hex(byte())+","+r;break;
            case 9:v="$"+hex(word(),4)+","+r;break;
            case 11:v="D,"+r;break;
            case 12:v="$"+hex(byte())+",PC";break;
            case 13:v="$"+hex(word(),4)+",PC";break;
            case 15:v="$"+hex(word(),4);break;
            default:v="?";break;
        }
        return (p&0x10)?"["+v+"]":v;
    };
    static constexpr const char* cond[]={"BRA","BRN","BHI","BLS","BCC","BCS","BNE","BEQ",
                                        "BVC","BVS","BPL","BMI","BGE","BLT","BGT","BLE"};
    if(page==0 && c>=0x20 && c<=0x2f) {
        const auto d=static_cast<std::int8_t>(byte());
        return done(std::string(cond[c-0x20])+" $"+hex((pc+n+d)&0xffff,4));
    }
    if(page==0x10 && c>=0x21 && c<=0x2f) {
        const auto d=static_cast<std::int16_t>(word());
        return done(std::string("L")+cond[c-0x20]+" $"+hex((pc+n+d)&0xffff,4));
    }
    if(page==0 && (c==0x16 || c==0x17)) {
        const auto d=static_cast<std::int16_t>(word());
        return done(std::string(c==0x16?"LBRA":"LBSR")+" $"+hex((pc+n+d)&0xffff,4));
    }
    if(page==0 && c==0x8d) {
        const auto d=static_cast<std::int8_t>(byte());
        return done("BSR $"+hex((pc+n+d)&0xffff,4));
    }
    if(page==0) {
        switch(c) {
            case 0x12:return done("NOP");
            case 0x13:return done("SYNC");
            case 0x19:return done("DAA");
            case 0x1a:return done("ORCC "+imm8());
            case 0x1c:return done("ANDCC "+imm8());
            case 0x1d:return done("SEX");
            case 0x1e:case 0x1f:{
                const unsigned p=byte();
                static constexpr const char* reg[]={"D","X","Y","U","S","PC","?","?","A","B","CC","DP","?","?","?","?"};
                return done(std::string(c==0x1e?"EXG ":"TFR ")+reg[p>>4]+","+reg[p&15]);
            }
            case 0x30:case 0x31:case 0x32:case 0x33:{
                static constexpr const char* names[]={"LEAX ","LEAY ","LEAS ","LEAU "};
                return done(std::string(names[c-0x30])+indexed());
            }
            case 0x34:case 0x35:case 0x36:case 0x37:
                return done(std::string((c&1)?"PUL":"PSH")+((c&2)?"U ":"S ")+"#$"+hex(byte()));
            case 0x39:return done("RTS");
            case 0x3a:return done("ABX");
            case 0x3b:return done("RTI");
            case 0x3c:return done("CWAI "+imm8());
            case 0x3d:return done("MUL");
            case 0x3f:return done("SWI");
        }
        const unsigned hi=c>>4,lo=c&15;
        static constexpr const char* unary[]={"NEG","","","COM","LSR","","ROR","ASR","ASL","ROL","DEC","","INC","TST","JMP","CLR"};
        if((hi==0||hi==4||hi==5||hi==6||hi==7) &&
           unary[lo][0] && !((hi==4||hi==5) && lo==14)) {
            const std::string m=unary[lo];
            if(hi==0)return done(m+" "+direct());
            if(hi==4||hi==5)return done(m+(hi==4?"A":"B"));
            return done(m+" "+(hi==6?indexed():extended()));
        }
    }
    const unsigned hi=c>>4,lo=c&15,mode=hi&3;
    if(page==0 && hi>=8) {
        static constexpr const char* a[]={"SUBA","CMPA","SBCA","SUBD","ANDA","BITA","LDA","STA","EORA","ADCA","ORA","ADDA","CMPX","JSR","LDX","STX"};
        static constexpr const char* bnames[]={"SUBB","CMPB","SBCB","ADDD","ANDB","BITB","LDB","STB","EORB","ADCB","ORB","ADDB","LDD","STD","LDU","STU"};
        if(mode==0 && (lo==7||lo==15||(hi>=12&&lo==13)||lo==13))return {1,"FCB $"+hex(op)};
        const bool wide=lo==3||lo==12||lo==14||lo==15||(hi>=12&&lo==13);
        const auto operand=mode==0?(wide?imm16():imm8()):mode==1?direct():mode==2?indexed():extended();
        return done(std::string(hi>=12?bnames[lo]:a[lo])+" "+operand);
    }
    if(page==0x10 && c==0x3f)return done("SWI2");
    if(page==0x11 && c==0x3f)return done("SWI3");
    if(page==0x10 && c>=0x83 && c<=0xff) {
        const unsigned lo=c&15,mode=(c>>4)&3;
        std::string mn;
        switch(lo) {
            case 3:if(c<0xc0)mn="CMPD";break;
            case 12:if(c<0xc0)mn="CMPY";break;
            case 14:mn=c<0xc0?"LDY":"LDS";break;
            case 15:mn=c<0xc0?"STY":"STS";break;
        }
        if(!mn.empty() && !(mode==0 && lo==15))
            return done(mn+" "+(mode==0?imm16():mode==1?direct():mode==2?indexed():extended()));
    }
    if(page==0x11 && ((c>=0x83 && c<=0xb3)||(c>=0xc3 && c<=0xf3)) && (c&15)==3) {
        const unsigned mode=(c>>4)&3;
        return done(std::string(c<0xc0?"CMPU":"CMPS")+" "+
                    (mode==0?imm16():mode==1?direct():mode==2?indexed():extended()));
    }
    (void)hd;
    return {1,"FCB $"+hex(op)};
}
inline Instruction decodeZ80(const Reader& rd,unsigned pc) {
    const auto b=[&](unsigned i){return unsigned(rd((pc+i)&0xffff));};
    unsigned n=0,op=b(n++);
    const char* index=nullptr;
    if(op==0xdd||op==0xfd){index=op==0xdd?"IX":"IY";op=b(n++);}
    auto done=[&](const std::string& s){return Instruction{n,s};};
    auto byte=[&](){return b(n++);};
    auto word=[&](){const unsigned v=byte();return v|(byte()<<8);};
    auto imm8=[&](){return "$"+hex(byte());};
    auto imm16=[&](){return "$"+hex(word(),4);};
    auto indexed=[&](){
        const int off=static_cast<std::int8_t>(byte());
        return std::string("(")+index+(off<0?"-":"+")+"$"+hex(off<0?-off:off)+")";
    };
    static constexpr const char* r[]={"B","C","D","E","H","L","(HL)","A"};
    static constexpr const char* pairs[]={"BC","DE","HL","SP"};
    static constexpr const char* pairs2[]={"BC","DE","HL","AF"};
    static constexpr const char* conditions[]={"NZ","Z","NC","C","PO","PE","P","M"};
    static constexpr const char* alu[]={"ADD A,","ADC A,","SUB ","SBC A,","AND ","XOR ","OR ","CP "};
    const unsigned x=op>>6,y=(op>>3)&7,z=op&7,p=y>>1,q=y&1;
    auto rr=[&](unsigned v){return v==6 && index?indexed():index && (v==4||v==5)?std::string(index)+(v==4?"H":"L"):std::string(r[v]);};
    auto pair=[&](unsigned v,bool af=false){return std::string(index&&v==2?index:(af?pairs2[v]:pairs[v]));};
    if(op==0xcb) {
        const bool withIndex=index!=nullptr;
        const int d=withIndex?static_cast<std::int8_t>(byte()):0;
        const unsigned cb=byte(),cy=(cb>>3)&7,cz=cb&7,cx=cb>>6;
        static constexpr const char* rot[]={"RLC","RRC","RL","RR","SLA","SRA","SLL","SRL"};
        const auto dst=withIndex?std::string("(")+index+(d<0?"-":"+")+"$"+hex(d<0?-d:d)+")":std::string(r[cz]);
        return done(cx==0?std::string(rot[cy])+" "+dst:std::string(cx==1?"BIT ":cx==2?"RES ":"SET ")+std::to_string(cy)+","+dst);
    }
    if(op==0xed) {
        const unsigned ed=byte(),ex=ed>>6,ey=(ed>>3)&7,ez=ed&7;
        static constexpr const char* edBlock[4][4]={
            {"LDI","CPI","INI","OUTI"},{"LDD","CPD","IND","OUTD"},
            {"LDIR","CPIR","INIR","OTIR"},{"LDDR","CPDR","INDR","OTDR"}};
        if(ex==2 && ey>=4 && ez<=3)
            return done(edBlock[ey-4][ez]);
        if(ex==1 && ez==0)
            return done(std::string("IN ")+(ey==6?"(C)":std::string(r[ey])+",(C)"));
        if(ex==1 && ez==1)
            return done(std::string("OUT (C),")+(ey==6?"0":r[ey]));
        if(ex==1 && ez==2)
            return done(std::string(ey&1?"ADC HL,":"SBC HL,")+pair(ey>>1));
        if(ed==0x67)return done("RRD");
        if(ed==0x6f)return done("RLD");
        if(ed==0x47)return done("LD I,A");
        if(ed==0x4f)return done("LD R,A");
        if(ed==0x57)return done("LD A,I");
        if(ed==0x5f)return done("LD A,R");
        if(ex==1&&ez==4)return done("NEG");
        if(ex==1&&ez==5)return done(ey&1?"RETI":"RETN");
        if(ex==1&&ez==6)return done("IM "+std::to_string((ey==2||ey==6)?1:(ey==3||ey==7)?2:0));
        if(ex==1&&ez==3) {
            const auto address=imm16();
            return done(ey&1?"LD "+pair(ey>>1)+",("+address+")":"LD ("+address+"),"+pair(ey>>1));
        }
        return done("DB $ED,$"+hex(ed));
    }
    if(x==1) {
        if(op==0x76)return done("HALT");
        const auto lhs=rr(y),rhs=rr(z);
        return done("LD "+lhs+","+rhs);
    }
    if(x==2)return done(std::string(alu[y])+rr(z));
    if(x==0) {
        if(z==0) {
            if(y==0)return done("NOP");
            if(y==1)return done("EX AF,AF'");
            const int d=static_cast<std::int8_t>(byte());
            if(y==2)return done("DJNZ "+std::to_string(d));
            if(y==3)return done("JR "+std::to_string(d));
            return done(std::string("JR ")+conditions[y-4]+","+std::to_string(d));
        }
        if(z==1) {if(q)return done("ADD "+pair(2)+","+pair(p));const auto operand=imm16();return done("LD "+pair(p)+","+operand);}
        if(z==2) {
            if(p<2)return done(q?"LD A,("+pair(p)+")":"LD ("+pair(p)+"),A");
            const auto adr=imm16();
            return done(q?"LD "+std::string(p==2?pair(2):"A")+",("+adr+")":"LD ("+adr+"),"+std::string(p==2?pair(2):"A"));
        }
        if(z==3)return done(std::string(q?"DEC ":"INC ")+pair(p));
        if(z==4)return done("INC "+rr(y));
        if(z==5)return done("DEC "+rr(y));
        if(z==6){const auto dst=rr(y);return done("LD "+dst+","+imm8());}
        static constexpr const char* misc[]={"RLCA","RRCA","RLA","RRA","DAA","CPL","SCF","CCF"};
        return done(misc[y]);
    }
    if(z==0)return done(std::string("RET ")+conditions[y]);
    if(z==1) {
        if(!q)return done("POP "+pair(p,true));
        if(p==0)return done("RET");
        if(p==1)return done("EXX");
        if(p==2)return done("JP ("+pair(2)+")");
        return done("LD SP,"+pair(2));
    }
    if(z==2)return done(std::string("JP ")+conditions[y]+","+imm16());
    if(z==3) {
        if(y==0)return done("JP "+imm16());
        if(y==2)return done("OUT ("+imm8()+"),A");
        if(y==3)return done("IN A,("+imm8()+")");
        if(y==4)return done("EX (SP),"+pair(2));
        if(y==5)return done("EX DE,HL");
        if(y==6)return done("DI");
        if(y==7)return done("EI");
    }
    if(z==4)return done(std::string("CALL ")+conditions[y]+","+imm16());
    if(z==5) {
        if(!q)return done("PUSH "+pair(p,true));
        if(p==0)return done("CALL "+imm16());
    }
    if(z==6)return done(std::string(alu[y])+imm8());
    if(z==7)return done("RST $"+hex(y*8));
    return {1,"DB $"+hex(b(0))};
}
inline Instruction decode8088(const Reader& rd,unsigned pc) {
    const auto b=[&](unsigned i){return unsigned(rd((pc+i)&0xfffff));};
    unsigned n=1,op=b(0);
    auto done=[&](const std::string& s){return Instruction{n,s};};
    auto byte=[&](){return b(n++);};
    auto word=[&](){const unsigned x=byte();return x|(byte()<<8);};
    static constexpr const char* r8[]={"AL","CL","DL","BL","AH","CH","DH","BH"};
    static constexpr const char* r16[]={"AX","CX","DX","BX","SP","BP","SI","DI"};
    if(op>=0xb0 && op<=0xb7)return done(std::string("MOV ")+r8[op-0xb0]+",$"+hex(byte()));
    if(op>=0xb8 && op<=0xbf)return done(std::string("MOV ")+r16[op-0xb8]+",$"+hex(word(),4));
    if(op>=0x40 && op<=0x47)return done(std::string("INC ")+r16[op-0x40]);
    if(op>=0x48 && op<=0x4f)return done(std::string("DEC ")+r16[op-0x48]);
    if(op>=0x50 && op<=0x57)return done(std::string("PUSH ")+r16[op-0x50]);
    if(op>=0x58 && op<=0x5f)return done(std::string("POP ")+r16[op-0x58]);
    switch(op) {
        case 0x90:return done("NOP");
        case 0xc3:return done("RET");
        case 0xcb:return done("RETF");
        case 0xcf:return done("IRET");
        case 0xf4:return done("HLT");
        case 0xfa:return done("CLI");
        case 0xfb:return done("STI");
        case 0xfc:return done("CLD");
        case 0xfd:return done("STD");
        case 0xcc:return done("INT 3");
        case 0xcd:return done("INT $"+hex(byte()));
        case 0xeb:{const int d=static_cast<std::int8_t>(byte());return done("JMP $"+hex((pc+n+d)&0xfffff,5));}
        case 0xe9:{const int d=static_cast<std::int16_t>(word());return done("JMP $"+hex((pc+n+d)&0xfffff,5));}
        case 0xe8:{const int d=static_cast<std::int16_t>(word());return done("CALL $"+hex((pc+n+d)&0xfffff,5));}
        case 0x04:return done("ADD AL,$"+hex(byte()));
        case 0x05:return done("ADD AX,$"+hex(word(),4));
        case 0x2c:return done("SUB AL,$"+hex(byte()));
        case 0x2d:return done("SUB AX,$"+hex(word(),4));
        case 0x3c:return done("CMP AL,$"+hex(byte()));
        case 0x3d:return done("CMP AX,$"+hex(word(),4));
        case 0xa0:return done("MOV AL,[$"+hex(word(),4)+"]");
        case 0xa1:return done("MOV AX,[$"+hex(word(),4)+"]");
        case 0xa2:return done("MOV [$"+hex(word(),4)+"],AL");
        case 0xa3:return done("MOV [$"+hex(word(),4)+"],AX");
    }
    return {1,"DB $"+hex(op)};
}
inline Instruction decode(Architecture cpu,const Reader& reader,unsigned pc) {
    switch(cpu) {
        case Architecture::M6809:return decode6809(reader,pc,false);
        case Architecture::HD6309:return decode6809(reader,pc,true);
        case Architecture::Z80:return decodeZ80(reader,pc);
        case Architecture::I8088:case Architecture::V20:return decode8088(reader,pc);
    }
    return {1,"DB $"+hex(reader(pc))};
}
} // namespace MonitorDisassembler
