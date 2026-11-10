#pragma once
#include <cstdint>

// Documented NMOS Z80 T-states. Timings include opcode/prefix fetches.
// The interpreter executes block-repeat instructions atomically; its caller
// adds 21 T-states for each repetition before the final 16-T-state pass.
namespace Z80Timing {
inline unsigned base(std::uint8_t op,bool z,bool c,bool p,bool s) {
    const unsigned x=op>>6, y=(op>>3)&7, zz=op&7;
    const unsigned pair=y>>1;
    const bool condition[]={!z,z,!c,c,!p,p,!s,s};
    if(x==0) {
        switch(zz) {
        case 0:
            if(y<2)return 4;
            if(y==2)return 8; // DJNZ not taken; taken resolved below
            if(y==3)return 12;
            return condition[y-4]?12:7;
        case 1:return (y&1)?11:10;
        case 2:
            if(pair<2)return 7;
            return pair==2?16:13;
        case 3:return 6;
        case 4:case 5:return y==6?11:4;
        case 6:return y==6?10:7;
        case 7:return 4;
        }
    }
    if(x==1) {
        if(op==0x76)return 4;
        return (y==6 || zz==6)?7:4;
    }
    if(x==2)return zz==6?7:4;
    switch(zz) {
    case 0:return condition[y]?11:5;             // RET cc
    case 1:
        if(!(y&1))return 10;                     // POP qq
        return pair==0?10:pair==3?6:4;          // RET,EXX,JP(HL),LD SP,HL
    case 2:return 10;                            // JP cc,nn
    case 3:
        switch(y) {
        case 0:return 10;                        // JP nn
        case 1:return 4;                         // CB: overridden by decoder
        case 2:case 3:return 11;                 // OUT/IN (n)
        case 4:return 19;                        // EX (SP),HL
        default:return 4;
        }
    case 4:return condition[y]?17:10;            // CALL cc,nn
    case 5:
        if(!(y&1))return 11;                     // PUSH qq
        return pair==0?17:4;                    // CALL nn or prefix
    case 6:return 7;                             // ALU n
    case 7:return 11;                            // RST
    }
    return 4;
}

inline unsigned cb(std::uint8_t op,bool indexed) {
    if(indexed)return (op&0xc0)==0x40?20:23;
    if((op&7)!=6)return 8;
    return (op&0xc0)==0x40?12:15;
}

inline unsigned ed(std::uint8_t op) {
    // Undocumented ED opcodes consume 8 T-states (two opcode fetches).
    if((op&0xc7)==0x40 || (op&0xc7)==0x41)return 12; // IN/OUT (C)
    if((op&0xcf)==0x42 || (op&0xcf)==0x4a)return 15; // SBC/ADC HL,rr
    if((op&0xcf)==0x43 || (op&0xcf)==0x4b)return 20; // LD (nn),rr / reverse
    if((op&0xc7)==0x44)return 8;                      // NEG aliases
    if((op&0xc7)==0x45)return 14;                     // RETN/RETI aliases
    if((op&0xc7)==0x46)return 8;                      // IM
    switch(op) {
    case 0x47:case 0x4f:return 9;                     // LD I/R,A
    case 0x57:case 0x5f:return 9;                     // LD A,I/R
    case 0x67:case 0x6f:return 18;                    // RRD / RLD
    case 0xa0:case 0xa1:case 0xa2:case 0xa3:
    case 0xa8:case 0xa9:case 0xaa:case 0xab:
    case 0xb0:case 0xb1:case 0xb2:case 0xb3:
    case 0xb8:case 0xb9:case 0xba:case 0xbb:
        return 16;                                  // block, last iteration
    default:return 8;
    }
}

inline bool blockRepeat(std::uint8_t op) {
    return op==0xb0 || op==0xb1 || op==0xb2 || op==0xb3 ||
           op==0xb8 || op==0xb9 || op==0xba || op==0xbb;
}

inline unsigned index(std::uint8_t op) {
    // Includes the 4-T-state DD/FD prefix. The existing emulator only
    // implements the indexed instructions recognized by z80emuIndex().
    switch(op) {
    case 0x09:case 0x19:case 0x29:case 0x39:return 15;
    case 0x21:return 14;
    case 0x22:case 0x2a:return 20;
    case 0x23:case 0x2b:return 10;
    case 0x34:case 0x35:return 23;
    case 0x36:return 19;
    case 0x46:case 0x4e:case 0x56:case 0x5e:
    case 0x66:case 0x6e:case 0x7e:
    case 0x70:case 0x71:case 0x72:case 0x73:
    case 0x74:case 0x75:case 0x77:
    case 0x86:case 0x8e:case 0x96:case 0x9e:
    case 0xa6:case 0xae:case 0xb6:case 0xbe:return 19;
    case 0xe1:return 14;
    case 0xe3:return 23;
    case 0xe5:return 15;
    case 0xe9:return 8;
    case 0xf9:return 10;
    case 0xcb:return 4; // overwritten by z80emuCB(ix)
    default:return 4;  // only the prefix/next opcode fetch is modeled
    }
}
} // namespace Z80Timing
