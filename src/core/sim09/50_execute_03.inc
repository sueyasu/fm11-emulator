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
  OPDEF(0xCD,	I_LONG L_TST E_Q);							/* no        */
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
    FinishInstructionTiming(op);

}
