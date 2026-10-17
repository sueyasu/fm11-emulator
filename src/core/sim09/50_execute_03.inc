  OPDEF(0xC0,	I_BYTE S_B B_SUB E_B);			/* SUBB (imm)*/
  OPDEF(0xC1,	I_BYTE S_B B_SUB);				/* CMPB (imm)*/
  OPDEF(0xC2,	I_BYTE S_B B_SBC E_B);			/* SBCB (imm)*/
  OPDEF(0xC3,	I_WORD S_D W_ADD E_D);			/* ADDD (imm)*/
  OPDEF(0xC4,	I_BYTE S_B B_AND E_B);			/* ANDB (imm)*/
  OPDEF(0xC5,	I_BYTE S_B B_AND);				/* BITB (imm)*/
  OPDEF(0xC6,	I_BYTE S_BX B_TST E_B);			/* LDB  (imm)*/
  OPDEF(0xC7,	O_ERR);							/* no        */
  OPDEF(0xC8,	I_BYTE S_B B_EOR E_B);			/* EORB (imm)*/
  OPDEF(0xC9,	I_BYTE S_B B_ADC E_B);			/* ADCB (imm)*/
  OPDEF(0xCA,	I_BYTE S_B B_OR E_B);			/* ORB  (imm)*/
  OPDEF(0xCB,	I_BYTE S_B B_ADD E_B);			/* ADDB (imm)*/
  OPDEF(0xCC,	I_WORD S_BX W_TST E_D);			/* LDD  (imm)*/
  OPDEF(0xCD,	O_ERR);							/* no        */
  OPDEF(0xCE,	I_WORD S_BX W_TST E_U);			/* LDU  (imm)*/
  OPDEF(0xCF,	O_ERR);							/* no        */

  OPDEF(0xD0,	DIR D_BYTE S_B B_SUB E_B);		/* SUBB (dir)*/
  OPDEF(0xD1,	DIR D_BYTE S_B B_SUB);			/* CMPB (dir)*/
  OPDEF(0xD2,	DIR D_BYTE S_B B_SBC E_B);		/* SBCB (dir)*/
  OPDEF(0xD3,	DIR D_WORD S_D W_ADD E_D);		/* ADDD (dir)*/
  OPDEF(0xD4,	DIR D_BYTE S_B B_AND E_B);		/* ANDB (dir)*/
  OPDEF(0xD5,	DIR D_BYTE S_B B_AND);			/* BITB (dir)*/
  OPDEF(0xD6,	DIR S_BYTE B_TST E_B);			/* LDB  (dir)*/
  OPDEF(0xD7,	DIR S_B B_TST E_BYTE);			/* STB  (dir)*/
  OPDEF(0xD8,	DIR D_BYTE S_B B_EOR E_B);		/* EORB (dir)*/
  OPDEF(0xD9,	DIR D_BYTE S_B B_ADC E_B);		/* ADCB (dir)*/
  OPDEF(0xDA,	DIR D_BYTE S_B B_OR E_B);		/* ORB  (dir)*/
  OPDEF(0xDB,	DIR D_BYTE S_B B_ADD E_B);		/* ADDB (dir)*/
  OPDEF(0xDC,	DIR S_WORD W_TST E_D);			/* LDD  (dir)*/
  OPDEF(0xDD,	DIR S_D W_TST E_WORD);			/* STD  (dir)*/
  OPDEF(0xDE,	DIR S_WORD W_TST E_U);			/* LDU  (dir)*/
  OPDEF(0xDF,	DIR S_U W_TST E_WORD);			/* STU  (dir)*/

  OPDEF(0xE0,	IND D_BYTE S_B B_SUB E_B);		/* SUBB (ind)*/
  OPDEF(0xE1,	IND D_BYTE S_B B_SUB);			/* CMPB (ind)*/
  OPDEF(0xE2,	IND D_BYTE S_B B_SBC E_B);		/* SBCB (ind)*/
  OPDEF(0xE3,	IND D_WORD S_D W_ADD E_D);		/* ADDD (ind)*/
  OPDEF(0xE4,	IND D_BYTE S_B B_AND E_B);		/* ANDB (ind)*/
  OPDEF(0xE5,	IND D_BYTE S_B B_AND);			/* BITB (ind)*/
  OPDEF(0xE6,	IND S_BYTE B_TST E_B);			/* LDB  (ind)*/
  OPDEF(0xE7,	IND S_B B_TST E_BYTE);			/* STB  (ind)*/
  OPDEF(0xE8,	IND D_BYTE S_B B_EOR E_B);		/* EORB (ind)*/
  OPDEF(0xE9,	IND D_BYTE S_B B_ADC E_B);		/* ADCB (ind)*/
  OPDEF(0xEA,	IND D_BYTE S_B B_OR E_B);		/* ORB  (ind)*/
  OPDEF(0xEB,	IND D_BYTE S_B B_ADD E_B);		/* ADDB (ind)*/
  OPDEF(0xEC,	IND S_WORD W_TST E_D);			/* LDD  (ind)*/
  OPDEF(0xED,	IND S_D W_TST E_WORD);			/* STD  (ind)*/
  OPDEF(0xEE,	IND S_WORD W_TST E_U);			/* LDU  (ind)*/
  OPDEF(0xEF,	IND S_U W_TST E_WORD);			/* STU  (ind)*/

  OPDEF(0xF0,	EXT D_BYTE S_B B_SUB E_B);		/* SUBB (ext)*/
  OPDEF(0xF1,	EXT D_BYTE S_B B_SUB);			/* CMPB (ext)*/
  OPDEF(0xF2,	EXT D_BYTE S_B B_SBC E_B);		/* SBCB (ext)*/
  OPDEF(0xF3,	EXT D_WORD S_D W_ADD E_D);		/* ADDD (ext)*/
  OPDEF(0xF4,	EXT D_BYTE S_B B_AND E_B);		/* ANDB (ext)*/
  OPDEF(0xF5,	EXT D_BYTE S_B B_AND);			/* BITB (ext)*/
  OPDEF(0xF6,	EXT S_BYTE B_TST E_B);			/* LDB  (ext)*/
  OPDEF(0xF7,	EXT S_B B_TST E_BYTE);			/* STB  (ext)*/
  OPDEF(0xF8,	EXT D_BYTE S_B B_EOR E_B);		/* EORB (ext)*/
  OPDEF(0xF9,	EXT D_BYTE S_B B_ADC E_B);		/* ADCB (ext)*/
  OPDEF(0xFA,	EXT D_BYTE S_B B_OR E_B);		/* ORB  (ext)*/
  OPDEF(0xFB,	EXT D_BYTE S_B B_ADD E_B);		/* ADDB (ext)*/
  OPDEF(0xFC,	EXT S_WORD W_TST E_D);			/* LDD  (ext)*/
  OPDEF(0xFD,	EXT S_D W_TST E_WORD);			/* STD  (ext)*/
  OPDEF(0xFE,	EXT S_WORD W_TST E_U);			/* LDU  (ext)*/
  OPDEF(0xFF,	EXT S_U W_TST E_WORD);			/* STU  (ext)*/
    }
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
    if (indexedByte || indexedWord || op == 0xad) {
        if (indexedCycleKnown_ && !illegalOperand_)
            lastInstructionCycles_ += indexedCycleExtra_;
        else
            lastInstructionCycles_ = 0;
    }
}
