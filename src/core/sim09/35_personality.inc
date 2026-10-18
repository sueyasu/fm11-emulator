////////////////////////////////////////////////////////////////
// MC6809 / HD6309 execution personality

static BYTE Read6309Inter8(const REGS& r, BYTE c) {
    switch(c&15) {
    case 8:return r.r.b.A; case 9:return r.r.b.B;
    case 10:return r.CC; case 11:return r.DP;
    case 14:return r.r.b.E; case 15:return r.r.b.F;
    default:return 0;
    }
}
static void Write6309Inter8(REGS& r, BYTE c, BYTE v) {
    switch(c&15) {
    case 8:r.r.b.A=v;break;case 9:r.r.b.B=v;break;
    case 10:r.CC=v;break;case 11:r.DP=v;break;
    case 14:r.r.b.E=v;break;case 15:r.r.b.F=v;break;
    }
}
static WORD Read6309Inter16(const REGS& r, BYTE c) {
    switch(c&15) {
    case 0: case 8: case 9:return r.r.w.D;
    case 1:return r.r.w.X;case 2:return r.r.w.Y;
    case 3:return r.r.w.U;case 4:return r.r.w.S;
    case 5:return r.r.w.PC;case 6:case 14:case 15:return r.r.w.W;
    case 7:return r.r.w.V;default:return 0;
    }
}
static void Write6309Inter16(REGS& r, BYTE c, WORD v) {
    switch(c&15) {
    case 0:case 8:case 9:r.r.w.D=v;break;
    case 1:r.r.w.X=v;break;case 2:r.r.w.Y=v;break;
    case 3:r.r.w.U=v;break;case 4:r.r.w.S=v;break;
    case 5:r.r.w.PC=v;break;case 6:case 14:case 15:r.r.w.W=v;break;
    case 7:r.r.w.V=v;break;
    }
}
static bool Is6309Inter16(BYTE p) {
    return (p&0x80)==0 || (p&0x08)==0;
}


static WORD Read6309TfrRegister(const REGS& r, BYTE code)
{
    switch(code & 15) {
    case 0: return r.r.w.D;
    case 1: return r.r.w.X;
    case 2: return r.r.w.Y;
    case 3: return r.r.w.U;
    case 4: return r.r.w.S;
    case 5: return r.r.w.PC;
    case 6: return r.r.w.W;
    case 7: return r.r.w.V;
    case 8: return WORD((WORD(r.r.b.A)<<8)|r.r.b.A);
    case 9: return WORD((WORD(r.r.b.B)<<8)|r.r.b.B);
    case 10: return WORD((WORD(r.CC)<<8)|r.CC);
    case 11: return WORD((WORD(r.DP)<<8)|r.DP);
    case 12: case 13: return 0;
    case 14: return WORD((WORD(r.r.b.E)<<8)|r.r.b.E);
    case 15: return WORD((WORD(r.r.b.F)<<8)|r.r.b.F);
    }
    return 0;
}
static void Write6309TfrRegister(REGS& r, BYTE code, WORD v)
{
    switch(code & 15) {
    case 0: r.r.w.D=v;break;
    case 1: r.r.w.X=v;break;
    case 2: r.r.w.Y=v;break;
    case 3: r.r.w.U=v;break;
    case 4: r.r.w.S=v;break;
    case 5: r.r.w.PC=v;break;
    case 6: r.r.w.W=v;break;
    case 7: r.r.w.V=v;break;
    case 8: r.r.b.A=BYTE(v>>8);break;
    case 9: r.r.b.B=BYTE(v);break;
    case 10: r.CC=BYTE(v);break;
    case 11: r.DP=BYTE(v>>8);break;
    case 12: case 13: break;
    case 14: r.r.b.E=BYTE(v>>8);break;
    case 15: r.r.b.F=BYTE(v);break;
    }
}


void CSim09::Set6309Enabled(bool enabled)
{
    enable6309_ = enabled;
}

bool CSim09::Is6309Enabled() const
{
    return enable6309_;
}

bool CSim09::IsOpcodeAllowed(WORD opcode) const
{
    if (enable6309_)
        return true;

    if ((opcode & 0xff00) == 0x0000) {
        switch (opcode) {
        case 0x01: // OIM direct
        case 0x02: // AIM direct
        case 0x05: // EIM direct
        case 0x0b: // TIM direct
        case 0xcd: // LDQ immediate
        case 0x14: // SEXW
        case 0x61: // OIM indexed
        case 0x62: // AIM indexed
        case 0x65: // EIM indexed
        case 0x6b: // TIM indexed
        case 0x71: // OIM extended
        case 0x72: // AIM extended
        case 0x75: // EIM extended
        case 0x7b: // TIM extended
            return false;
        default:
            return true;
        }
    }

    if ((opcode & 0xff00) == 0x1000) {
        switch (opcode) {
        case 0x1021: case 0x1022: case 0x1023: case 0x1024:
        case 0x1025: case 0x1026: case 0x1027: case 0x1028:
        case 0x1029: case 0x102a: case 0x102b: case 0x102c:
        case 0x102d: case 0x102e: case 0x102f:
        case 0x103f: // SWI2
        case 0x1083: case 0x108c: case 0x108e:
        case 0x1093: case 0x109c: case 0x109e: case 0x109f:
        case 0x10a3: case 0x10ac: case 0x10ae: case 0x10af:
        case 0x10b3: case 0x10bc: case 0x10be: case 0x10bf:
        case 0x10ce:
        case 0x10de: case 0x10df:
        case 0x10ee: case 0x10ef:
        case 0x10fe: case 0x10ff:
            return true;
        default:
            return false;
        }
    }

    if ((opcode & 0xff00) == 0x1100) {
        switch (opcode) {
        case 0x113f: // SWI3
        case 0x1183: case 0x118c:
        case 0x1193: case 0x119c:
        case 0x11a3: case 0x11ac:
        case 0x11b3: case 0x11bc:
            return true;
        default:
            return false;
        }
    }

    return true;
}

bool CSim09::IsIndexedPostbyteAllowed(BYTE postbyte) const
{
    if (enable6309_ || (postbyte & 0x80) == 0)
        return true;

    // HD6309 adds E,R / F,R / W,R forms, with direct and indirect variants.
    switch (postbyte & 0x0f) {
    case 0x07:
    case 0x0a:
    case 0x0e:
        return false;
    default:
        break;
    }

    // HD6309 also reuses otherwise invalid MC6809 postbytes for W-based modes.
    switch (postbyte) {
    case 0x8f:
    case 0x90:
    case 0xaf:
    case 0xb0:
    case 0xcf:
    case 0xd0:
    case 0xef:
    case 0xf0:
        return false;
    default:
        return true;
    }
}

bool CSim09::IsTransferPostbyteAllowed(BYTE postbyte) const
{
    if (enable6309_)
        return true;

    const auto is6809Register = [](BYTE reg) {
        reg &= 0x0f;
        return reg <= 0x05 || (reg >= 0x08 && reg <= 0x0b);
    };

    return is6809Register(postbyte >> 4) && is6809Register(postbyte);
}

bool CSim09::ConsumeIllegalOperand()
{
    const bool result = illegalOperand_;
    illegalOperand_ = false;
    return result;
}

// Indexed postbytes are decoded by GetAddrInd().  Reject an HD6309-only
// postbyte before the instruction body can read or write the dummy address.
#undef IND
#define IND                                                         \
    ACC_PT = GetAddrInd();                                          \
    if (ConsumeIllegalOperand()) { O_ERR; break; }

// TFR/EXG use an operand byte to select registers.  MC6809 only has
// D/X/Y/U/S/PC and A/B/CC/DP in this encoding; W/V/E/F and the HD6309 zero
// registers must not leak into the MC6809 personality.
#undef O_EXG
#define O_EXG \
    { \
        if (!IsTransferPostbyteAllowed(ACC_BL)) { O_ERR; } \
        else { \
            const BYTE src = ACC_BL >> 4, dst = ACC_BL & 15; \
            const WORD sv = Read6309TfrRegister(regs,src); \
            const WORD dv = Read6309TfrRegister(regs,dst); \
            Write6309TfrRegister(regs,src,dv); \
            Write6309TfrRegister(regs,dst,sv); \
        } \
    }

#undef O_TFR
#define O_TFR \
    { \
        if (!IsTransferPostbyteAllowed(ACC_BL)) { O_ERR; } \
        else Write6309TfrRegister(regs,ACC_BL & 15, \
                                   Read6309TfrRegister(regs,ACC_BL >> 4)); \
    }

// Keep the original instruction bodies intact.  The dispatch macro rejects
// HD6309-only opcodes before their implementation is reached.
#undef OPDEF
#define OPDEF(c,p) case c: if (!IsOpcodeAllowed(c)) { O_ERR; } else { p; } break;
