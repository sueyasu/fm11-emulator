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

// Keep the original instruction bodies intact.  The dispatch macro now
// rejects HD6309-only opcodes before their implementation is reached.
#undef OPDEF
#define OPDEF(c,p) case c: if (!IsOpcodeAllowed(c)) { O_ERR; } else { p; } break;
