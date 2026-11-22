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


// TFM micro-operations: an individual transfer is a memory read followed
// by a memory write. Interrupts can be accepted between these phases.
// The read is intentionally repeated after RTI, as on the HD6309.
// The PC is kept at the instruction while W is nonzero, so the CPU's
// normal interrupt stack contains the TFM restart address.
void CSim09::InterruptTfm()
{
    if (tfmActive_) {
        tfmReadPending_ = false; // discarded read latch: repeat read on RTI
        tfmNeedsSetup_ = true;
        REG_CC &= ~CC_Z; // interrupted TFM clears Z
    }
}

void CSim09::StepTfm(WORD opcode, BYTE postbyte)
{
    const unsigned srcReg = postbyte >> 4, dstReg = postbyte & 15;
    // Do not treat a register number outside D/X/Y/U/S as an address.
    if (srcReg > 4 || dstReg > 4) {
        REG_MD |= MD_X;
        REG_CC = (REG_CC & ~CC_Z) | CC_E;
        PushAll();
        REG_PC = GetMemWord(0xfff0);
        tfmActive_ = false;
        return;
    }
    const WORD instructionPC = WORD(REG_PC - 3);
    if (!tfmActive_ || tfmPC_ != instructionPC ||
        tfmOpcode_ != opcode || tfmPostbyte_ != postbyte) {
        tfmActive_ = true;
        tfmReadPending_ = false;
        tfmNeedsSetup_ = true;
        tfmPC_ = instructionPC;
        tfmOpcode_ = opcode;
        tfmPostbyte_ = postbyte;
    }
    auto tfmRegister = [this](unsigned code) -> WORD& {
        switch (code) {
        case 0: return regs.r.w.D;
        case 1: return regs.r.w.X;
        case 2: return regs.r.w.Y;
        case 3: return regs.r.w.U;
        default: return regs.r.w.S;
        }
    };
    WORD& source = tfmRegister(srcReg);
    WORD& target = tfmRegister(dstReg);
    if (!tfmReadPending_) {
        tfmReadValue_ = GetMemByte(source);
        tfmReadPending_ = true;
        tfmStepCycles_ = tfmNeedsSetup_ ? 7u : 1u;
        tfmNeedsSetup_ = false;
        REG_PC = tfmPC_;
        return;
    }
    // The source update precedes the destination write. If the two
    // register codes are identical (e.g. TFM X+,X+), the destination
    // therefore sees the updated value of the shared register.
    if (opcode == 0x1138 || opcode == 0x113a)
        source = WORD(source + 1);
    else if (opcode == 0x1139)
        source = WORD(source - 1);
    SetMemByte(target,tfmReadValue_);
    if (opcode == 0x1138 || opcode == 0x113b)
        target = WORD(target + 1);
    else if (opcode == 0x1139)
        target = WORD(target - 1);
    REG_W = WORD(REG_W - 1);
    tfmReadPending_ = false;
    tfmStepCycles_ = 2;
    if (REG_W != 0)
        REG_PC = tfmPC_;
    else {
        tfmActive_ = false;
        REG_CC |= CC_Z; // successful TFM sets Z
    }
}

// Signed HD6309 math and condition codes. Source: Atkinson 6x09
// Instruction Sets and hoglet67's hardware-verified 6309 behaviours.
void CSim09::ExecuteMuld(WORD operand)
{
    divisionCycleAdjustment_ =
        ((REG_D & 0x8000) || (operand & 0x8000)) ? 2 : 0;
    const std::int32_t product =
        std::int32_t(std::int16_t(REG_D)) * std::int32_t(std::int16_t(operand));
    REG_Q = std::uint32_t(product);
    REG_CC &= ~(CC_Z | CC_N);
    if ((REG_Q & 0xffff0000u) == 0) REG_CC |= CC_Z;
    if ((REG_Q & 0x80000000u) != 0) REG_CC |= CC_N;
}

void CSim09::ExecuteDivD(BYTE operand)
{
    const int dividend = std::int16_t(REG_D);
    const int divisor = std::int8_t(operand);
    const unsigned magnitude = unsigned(dividend < 0 ? -dividend : dividend);
    divisionCycleAdjustment_ = 0;
    if (divisor == 0) {
        divisionCycleAdjustment_ = nativeTimingMode_ ? 0 : -2;
        REG_CC = (REG_CC & ~(CC_N | CC_V)) | CC_Z | CC_E;
        REG_MD |= MD_D;
        PushAll();
        REG_PC = GetMemWord(0xfff0);
        return;
    }
    const unsigned absDivisor = unsigned(divisor < 0 ? -divisor : divisor);
    const unsigned quotientMagnitude = magnitude / absDivisor;
    divisionCycleAdjustment_ = int(dividend < 0) + int(divisor < 0);
    REG_CC &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (quotientMagnitude > 255u) {
        divisionCycleAdjustment_ -= 13;
        REG_D = WORD(magnitude);
        REG_CC |= CC_V;
        if (dividend < 0) REG_CC |= CC_N;
        return;
    }
    const int signedQuotient = dividend / divisor;
    const int remainder = dividend % divisor;
    if (quotientMagnitude >= 128u) {
        --divisionCycleAdjustment_;
        REG_B = BYTE(quotientMagnitude);
        REG_A = BYTE(remainder);
        REG_CC |= CC_V;
        // Hardware clears N/Z on signed-representation overflow.
    } else {
        REG_B = BYTE(signedQuotient);
        REG_A = BYTE(remainder);
        if (REG_B == 0) REG_CC |= CC_Z;
        if ((REG_B & 0x80) != 0) REG_CC |= CC_N;
    }
    if ((quotientMagnitude & 1u) != 0) REG_CC |= CC_C;
}

void CSim09::ExecuteDivQ(WORD operand)
{
    const std::int64_t dividend = std::int32_t(REG_Q);
    const int divisor = std::int16_t(operand);
    const std::uint64_t magnitude =
        std::uint64_t(dividend < 0 ? -dividend : dividend);
    divisionCycleAdjustment_ = 0;
    if (divisor == 0) {
        divisionCycleAdjustment_ = nativeTimingMode_ ? -8 : -10;
        REG_CC = (REG_CC & ~(CC_N | CC_V)) | CC_Z | CC_E;
        REG_MD |= MD_D;
        PushAll();
        REG_PC = GetMemWord(0xfff0);
        return;
    }
    const unsigned absDivisor = unsigned(divisor < 0 ? -divisor : divisor);
    const std::uint64_t quotientMagnitude = magnitude / absDivisor;
    divisionCycleAdjustment_ = int(dividend < 0) + int(divisor < 0);
    REG_CC &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (quotientMagnitude > 65535u) {
        divisionCycleAdjustment_ -= 21;
        REG_Q = std::uint32_t(magnitude);
        REG_CC |= CC_V;
        if (dividend < 0) REG_CC |= CC_N;
        return;
    }
    const std::int64_t quotient = dividend / divisor;
    const std::int64_t remainder = dividend % divisor;
    if (quotientMagnitude >= 32768u) {
        REG_W = WORD(quotientMagnitude);
        REG_D = WORD(remainder);
        REG_CC |= CC_V;
    } else {
        REG_W = WORD(quotient);
        REG_D = WORD(remainder);
        if (REG_W == 0) REG_CC |= CC_Z;
        if ((REG_W & 0x8000) != 0) REG_CC |= CC_N;
    }
    if ((quotientMagnitude & 1u) != 0) REG_CC |= CC_C;
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
    if ((postbyte & 0x80) == 0)
        return true;
    if (enable6309_) {
        // HD6309 indexed-postbyte map (Darren Atkinson, table p. 150).
        // Nine named W/extended-indirect forms replace reserved 6809
        // encodings. Unlisted encodings must trap instead of silently
        // executing with a fabricated effective address.
        switch (postbyte) {
        case 0x8f: case 0x90: case 0x9f: case 0xaf: case 0xb0:
        case 0xcf: case 0xd0: case 0xef: case 0xf0:
            return true;
        default: break;
        }
        switch (postbyte & 0x0f) {
        case 0x00: case 0x02:
            return (postbyte & 0x10) == 0; // indirect single-step illegal
        case 0x0f:
            return false; // only the special encodings above are legal
        default:
            return true; // includes E/F/W indexed offsets
        }
    }

    // Motorola M6809PM/AD, Appendix F, Table F-2:
    // the four register selectors use identical forms except for the
    // extended indirect [nn], which has exactly one encoding (9Fh).
    // In indirect mode, one-byte auto increment/decrement is illegal.
    const BYTE mode = postbyte & 0x0f;
    const bool indirect = (postbyte & 0x10) != 0;
    switch (mode) {
    case 0x00: case 0x02:
        return !indirect; // ,R+ / ,-R; indirect requires ++ / --
    case 0x01: case 0x03: case 0x04: case 0x05: case 0x06:
    case 0x08: case 0x09: case 0x0b: case 0x0c: case 0x0d:
        return true;
    case 0x0f:
        return postbyte == 0x9f; // extended indirect [nn]
    default:
        // 07/0A/0E: HD6309 E/F/W indexed modes; all invalid on 6809.
        return false;
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
