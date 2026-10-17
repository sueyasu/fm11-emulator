// Approximate MC6809 opcode-cycle database. Zero means unsupported or
// not yet classified, NOT zero elapsed time. Indexed operands add their
// decoded postbyte cycles; stack/branch/RTI have separate dynamic rules.
// The HD6309 native-mode deltas are applied by BeginInstructionTiming.
static constexpr unsigned char k6809BaseCycles[256] = {
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  3,  6, // 00
     0,  0,  2,  4,  2,  0,  5,  9,  0,  3,  3,  0,  3,  2,  8,  6, // 10
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3, // 20
     4,  4,  4,  4,  5,  5,  5,  5,  0,  5,  3,  6, 20, 11,  0, 19, // 30
     2,  0,  0,  2,  2,  0,  2,  2,  2,  2,  2,  0,  2,  2,  0,  2, // 40
     2,  0,  0,  2,  2,  0,  2,  2,  2,  2,  2,  0,  2,  2,  0,  2, // 50
     6,  7,  7,  6,  6,  7,  6,  6,  6,  6,  6,  7,  6,  6,  3,  6, // 60
     7,  8,  8,  7,  7,  8,  7,  7,  7,  7,  7,  8,  7,  7,  3,  7, // 70
     2,  2,  2,  4,  2,  2,  2,  0,  2,  2,  2,  2,  4,  7,  3,  0, // 80
     4,  4,  4,  6,  4,  4,  4,  4,  4,  4,  4,  4,  6,  7,  5,  5, // 90
     4,  4,  4,  6,  4,  4,  4,  4,  4,  4,  4,  4,  6,  7,  5,  5, // a0
     5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  5,  7,  8,  6,  6, // b0
     2,  2,  2,  4,  2,  2,  2,  0,  2,  2,  2,  2,  3,  0,  3,  0, // c0
     4,  4,  4,  6,  4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5, // d0
     4,  4,  4,  6,  4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5, // e0
     5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6, // f0
};

static constexpr unsigned char k6809Page10Cycles[256] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 00
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 10
     0,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5, // 20
     5,  5,  5,  5,  5,  5,  5,  5,  7,  7,  7,  7,  0,  0,  0, 20, // 30
     5,  0,  0,  5,  5,  0,  5,  5,  5,  5,  5,  0,  5,  5,  0,  5, // 40
     0,  0,  0,  5,  5,  0,  5,  0,  0,  5,  5,  0,  5,  5,  0,  5, // 50
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 60
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 70
     6,  6,  6,  6,  6,  6,  6,  0,  6,  6,  6,  6,  6,  0,  6,  0, // 80
     7,  7,  7,  7,  7,  7,  7,  6,  7,  7,  7,  7,  7,  0,  7,  6, // 90
     9,  9,  9,  9,  9,  9,  9,  8,  9,  9,  9,  9,  9,  0,  9,  8, // a0
     8,  8,  8,  8,  8,  8,  8,  7,  8,  8,  8,  8,  8,  0,  8,  7, // b0
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  0, // c0
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  6,  7,  6, // d0
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  8,  8,  9,  8, // e0
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  7,  7,  8,  7, // f0
};

static constexpr unsigned char k6309Page11Cycles[256] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 00
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 10
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 20
     6,  6,  6,  6,  6,  6,  6,  6,  5,  5,  5,  5,  5,  5,  0, 20, // 30
     0,  0,  0,  5,  0,  0,  0,  0,  0,  0,  5,  0,  5,  5,  0,  5, // 40
     0,  0,  0,  5,  0,  0,  0,  0,  0,  0,  5,  0,  5,  5,  0,  5, // 50
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 60
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 70
     5,  5,  0,  6,  0,  0,  5,  0,  0,  0,  0,  5,  6,  5,  6,  6, // 80
     6,  6,  0,  7,  0,  0,  6,  6,  0,  0,  0,  6,  7,  6,  7,  7, // 90
     8,  8,  0,  9,  0,  0,  8,  8,  0,  0,  0,  8,  9,  8,  9,  9, // a0
     7,  7,  0,  8,  0,  0,  7,  7,  0,  0,  0,  7,  8,  7,  8,  8, // b0
     5,  5,  0,  0,  0,  0,  5,  0,  0,  0,  0,  5,  0,  0,  0,  0, // c0
     6,  6,  0,  0,  0,  0,  6,  6,  0,  0,  0,  6,  0,  0,  0,  0, // d0
     8,  8,  0,  0,  0,  0,  8,  8,  0,  0,  0,  8,  0,  0,  0,  0, // e0
     7,  7,  0,  0,  0,  0,  7,  7,  0,  0,  0,  7,  0,  0,  0,  0, // f0
};

// MC6809/HD6309 timing lookup and variable-cycle correction.
void CSim09::BeginInstructionTiming(WORD op)
{
    lastInstructionCycles_ = op < 0x100 ? k6809BaseCycles[op] :
        op < 0x1100 ? k6809Page10Cycles[op & 0xff] :
        op < 0x1200 ? k6309Page11Cycles[op & 0xff] : 0;
    if (op == 0x3b) {
        const BYTE stackedCc = GetMemByte(REG_S);
        lastInstructionCycles_ = (stackedCc & CC_E) != 0 ?
            (enable6309_ && (REG_MD & MD_N) != 0 ? 17 : 15) : 6;
    }
    // The HD6309 enters emulation mode on reset (MD.N=0). Only
    // established native-mode differences are applied here. Other
    // instructions retain their existing MC6809 timing until verified.
    if (enable6309_ && (REG_MD & MD_N) != 0 &&
        lastInstructionCycles_ != 0) {
        // The documented native-mode word ALU improvements differ by
        // addressing mode; loads/stores are unchanged for immediate/indexed.
        switch (op) {
        case 0x83: case 0x8c: case 0xc3: // SUBD CMPX ADDD #imm
            --lastInstructionCycles_;
            break;
        case 0x93: case 0x9c: case 0xd3: // direct word arithmetic
        case 0xb3: case 0xbc: case 0xf3: // extended word arithmetic
            lastInstructionCycles_ -= 2;
            break;
        case 0xa3: case 0xac: case 0xe3: // indexed word ALU
            --lastInstructionCycles_;
            break;
        case 0x9e: case 0x9f: case 0xdc: case 0xdd: case 0xde: case 0xdf:
        case 0xbe: case 0xbf: case 0xfc: case 0xfd: case 0xfe: case 0xff:
            --lastInstructionCycles_; // direct/extended word transfers
            break;
        // HD6309 native flow-control timings from 6309 reference.
        // Ordinary 8-bit conditional branches remain three cycles.
        case 0x16: // LBRA: 5 -> 4
        case 0x8d: // BSR: 7 -> 6
        case 0x9d: // JSR direct: 7 -> 6
        case 0xad: // JSR indexed: 7+ -> 6+
        case 0xbd: // JSR extended: 8 -> 7
        case 0x39: // RTS: 5 -> 4
        case 0x34: case 0x35: case 0x36: case 0x37:
            --lastInstructionCycles_; // PSH/PUL: 5+ -> 4+
            break;
        case 0x17: // LBSR: 9 -> 7
            lastInstructionCycles_ -= 2;
            break;
        case 0x12: // NOP: native 1, emulation/6809 2
            --lastInstructionCycles_;
            break;
        default:
            // Native-mode byte accumulator direct/extended ALU:
            // one cycle faster. Exclude word, JSR and indexed forms.
            if ((((op & 0xf0) == 0x90 || (op & 0xf0) == 0xd0) &&
                 lastInstructionCycles_ == 4) ||
                (((op & 0xf0) == 0xb0 || (op & 0xf0) == 0xf0) &&
                 lastInstructionCycles_ == 5))
                --lastInstructionCycles_;
            break;
        }
    }
}


void CSim09::FinishInstructionTiming(WORD op)
{
    // A prefixed opcode is fetched inside the instruction dispatcher.
    // Its complete 16-bit key becomes available only after dispatch.
    if (op >= 0x1000 && op < 0x1200)
        lastInstructionCycles_ = op < 0x1100 ?
            k6809Page10Cycles[op & 0xff] : k6309Page11Cycles[op & 0xff];
    // PSHS/PULS/PSHU/PULU: five cycles plus one per 8-bit register
    // and two per 16-bit register selected in the mask.
    if (op >= 0x34 && op <= 0x37) {
        const BYTE mask = stackTimingMask_;
        unsigned bytes = 0;
        for (unsigned bit = 0; bit < 8; ++bit)
            if (mask & (1u << bit)) bytes += bit >= 4 ? 2 : 1;
        lastInstructionCycles_ += bytes;
    }
    // Long conditional branches are 5 cycles if not taken and 6 if
    // taken. Test the same CC predicates used by O_Bxx, including the
    // signed N xor V comparisons; a zero displacement is still taken.
    if (op >= 0x1021 && op <= 0x102f) {
        const BYTE cc = REG_CC;
        const bool c = (cc & CC_C) != 0;
        const bool z = (cc & CC_Z) != 0;
        const bool n = (cc & CC_N) != 0;
        const bool v = (cc & CC_V) != 0;
        bool taken = false;
        switch (op & 0xff) {
        case 0x21: taken = false; break; // LBRN
        case 0x22: taken = !c && !z; break;
        case 0x23: taken = c || z; break;
        case 0x24: taken = !c; break;
        case 0x25: taken = c; break;
        case 0x26: taken = !z; break;
        case 0x27: taken = z; break;
        case 0x28: taken = !v; break;
        case 0x29: taken = v; break;
        case 0x2a: taken = !n; break;
        case 0x2b: taken = n; break;
        case 0x2c: taken = n == v; break;
        case 0x2d: taken = n != v; break;
        case 0x2e: taken = !z && (n == v); break;
        case 0x2f: taken = z || (n != v); break;
        }
        lastInstructionCycles_ = taken ? 6 : 5;
    }
    // Postbyte timing is valid only if the indexed operand was decoded.
    const bool indexedByte = ((op >= 0xa0 && op <= 0xab) ||
                              (op >= 0xe0 && op <= 0xeb)) &&
                             (op & 0x0f) != 0x03 && (op & 0x0f) != 0x07;
    const bool indexedWord = op == 0xa3 || op == 0xac || op == 0xae ||
                             op == 0xaf || op == 0xe3 || op == 0xec ||
                             op == 0xed || op == 0xee || op == 0xef;
    const bool otherIndexed = op >= 0x1000 && op < 0x1200 &&
        ((op & 0xf0) == 0xa0 || (op & 0xf0) == 0xe0);
    if (indexedByte || indexedWord || otherIndexed || op == 0xad ||
        (op >= 0x30 && op <= 0x33)) {
        if (indexedCycleKnown_ && !illegalOperand_)
            lastInstructionCycles_ += indexedCycleExtra_;
        else
            lastInstructionCycles_ = 0;
    }
}
