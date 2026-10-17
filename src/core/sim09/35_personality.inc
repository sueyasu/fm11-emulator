////////////////////////////////////////////////////////////////
// MC6809 / HD6309 execution personality

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
#define O_EXG                                                       \
{                                                                   \
    if (!IsTransferPostbyteAllowed(ACC_BL)) {                       \
        O_ERR;                                                      \
    } else {                                                        \
        BYTE *bs = (BYTE *)RegOfs(ACC_BL >> 4);                     \
        BYTE *bd = (BYTE *)RegOfs(ACC_BL & 0x0F);                   \
        WORD *ws = (WORD *)bs;                                      \
        WORD *wd = (WORD *)bd;                                      \
        switch(ACC_BL & 0x88) {                                    \
        case 0x00: ACC_AX = *ws; *ws = *wd; *wd = ACC_AX; break;   \
        case 0x08: ACC_AX = *ws; *ws = (WORD)*bd; *bd = ACC_AL; break; \
        case 0x80: ACC_AL = *bs; *bs = (BYTE)*wd; *wd = ACC_AX; break; \
        case 0x88: ACC_AL = *bs; *bs = *bd; *bd = ACC_AL; break;   \
        }                                                           \
    }                                                               \
}

#undef O_TFR
#define O_TFR                                                       \
{                                                                   \
    if (!IsTransferPostbyteAllowed(ACC_BL)) {                       \
        O_ERR;                                                      \
    } else {                                                        \
        BYTE *bs = (BYTE *)RegOfs(ACC_BL >> 4);                     \
        BYTE *bd = (BYTE *)RegOfs(ACC_BL & 0x0F);                   \
        WORD *ws = (WORD *)bs;                                      \
        WORD *wd = (WORD *)bd;                                      \
        switch(ACC_BL & 0x88) {                                    \
        case 0x00: *wd = *ws; break;                               \
        case 0x08: *wd = (WORD)*bs; break;                         \
        case 0x80: *bd = (BYTE)*ws; break;                         \
        case 0x88: *bd = *bs; break;                               \
        }                                                           \
    }                                                               \
}

// Keep the original instruction bodies intact.  The dispatch macro rejects
// HD6309-only opcodes before their implementation is reached.
#undef OPDEF
#define OPDEF(c,p) case c: if (!IsOpcodeAllowed(c)) { O_ERR; } else { p; } break;
