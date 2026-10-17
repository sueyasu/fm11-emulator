// MC6809 instruction timing, kept separate from opcode execution.
// An unknown instruction remains unmeasured (zero cycles). HD6309
// timing differences are intentionally not assigned speculatively.
void CSim09::BeginInstructionTiming(WORD op)
{
    // Verified MC6809 subset; unmeasured opcodes remain at zero.
    switch (op) {
    case 0x12: lastInstructionCycles_ = 2; break; // NOP
    case 0x1a: case 0x1c: lastInstructionCycles_ = 3; break; // ORCC / ANDCC
    case 0x1e: lastInstructionCycles_ = 8; break; // EXG
    case 0x1f: lastInstructionCycles_ = 6; break; // TFR
    case 0x30: case 0x31: case 0x32: case 0x33:
        lastInstructionCycles_ = 4; break; // LEA indexed base
    case 0x20: lastInstructionCycles_ = 3; break; // BRA rel8
    case 0x21: case 0x22: case 0x23: case 0x24:
    case 0x25: case 0x26: case 0x27: case 0x28:
    case 0x29: case 0x2a: case 0x2b: case 0x2c:
    case 0x2d: case 0x2e: case 0x2f:
        lastInstructionCycles_ = 3; break; // conditional rel8
    case 0x16: lastInstructionCycles_ = 5; break; // LBRA rel16
    case 0x17: lastInstructionCycles_ = 9; break; // LBSR
    case 0x8d: lastInstructionCycles_ = 7; break; // BSR
    case 0x9d: lastInstructionCycles_ = 7; break; // JSR direct
    case 0xbd: lastInstructionCycles_ = 8; break; // JSR extended
    case 0x39: lastInstructionCycles_ = 5; break; // RTS
    case 0xad: lastInstructionCycles_ = 7; break; // JSR indexed base
    case 0x3b: {
        // CC.E in the saved stack frame controls full vs partial RTI.
        // The MC6809 short RTI is 6 cycles; full RTI is 15.
        const BYTE stackedCc = GetMemByte(REG_S);
        lastInstructionCycles_ = (stackedCc & 0x80) != 0 ?
            (enable6309_ && (REG_MD & MD_N) != 0 ? 17 : 15) : 6;
        break;
    }
    case 0x34: case 0x35: case 0x36: case 0x37:
        lastInstructionCycles_ = 5; break; // stack fixed overhead
    // Base 6809 8-bit immediate ALU operations: two cycles each.
    // Keep the explicit set: other opcodes in these ranges use 16-bit
    // operands or are reserved and must not inherit a guessed timing.
    case 0x80: case 0x81: case 0x82: // SUBA, CMPA, SBCA
    case 0x84: case 0x85: case 0x86: // ANDA, BITA, LDA
    case 0x88: case 0x89: case 0x8a: case 0x8b: // EORA, ADCA, ORA, ADDA
    case 0xc0: case 0xc1: case 0xc2: // SUBB, CMPB, SBCB
    case 0xc4: case 0xc5: case 0xc6: // ANDB, BITB, LDB
    case 0xc8: case 0xc9: case 0xca: case 0xcb: // EORB, ADCB, ORB, ADDB
        lastInstructionCycles_ = 2;
        break;
    // Native MC6809 direct/extended 8-bit accumulator ALU and stores.
    // Direct takes 4 cycles; extended takes 5. Word, JSR and indexed
    // operations have different timings and remain unmeasured here.
    case 0x90: case 0x91: case 0x92:
    case 0x94: case 0x95: case 0x96: case 0x97:
    case 0x98: case 0x99: case 0x9a: case 0x9b:
    case 0xd0: case 0xd1: case 0xd2:
    case 0xd4: case 0xd5: case 0xd6: case 0xd7:
    case 0xd8: case 0xd9: case 0xda: case 0xdb:
        lastInstructionCycles_ = 4;
        break;
    case 0xb0: case 0xb1: case 0xb2:
    case 0xb4: case 0xb5: case 0xb6: case 0xb7:
    case 0xb8: case 0xb9: case 0xba: case 0xbb:
    case 0xf0: case 0xf1: case 0xf2:
    case 0xf4: case 0xf5: case 0xf6: case 0xf7:
    case 0xf8: case 0xf9: case 0xfa: case 0xfb:
        lastInstructionCycles_ = 5;
        break;
    // Indexed A/B 8-bit ALU operations have a four-cycle base.
    case 0xa0: case 0xa1: case 0xa2:
    case 0xa4: case 0xa5: case 0xa6: case 0xa7:
    case 0xa8: case 0xa9: case 0xaa: case 0xab:
    case 0xe0: case 0xe1: case 0xe2:
    case 0xe4: case 0xe5: case 0xe6: case 0xe7:
    case 0xe8: case 0xe9: case 0xea: case 0xeb:
        lastInstructionCycles_ = 4;
        break;
    // MC6809 16-bit ALU/load/store, excluding indexed addressing.
    // 16-bit arithmetic and CMPX: immediate 4, direct 6, extended 7.
    case 0x83: case 0x8c: case 0xc3:
        lastInstructionCycles_ = 4; break;
    case 0x93: case 0x9c: case 0xd3:
        lastInstructionCycles_ = 6; break;
    case 0xb3: case 0xbc: case 0xf3:
        lastInstructionCycles_ = 7; break;
    // 16-bit loads/stores: immediate 3, direct 5, extended 6.
    case 0x8e: case 0xcc: case 0xce:
        lastInstructionCycles_ = 3; break;
    case 0x9e: case 0x9f: case 0xdc: case 0xdd: case 0xde: case 0xdf:
        lastInstructionCycles_ = 5; break;
    case 0xbe: case 0xbf: case 0xfc: case 0xfd: case 0xfe: case 0xff:
        lastInstructionCycles_ = 6; break;
    // Native 6809 indexed word operations. Effective-address postbyte
    // cycles are added only after GetAddrInd successfully decodes it.
    case 0xa3: case 0xac: case 0xe3: // SUBD, CMPX, ADDD
        lastInstructionCycles_ = 6; break;
    case 0xae: case 0xaf: case 0xec: case 0xed: case 0xee: case 0xef:
        lastInstructionCycles_ = 5; break; // LDX/STX/LDD/STD/LDU/STU
    default: break;
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
    if (indexedByte || indexedWord || op == 0xad ||
        (op >= 0x30 && op <= 0x33)) {
        if (indexedCycleKnown_ && !illegalOperand_)
            lastInstructionCycles_ += indexedCycleExtra_;
        else
            lastInstructionCycles_ = 0;
    }
}
