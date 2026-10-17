  OPDEF(0x11B0,	EXT D_BYTE S_E B_SUB E_E);		/* SUBE (ext)*/
  OPDEF(0x11B1,	EXT D_BYTE S_E B_SUB);			/* CMPE (ext)*/
  OPDEF(0x11B3,	EXT D_WORD S_U W_SUB);			/* CMPU (ext)*/
  OPDEF(0x11B6,	EXT S_BYTE B_TST E_E);			/* LDE  (ext)*/
  OPDEF(0x11B7,	EXT S_E B_TST E_BYTE);			/* STE  (ext)*/
  OPDEF(0x11BB,	EXT D_BYTE S_E B_ADD E_E);		/* ADDE (ext)*/
  OPDEF(0x11BC,	EXT D_WORD S_S W_SUB);			/* CMPS (ext)*/
  OPDEF(0x11BD,	EXT D_BYTE B_DIV);				/* DIVD (ext)*/
  OPDEF(0x11BE,	EXT D_WORD W_DIV);				/* DIVQ (ext)*/
  OPDEF(0x11BF,	EXT D_WORD W_MUL);				/* MULD (ext)*/

  OPDEF(0x11C0,	I_BYTE S_F B_SUB E_F);			/* SUBF (imm)*/
  OPDEF(0x11C1,	I_BYTE S_F B_SUB);				/* CMPF (imm)*/
  OPDEF(0x11C6,	I_BYTE S_BX B_TST E_F);			/* LDF  (imm)*/
  OPDEF(0x11CB,	I_BYTE S_F B_ADD E_F);			/* ADDF (imm)*/

  OPDEF(0x11D0,	DIR D_BYTE S_F B_SUB E_F);		/* SUBF (dir)*/
  OPDEF(0x11D1,	DIR D_BYTE S_F B_SUB);			/* CMPF (dir)*/
  OPDEF(0x11D6,	DIR S_BYTE B_TST E_F);			/* LDF  (dir)*/
  OPDEF(0x11D7,	DIR S_F B_TST E_BYTE);			/* STF  (dir)*/
  OPDEF(0x11DB,	DIR D_BYTE S_F B_ADD E_F);		/* ADDF (dir)*/

  OPDEF(0x11E0,	IND D_BYTE S_F B_SUB E_F);		/* SUBF (ind)*/
  OPDEF(0x11E1,	IND D_BYTE S_F B_SUB);			/* CMPF (ind)*/
  OPDEF(0x11E6,	IND S_BYTE B_TST E_F);			/* LDF  (ind)*/
  OPDEF(0x11E7,	IND S_F B_TST E_BYTE);			/* STF  (ind)*/
  OPDEF(0x11EB,	IND D_BYTE S_F B_ADD E_F);		/* ADDF (ind)*/

  OPDEF(0x11F0,	EXT D_BYTE S_F B_SUB E_F);		/* SUBF (ext)*/
  OPDEF(0x11F1,	EXT D_BYTE S_F B_SUB);			/* CMPF (ext)*/
  OPDEF(0x11F6,	EXT S_BYTE B_TST E_F);			/* LDF  (ext)*/
  OPDEF(0x11F7,	EXT S_F B_TST E_BYTE);			/* STF  (ext)*/
  OPDEF(0x11FB,	EXT D_BYTE S_F B_ADD E_F);		/* ADDF (ext)*/
	default: O_ERR; break;
	} break;

  OPDEF(0x12,	O_NON);							/* NOP    */
  OPDEF(0x13,	O_SYNC);						/* SYNC   */
  OPDEF(0x14,	O_SEXW);						/* SEXW   */
  OPDEF(0x15,	O_ERR);							/* no     */
  OPDEF(0x16,	I_WORD S_PC O_BRA E_PC);		/* LBRA   */
  OPDEF(0x17,	I_WORD S_PC O_BSR E_PC);		/* LBRA   */
  OPDEF(0x18,	O_ERR);							/* no     */
  OPDEF(0x19,	S_A O_DAA E_A);					/* DAA    */
  OPDEF(0x1A,	I_BYTE S_CC B_OR E_CC);			/* ORCC   */
  OPDEF(0x1B,	O_ERR);							/* no     */
  OPDEF(0x1C,	I_BYTE S_CC B_AND E_CC);		/* ANDCC  */
  OPDEF(0x1D,	O_SEX);							/* SEX    */
  OPDEF(0x1E,	I_BYTE O_EXG);					/* EXG    */
  OPDEF(0x1F,	I_BYTE O_TFR);					/* TFR    */

  OPDEF(0x20,	I_BYTE D_SX S_PC O_BRA E_PC);		/* BRA */
  OPDEF(0x21,	I_BYTE D_SX S_PC O_BRN E_PC);		/* BRN */
  OPDEF(0x22,	I_BYTE D_SX S_PC O_BHI E_PC);		/* BHI */
  OPDEF(0x23,	I_BYTE D_SX S_PC O_BLS E_PC);		/* BLS */
  OPDEF(0x24,	I_BYTE D_SX S_PC O_BHS E_PC);		/* BHS */
  OPDEF(0x25,	I_BYTE D_SX S_PC O_BLO E_PC);		/* BLO */
  OPDEF(0x26,	I_BYTE D_SX S_PC O_BNE E_PC);		/* BNE */
  OPDEF(0x27,	I_BYTE D_SX S_PC O_BEQ E_PC);		/* BEQ */
  OPDEF(0x28,	I_BYTE D_SX S_PC O_BVC E_PC);		/* BVC */
  OPDEF(0x29,	I_BYTE D_SX S_PC O_BVS E_PC);		/* BVS */
  OPDEF(0x2A,	I_BYTE D_SX S_PC O_BPL E_PC);		/* BPL */
  OPDEF(0x2B,	I_BYTE D_SX S_PC O_BMI E_PC);		/* BMI */
  OPDEF(0x2C,	I_BYTE D_SX S_PC O_BGE E_PC);		/* BGE */
  OPDEF(0x2D,	I_BYTE D_SX S_PC O_BLT E_PC);		/* BLT */
  OPDEF(0x2E,	I_BYTE D_SX S_PC O_BGT E_PC);		/* BGT */
  OPDEF(0x2F,	I_BYTE D_SX S_PC O_BLE E_PC);		/* BLE */

  OPDEF(0x30,	IND S_PT W_LEA E_X);			/* LEAX */
  OPDEF(0x31,	IND S_PT W_LEA E_Y);			/* LEAY */
  OPDEF(0x32,	IND S_PT O_NON E_S);			/* LEAS */
  OPDEF(0x33,	IND S_PT O_NON E_U);			/* LEAU */
  OPDEF(0x34,	I_BYTE O_PSHS);					/* PSHS */
  OPDEF(0x35,	I_BYTE O_PULS);					/* PULS */
  OPDEF(0x36,	I_BYTE O_PSHU);					/* PSHU */
  OPDEF(0x37,	I_BYTE O_PULU);					/* PULU */
  OPDEF(0x38,	O_ERR);							/* no   */
  OPDEF(0x39,	O_RTS);							/* RTS  */
  OPDEF(0x3A,	O_ABX);							/* ABX  */
  OPDEF(0x3B,	O_RTI);							/* RTI  */
  OPDEF(0x3C,	I_BYTE O_CWAI);					/* CWAI */
  OPDEF(0x3D,	O_MUL);							/* MUL  */
  OPDEF(0x3E,	O_ERR);							/* no   */
  OPDEF(0x3F,	O_SWI);							/* SWI  */

  OPDEF(0x40,	S_A B_NEG E_A);					/* NEGA */
  OPDEF(0x41,	O_ERR);							/* no   */
  OPDEF(0x42,	O_ERR);							/* no   */
  OPDEF(0x43,	S_A B_COM E_A);					/* COMA */
  OPDEF(0x44,	S_A B_LSR E_A);					/* LSRA */
  OPDEF(0x45,	O_ERR);							/* no   */
  OPDEF(0x46,	S_A B_ROR E_A);					/* RORA */
  OPDEF(0x47,	S_A B_ASR E_A);					/* ASRA */
  OPDEF(0x48,	S_A B_ASL E_A);					/* ASLA */
  OPDEF(0x49,	S_A B_ROL E_A);					/* ROLA */
  OPDEF(0x4A,	S_A B_DEC E_A);					/* DECA */
  OPDEF(0x4B,	O_ERR);							/* no   */
  OPDEF(0x4C,	S_A B_INC E_A);					/* INCA */
  OPDEF(0x4D,	S_A B_TST);						/* TSTA */
  OPDEF(0x4E,	O_ERR);							/* no   */
  OPDEF(0x4F,	S_A B_CLR E_A);					/* CLRA */

  OPDEF(0x50,	S_B B_NEG E_B);					/* NEGB */
  OPDEF(0x51,	O_ERR);							/* no   */
  OPDEF(0x52,	O_ERR);							/* no   */
  OPDEF(0x53,	S_B B_COM E_B);					/* COMB */
  OPDEF(0x54,	S_B B_LSR E_B);					/* LSRB */
  OPDEF(0x55,	O_ERR);							/* no   */
  OPDEF(0x56,	S_B B_ROR E_B);					/* RORB */
  OPDEF(0x57,	S_B B_ASR E_B);					/* ASRB */
  OPDEF(0x58,	S_B B_ASL E_B);					/* ASLB */
  OPDEF(0x59,	S_B B_ROL E_B);					/* ROLB */
  OPDEF(0x5A,	S_B B_DEC E_B);					/* DECB */
  OPDEF(0x5B,	O_ERR);							/* no   */
  OPDEF(0x5C,	S_B B_INC E_B);					/* INCB */
  OPDEF(0x5D,	S_B B_TST);						/* TSTB */
  OPDEF(0x5E,	O_ERR);							/* no   */
  OPDEF(0x5F,	S_B B_CLR E_B);					/* CLRB */

  OPDEF(0x60,	IND S_BYTE B_NEG E_BYTE);			/* NEG (ind) */
  OPDEF(0x61,	I_BYTE IND S_BYTE B_OR E_BYTE);		/* OIM (ind) */
  OPDEF(0x62,	I_BYTE IND S_BYTE B_AND E_BYTE);	/* AIM (ind) */
  OPDEF(0x63,	IND S_BYTE B_COM E_BYTE);			/* COM (ind) */
  OPDEF(0x64,	IND S_BYTE B_LSR E_BYTE);			/* LSR (ind) */
  OPDEF(0x65,	I_BYTE IND S_BYTE B_EOR E_BYTE);	/* EIM (ind) */
  OPDEF(0x66,	IND S_BYTE B_ROR E_BYTE);			/* ROR (ind) */
  OPDEF(0x67,	IND S_BYTE B_ASR E_BYTE);			/* ASR (ind) */
  OPDEF(0x68,	IND S_BYTE B_ASL E_BYTE);			/* ASL (ind) */
  OPDEF(0x69,	IND S_BYTE B_ROL E_BYTE);			/* ROL (ind) */
  OPDEF(0x6A,	IND S_BYTE B_DEC E_BYTE);			/* DEC (ind) */
  OPDEF(0x6B,	I_BYTE IND S_BYTE B_AND);			/* TIM (ind) */
  OPDEF(0x6C,	IND S_BYTE B_INC E_BYTE);			/* INC (ind) */
  OPDEF(0x6D,	IND S_BYTE B_TST);					/* TST (ind) */
  OPDEF(0x6E,	IND S_PT O_NON E_PC );				/* JMP (ind) */
  OPDEF(0x6F,	IND B_CLR E_BYTE);					/* CLR (ind) */

  OPDEF(0x70,	EXT S_BYTE B_NEG E_BYTE);			/* NEG (ext) */
  OPDEF(0x71,	I_BYTE EXT S_BYTE B_OR E_BYTE);		/* OIM (ext) */
  OPDEF(0x72,	I_BYTE EXT S_BYTE B_AND E_BYTE);	/* AIM (ext) */
  OPDEF(0x73,	EXT S_BYTE B_COM E_BYTE);			/* COM (ext) */
  OPDEF(0x74,	EXT S_BYTE B_LSR E_BYTE);			/* LSR (ext) */
  OPDEF(0x75,	I_BYTE EXT S_BYTE B_EOR E_BYTE);	/* EIM (ext) */
  OPDEF(0x76,	EXT S_BYTE B_ROR E_BYTE);			/* ROR (ext) */
  OPDEF(0x77,	EXT S_BYTE B_ASR E_BYTE);			/* ASR (ext) */
  OPDEF(0x78,	EXT S_BYTE B_ASL E_BYTE);			/* ASL (ext) */
  OPDEF(0x79,	EXT S_BYTE B_ROL E_BYTE);			/* ROL (ext) */
  OPDEF(0x7A,	EXT S_BYTE B_DEC E_BYTE);			/* DEC (ext) */
  OPDEF(0x7B,	I_BYTE EXT S_BYTE B_AND);			/* TIM (ext) */
  OPDEF(0x7C,	EXT S_BYTE B_INC E_BYTE);			/* INC (ext) */
  OPDEF(0x7D,	EXT S_BYTE B_TST);					/* TST (ext) */
  OPDEF(0x7E,	EXT S_PT O_NON E_PC );				/* JMP (ext) */
  OPDEF(0x7F,	EXT B_CLR E_BYTE);					/* CLR (ext) */

  OPDEF(0x80,	I_BYTE S_A B_SUB E_A);			/* SUBA (imm)*/
  OPDEF(0x81,	I_BYTE S_A B_SUB);				/* CMPA (imm)*/
  OPDEF(0x82,	I_BYTE S_A B_SBC E_A);			/* SBCA (imm)*/
  OPDEF(0x83,	I_WORD S_D W_SUB E_D);			/* SUBD (imm)*/
  OPDEF(0x84,	I_BYTE S_A B_AND E_A);			/* ANDA (imm)*/
  OPDEF(0x85,	I_BYTE S_A B_AND);				/* BITA (imm)*/
  OPDEF(0x86,	I_BYTE S_BX B_TST E_A);			/* LDA  (imm)*/
  OPDEF(0x87,	O_ERR);							/* no        */
  OPDEF(0x88,	I_BYTE S_A B_EOR E_A);			/* EORA (imm)*/
  OPDEF(0x89,	I_BYTE S_A B_ADC E_A);			/* ADCA (imm)*/
  OPDEF(0x8A,	I_BYTE S_A B_OR E_A);			/* ORA  (imm)*/
  OPDEF(0x8B,	I_BYTE S_A B_ADD E_A);			/* ADDA (imm)*/
  OPDEF(0x8C,	I_WORD S_X W_SUB);				/* CMPX (imm)*/
  OPDEF(0x8D,	I_BYTE D_SX S_PC O_BSR E_PC);	/* BSR  (imm)*/
  OPDEF(0x8E,	I_WORD S_BX W_TST E_X);			/* LDX  (imm)*/
  OPDEF(0x8F,	O_ERR);							/* no        */

  OPDEF(0x90,	DIR D_BYTE S_A B_SUB E_A);		/* SUBA (dir)*/
  OPDEF(0x91,	DIR D_BYTE S_A B_SUB);			/* CMPA (dir)*/
  OPDEF(0x92,	DIR D_BYTE S_A B_SBC E_A);		/* SBCA (dir)*/
  OPDEF(0x93,	DIR D_WORD S_D W_SUB E_D);		/* SUBD (dir)*/
  OPDEF(0x94,	DIR D_BYTE S_A B_AND E_A);		/* ANDA (dir)*/
  OPDEF(0x95,	DIR D_BYTE S_A B_AND);			/* BITA (dir)*/
  OPDEF(0x96,	DIR S_BYTE B_TST E_A);			/* LDA  (dir)*/
  OPDEF(0x97,	DIR S_A B_TST E_BYTE);			/* STA  (dir)*/
  OPDEF(0x98,	DIR D_BYTE S_A B_EOR E_A);		/* EORA (dir)*/
  OPDEF(0x99,	DIR D_BYTE S_A B_ADC E_A);		/* ADCA (dir)*/
  OPDEF(0x9A,	DIR D_BYTE S_A B_OR E_A);		/* ORA  (dir)*/
  OPDEF(0x9B,	DIR D_BYTE S_A B_ADD E_A);		/* ADDA (dir)*/
  OPDEF(0x9C,	DIR D_WORD S_X W_SUB);			/* CMPX (dir)*/
  OPDEF(0x9D,	DIR S_PT O_JSR E_PC);			/* JSR  (dir)*/
  OPDEF(0x9E,	DIR S_WORD W_TST E_X);			/* LDX  (dir)*/
  OPDEF(0x9F,	DIR S_X W_TST E_WORD);			/* STX  (dir)*/

  OPDEF(0xA0,	IND D_BYTE S_A B_SUB E_A);		/* SUBA (ind)*/
  OPDEF(0xA1,	IND D_BYTE S_A B_SUB);			/* CMPA (ind)*/
  OPDEF(0xA2,	IND D_BYTE S_A B_SBC E_A);		/* SBCA (ind)*/
  OPDEF(0xA3,	IND D_WORD S_D W_SUB E_D);		/* SUBD (ind)*/
  OPDEF(0xA4,	IND D_BYTE S_A B_AND E_A);		/* ANDA (ind)*/
  OPDEF(0xA5,	IND D_BYTE S_A B_AND);			/* BITA (ind)*/
  OPDEF(0xA6,	IND S_BYTE B_TST E_A);			/* LDA  (ind)*/
  OPDEF(0xA7,	IND S_A B_TST E_BYTE);			/* STA  (ind)*/
  OPDEF(0xA8,	IND D_BYTE S_A B_EOR E_A);		/* EORA (ind)*/
  OPDEF(0xA9,	IND D_BYTE S_A B_ADC E_A);		/* ADCA (ind)*/
  OPDEF(0xAA,	IND D_BYTE S_A B_OR E_A);		/* ORA  (ind)*/
  OPDEF(0xAB,	IND D_BYTE S_A B_ADD E_A);		/* ADDA (ind)*/
  OPDEF(0xAC,	IND D_WORD S_X W_SUB);			/* CMPX (ind)*/
  OPDEF(0xAD,	IND S_PT O_JSR E_PC);			/* JSR  (ind)*/
  OPDEF(0xAE,	IND S_WORD W_TST E_X);			/* LDX  (ind)*/
  OPDEF(0xAF,	IND S_X W_TST E_WORD);			/* STX  (ind)*/

  OPDEF(0xB0,	EXT D_BYTE S_A B_SUB E_A);		/* SUBA (ext)*/
  OPDEF(0xB1,	EXT D_BYTE S_A B_SUB);			/* CMPA (ext)*/
  OPDEF(0xB2,	EXT D_BYTE S_A B_SBC E_A);		/* SBCA (ext)*/
  OPDEF(0xB3,	EXT D_WORD S_D W_SUB E_D);		/* SUBD (ext)*/
  OPDEF(0xB4,	EXT D_BYTE S_A B_AND E_A);		/* ANDA (ext)*/
  OPDEF(0xB5,	EXT D_BYTE S_A B_AND);			/* BITA (ext)*/
  OPDEF(0xB6,	EXT S_BYTE B_TST E_A);			/* LDA  (ext)*/
  OPDEF(0xB7,	EXT S_A B_TST E_BYTE);			/* STA  (ext)*/
  OPDEF(0xB8,	EXT D_BYTE S_A B_EOR E_A);		/* EORA (ext)*/
  OPDEF(0xB9,	EXT D_BYTE S_A B_ADC E_A);		/* ADCA (ext)*/
  OPDEF(0xBA,	EXT D_BYTE S_A B_OR E_A);		/* ORA  (ext)*/
  OPDEF(0xBB,	EXT D_BYTE S_A B_ADD E_A);		/* ADDA (ext)*/
  OPDEF(0xBC,	EXT D_WORD S_X W_SUB);			/* CMPX (ext)*/
  OPDEF(0xBD,	EXT S_PT O_JSR E_PC);			/* JSR  (ext)*/
  OPDEF(0xBE,	EXT S_WORD W_TST E_X);			/* LDX  (ext)*/
  OPDEF(0xBF,	EXT S_X W_TST E_WORD);			/* STX  (ext)*/

