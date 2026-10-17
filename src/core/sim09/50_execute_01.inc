void	CSim09::SimONE()
{
    lastInstructionCycles_ = 0;
    indexedCycleKnown_ = false;
    indexedCycleExtra_ = 0;
    stackTimingMask_ = 0;
    WORD op;
    register ACC accA;
    register ACC accB;
    WORD accP;
    LONG accL;

    if ( (REG_ST & ST_W) != 0 ) {
		regs.HD |= 010;
		GetMemByte(REG_PC);	/* Dummy */
		return;
	}

    op = GetOpByte();
    BeginInstructionTiming(op);
    if (op >= 0x34 && op <= 0x37)
        stackTimingMask_ = GetMemByte(REG_PC);
    switch(op) {
  OPDEF(0x00,	DIR S_BYTE B_NEG E_BYTE);			/* NEG (dir) */
  OPDEF(0x01,	I_BYTE DIR S_BYTE B_OR E_BYTE);		/* OIM (dir) */
  OPDEF(0x02,	I_BYTE DIR S_BYTE B_AND E_BYTE);	/* AIM (dir) */
  OPDEF(0x03,	DIR S_BYTE B_COM E_BYTE);			/* COM (dir) */
  OPDEF(0x04,	DIR S_BYTE B_LSR E_BYTE);			/* LSR (dir) */
  OPDEF(0x05,	I_BYTE DIR S_BYTE B_EOR E_BYTE);	/* EIM (dir) */
  OPDEF(0x06,	DIR S_BYTE B_ROR E_BYTE);			/* ROR (dir) */
  OPDEF(0x07,	DIR S_BYTE B_ASR E_BYTE);			/* ASR (dir) */
  OPDEF(0x08,	DIR S_BYTE B_ASL E_BYTE);			/* ASL (dir) */
  OPDEF(0x09,	DIR S_BYTE B_ROL E_BYTE);			/* ROL (dir) */
  OPDEF(0x0A,	DIR S_BYTE B_DEC E_BYTE);			/* DEC (dir) */
  OPDEF(0x0B,	I_BYTE DIR S_BYTE B_AND);			/* TIM (dir) */
  OPDEF(0x0C,	DIR S_BYTE B_INC E_BYTE);			/* INC (dir) */
  OPDEF(0x0D,	DIR S_BYTE B_TST);					/* TST (dir) */
  OPDEF(0x0E,	DIR S_PT O_NON E_PC );				/* JMP (dir) */
  OPDEF(0x0F,	DIR B_CLR E_BYTE);					/* CLR (dir) */

    case 0x10:
	op = 0x1000 | GetOpByte();
	switch(op) {
  OPDEF(0x1021,	I_WORD S_PC O_BRN E_PC);		/* LBRN */
  OPDEF(0x1022,	I_WORD S_PC O_BHI E_PC);		/* LBHI */
  OPDEF(0x1023,	I_WORD S_PC O_BLS E_PC);		/* LBLS */
  OPDEF(0x1024,	I_WORD S_PC O_BHS E_PC);		/* LBHS */
  OPDEF(0x1025,	I_WORD S_PC O_BLO E_PC);		/* LBLO */
  OPDEF(0x1026,	I_WORD S_PC O_BNE E_PC);		/* LBNE */
  OPDEF(0x1027,	I_WORD S_PC O_BEQ E_PC);		/* LBEQ */
  OPDEF(0x1028,	I_WORD S_PC O_BVC E_PC);		/* LBVC */
  OPDEF(0x1029,	I_WORD S_PC O_BVS E_PC);		/* LBVS */
  OPDEF(0x102A,	I_WORD S_PC O_BPL E_PC);		/* LBPL */
  OPDEF(0x102B,	I_WORD S_PC O_BMI E_PC);		/* LBMI */
  OPDEF(0x102C,	I_WORD S_PC O_BGE E_PC);		/* LBGE */
  OPDEF(0x102D,	I_WORD S_PC O_BLT E_PC);		/* LBLT */
  OPDEF(0x102E,	I_WORD S_PC O_BGT E_PC);		/* LBGT */
  OPDEF(0x102F,	I_WORD S_PC O_BLE E_PC);		/* LBLE */

  OPDEF(0x1030,	I_BYTE O_ADDR);				/* ADDR */
  OPDEF(0x1031,	I_BYTE O_ADCR);				/* ADCR */
  OPDEF(0x1032,	I_BYTE O_SUBR);				/* SUBR */
  OPDEF(0x1033,	I_BYTE O_SBCR);				/* SBCR */
  OPDEF(0x1034,	I_BYTE O_ANDR);				/* ANDR */
  OPDEF(0x1035,	I_BYTE O_ORR);				/* ORR */
  OPDEF(0x1036,	I_BYTE O_EORR);				/* EORR */
  OPDEF(0x1037,	I_BYTE O_CMPR);				/* CMPR */
  OPDEF(0x1038,	O_PSHSW);					/* PSHSW */
  OPDEF(0x1039,	O_PULSW);					/* PULSW */
  OPDEF(0x103A,	O_PSHUW);					/* PSHUW */
  OPDEF(0x103B,	O_PULUW);					/* PULUW */
  OPDEF(0x103F,	O_SWI2);					/* SWI2 */

  OPDEF(0x1040,	S_D W_NEG E_D);				/* NEGD */
  OPDEF(0x1043,	S_D W_COM E_D);				/* COMD */
  OPDEF(0x1044,	S_D W_LSR E_D);				/* LSRD */
  OPDEF(0x1046,	S_D W_ROR E_D);				/* RORD */
  OPDEF(0x1047,	S_D W_ASR E_D);				/* ASRD */
  OPDEF(0x1048,	S_D W_ASL E_D);				/* ASLD */
  OPDEF(0x1049,	S_D W_ROL E_D);				/* ROLD */
  OPDEF(0x104A,	S_D W_DEC E_D);				/* DECD */
  OPDEF(0x104C,	S_D W_INC E_D);				/* INCD */
  OPDEF(0x104D,	S_D W_TST);					/* TSTD */
  OPDEF(0x104F,	S_D W_CLR E_D);				/* CLRD */

  OPDEF(0x1053,	S_W W_COM E_W);				/* COMW */
  OPDEF(0x1054,	S_W W_LSR E_W);				/* LSRW */
  OPDEF(0x1056,	S_W W_ROR E_W);				/* RORW */
  OPDEF(0x1059,	S_W W_ROL E_W);				/* ROLW */
  OPDEF(0x105A,	S_W W_DEC E_W);				/* DECW */
  OPDEF(0x105C,	S_W W_INC E_W);				/* INCW */
  OPDEF(0x105D,	S_W W_TST);					/* TSTW */
  OPDEF(0x105F,	S_W W_CLR E_W);				/* CLRW */

  OPDEF(0x1080,	I_WORD S_W W_SUB E_W);			/* SUBW (imm)*/
  OPDEF(0x1081,	I_WORD S_W W_SUB);				/* CMPW (imm)*/
  OPDEF(0x1082,	I_WORD S_D W_SBC E_D);			/* SBCD (imm)*/
  OPDEF(0x1083,	I_WORD S_D W_SUB);				/* CMPD (imm)*/
  OPDEF(0x1084,	I_WORD S_D W_AND E_D);			/* ANDD (imm)*/
  OPDEF(0x1085,	I_WORD S_D W_AND);				/* BITD (imm)*/
  OPDEF(0x1086,	I_WORD S_BX W_TST E_W);			/* LDW  (imm)*/
  OPDEF(0x1088,	I_WORD S_D W_EOR E_D);			/* EORD (imm)*/
  OPDEF(0x1089,	I_WORD S_D W_ADC E_D);			/* ADCD (imm)*/
  OPDEF(0x108A,	I_WORD S_D W_OR E_D);			/* ORD  (imm)*/
  OPDEF(0x108B,	I_WORD S_W W_ADD E_W);			/* ADDW (imm)*/
  OPDEF(0x108C,	I_WORD S_Y W_SUB);				/* CMPY (imm)*/
  OPDEF(0x108E,	I_WORD S_BX W_TST E_Y);			/* LDY  (imm)*/

  OPDEF(0x1090,	DIR D_WORD S_W W_SUB E_W);		/* SUBW (dir)*/
  OPDEF(0x1091,	DIR D_WORD S_W W_SUB);			/* CMPW (dir)*/
  OPDEF(0x1092,	DIR D_WORD S_D W_SBC E_D);		/* SBCD (dir)*/
  OPDEF(0x1093,	DIR D_WORD S_D W_SUB);			/* CMPD (dir)*/
  OPDEF(0x1094,	DIR D_WORD S_D W_AND E_D);		/* ANDD (dir)*/
  OPDEF(0x1095,	DIR D_WORD S_D W_AND);			/* BITD (dir)*/
  OPDEF(0x1096,	DIR S_WORD W_TST E_W);			/* LDW  (dir)*/
  OPDEF(0x1097,	DIR S_W W_TST E_WORD);			/* STW  (dir)*/
  OPDEF(0x1098,	DIR D_WORD S_D W_EOR E_D);		/* EORD (dir)*/
  OPDEF(0x1099,	DIR D_WORD S_D W_ADC E_D);		/* ADCD (dir)*/
  OPDEF(0x109A,	DIR D_WORD S_D W_OR E_D);		/* ORD  (dir)*/
  OPDEF(0x109B,	DIR D_WORD S_W W_ADD E_W);		/* ADDW (dir)*/
  OPDEF(0x109C,	DIR D_WORD S_Y W_SUB);			/* CMPY (dir)*/
  OPDEF(0x109E,	DIR S_WORD W_TST E_Y);			/* LDY  (dir)*/
  OPDEF(0x109F,	DIR S_Y W_TST E_WORD);			/* STY  (dir)*/

  OPDEF(0x10A0,	IND D_WORD S_W W_SUB E_W);		/* SUBW (ind)*/
  OPDEF(0x10A1,	IND D_WORD S_W W_SUB);			/* CMPW (ind)*/
  OPDEF(0x10A2,	IND D_WORD S_D W_SBC E_D);		/* SBCD (ind)*/
  OPDEF(0x10A3,	IND D_WORD S_D W_SUB);			/* CMPD (ind)*/
  OPDEF(0x10A4,	IND D_WORD S_D W_AND E_D);		/* ANDD (ind)*/
  OPDEF(0x10A5,	IND D_WORD S_D W_AND);			/* BITD (ind)*/
  OPDEF(0x10A6,	IND S_WORD W_TST E_W);			/* LDW  (ind)*/
  OPDEF(0x10A7,	IND S_W W_TST E_WORD);			/* STW  (ind)*/
  OPDEF(0x10A8,	IND D_WORD S_D W_EOR E_D);		/* EORD (ind)*/
  OPDEF(0x10A9,	IND D_WORD S_D W_ADC E_D);		/* ADCD (ind)*/
  OPDEF(0x10AA,	IND D_WORD S_D W_OR E_D);		/* ORD  (ind)*/
  OPDEF(0x10AB,	IND D_WORD S_W W_ADD E_W);		/* ADDW (ind)*/
  OPDEF(0x10AC,	IND D_WORD S_Y W_SUB);			/* CMPY (ind)*/
  OPDEF(0x10AE,	IND S_WORD W_TST E_Y);			/* LDY  (ind)*/
  OPDEF(0x10AF,	IND S_Y W_TST E_WORD);			/* STY  (ind)*/

  OPDEF(0x10B0,	EXT D_WORD S_W W_SUB E_W);		/* SUBW (ext)*/
  OPDEF(0x10B1,	EXT D_WORD S_W W_SUB);			/* CMPW (ext)*/
  OPDEF(0x10B2,	EXT D_WORD S_D W_SBC E_D);		/* SBCD (ext)*/
  OPDEF(0x10B3,	EXT D_WORD S_D W_SUB);			/* CMPD (ext)*/
  OPDEF(0x10B4,	EXT D_WORD S_D W_AND E_D);		/* ANDD (ext)*/
  OPDEF(0x10B5,	EXT D_WORD S_D W_AND);			/* BITD (ext)*/
  OPDEF(0x10B6,	EXT S_WORD W_TST E_W);			/* LDW  (ext)*/
  OPDEF(0x10B7,	EXT S_W W_TST E_WORD);			/* STW  (ext)*/
  OPDEF(0x10B8,	EXT D_WORD S_D W_EOR E_D);		/* EORD (ext)*/
  OPDEF(0x10B9,	EXT D_WORD S_D W_ADC E_D);		/* ADCD (ext)*/
  OPDEF(0x10BA,	EXT D_WORD S_D W_OR E_D);		/* ORD  (ext)*/
  OPDEF(0x10BB,	EXT D_WORD S_W W_ADD E_W);		/* ADDW (ext)*/
  OPDEF(0x10BC,	EXT D_WORD S_Y W_SUB);			/* CMPY (ext)*/
  OPDEF(0x10BE,	EXT S_WORD W_TST E_Y);			/* LDY  (ext)*/
  OPDEF(0x10BF,	EXT S_Y W_TST E_WORD);			/* STY  (ext)*/

  OPDEF(0x10CE,	I_WORD S_BX W_TST E_S);			/* LDS  (imm)*/

  OPDEF(0x10DC,	DIR S_LONG L_TST E_Q);			/* LDQ  (dir)*/
  OPDEF(0x10DD,	DIR S_Q L_TST E_LONG);			/* STQ  (dir)*/
  OPDEF(0x10DE,	DIR S_WORD W_TST E_S);			/* LDS  (dir)*/
  OPDEF(0x10DF,	DIR S_S W_TST E_WORD);			/* STS  (dir)*/

  OPDEF(0x10EC,	IND S_LONG L_TST E_Q);			/* LDQ  (ind)*/
  OPDEF(0x10ED,	IND S_Q L_TST E_LONG);			/* STQ  (ind)*/
  OPDEF(0x10EE,	IND S_WORD W_TST E_S);			/* LDS  (ind)*/
  OPDEF(0x10EF,	IND S_S W_TST E_WORD);			/* STS  (ind)*/

  OPDEF(0x10FC,	EXT S_LONG L_TST E_Q);			/* LDQ  (ext)*/
  OPDEF(0x10FD,	EXT S_Q L_TST E_LONG);			/* STQ  (ext)*/
  OPDEF(0x10FE,	EXT S_WORD W_TST E_S);			/* LDS  (ext)*/
  OPDEF(0x10FF,	EXT S_S W_TST E_WORD);			/* STS  (ext)*/
	default: O_ERR; break;
	} break;

    case 0x11:
	op = 0x1100 | GetOpByte();
	switch(op) {
  OPDEF(0x1130,	I_BYTE DIR S_BYTE O_BAND);		/* BAND    */
  OPDEF(0x1131,	I_BYTE DIR S_BYTE O_IAND);		/* BIAND   */
  OPDEF(0x1132,	I_BYTE DIR S_BYTE O_BOR);		/* BOR     */
  OPDEF(0x1133,	I_BYTE DIR S_BYTE O_IOR);		/* BIOR    */
  OPDEF(0x1134,	I_BYTE DIR S_BYTE O_BEOR);		/* BEOR    */
  OPDEF(0x1135,	I_BYTE DIR S_BYTE O_IEOR);		/* BIEOR   */
  OPDEF(0x1136,	I_BYTE DIR S_BYTE O_LDBT);		/* LDBT    */
  OPDEF(0x1137,	I_BYTE DIR S_BYTE O_STBT E_BYTE);	/* STBT    */
  OPDEF(0x1138,	I_BYTE O_TFMI);					/* TFM +.+ */
  OPDEF(0x1139,	I_BYTE O_TFMD);					/* TFM -.- */
  OPDEF(0x113A,	I_BYTE O_TFMS);					/* TFM +.  */
  OPDEF(0x113B,	I_BYTE O_TFML);					/* TFM  .+ */
  OPDEF(0x113C,	I_BYTE O_BITMD);				/* bitmd   */
  OPDEF(0x113D,	I_BYTE S_BX O_NON  E_MD);		/* ldmd    */
  OPDEF(0x113F,	O_SWI3);						/* SWI3    */

  OPDEF(0x1143,	S_E B_COM E_E);				/* COME */
  OPDEF(0x114A,	S_E B_DEC E_E);				/* DECE */
  OPDEF(0x114C,	S_E B_INC E_E);				/* INCE */
  OPDEF(0x114D,	S_E B_TST);					/* TSTE */
  OPDEF(0x114F,	S_E B_CLR E_E);				/* CLRE */

  OPDEF(0x1153,	S_F B_COM E_F);				/* COMF */
  OPDEF(0x115A,	S_F B_DEC E_F);				/* DECF */
  OPDEF(0x115C,	S_F B_INC E_F);				/* INCF */
  OPDEF(0x115D,	S_F B_TST);					/* TSTF */
  OPDEF(0x115F,	S_F B_CLR E_F);				/* CLRF */

  OPDEF(0x1180,	I_BYTE S_E B_SUB E_E);			/* SUBE (imm)*/
  OPDEF(0x1181,	I_BYTE S_E B_SUB);				/* CMPE (imm)*/
  OPDEF(0x1183,	I_WORD S_U W_SUB);				/* CMPU (imm)*/
  OPDEF(0x1186,	I_BYTE S_BX B_TST E_E);			/* LDE  (imm)*/
  OPDEF(0x118B,	I_BYTE S_E B_ADD E_E);			/* ADDE (imm)*/
  OPDEF(0x118C,	I_WORD S_S W_SUB);				/* CMPS (imm)*/
  OPDEF(0x118D,	I_BYTE B_DIV);					/* DIVD (imm)*/
  OPDEF(0x118E,	I_WORD W_DIV);					/* DIVQ (imm)*/
  OPDEF(0x118F,	I_WORD W_MUL);					/* MULD (imm)*/

  OPDEF(0x1190,	DIR D_BYTE S_E B_SUB E_E);		/* SUBE (dir)*/
  OPDEF(0x1191,	DIR D_BYTE S_E B_SUB);			/* CMPE (dir)*/
  OPDEF(0x1193,	DIR D_WORD S_U W_SUB);			/* CMPU (dir)*/
  OPDEF(0x1196,	DIR S_BYTE B_TST E_E);			/* LDE  (dir)*/
  OPDEF(0x1197,	DIR S_E B_TST E_BYTE);			/* STE  (dir)*/
  OPDEF(0x119B,	DIR D_BYTE S_E B_ADD E_E);		/* ADDE (dir)*/
  OPDEF(0x119C,	DIR D_WORD S_S W_SUB);			/* CMPS (dir)*/
  OPDEF(0x119D,	DIR D_BYTE B_DIV);				/* DIVD (dir)*/
  OPDEF(0x119E,	DIR D_WORD W_DIV);				/* DIVQ (dir)*/
  OPDEF(0x119F,	DIR D_WORD W_MUL);				/* MULD (dir)*/

  OPDEF(0x11A0,	IND D_BYTE S_E B_SUB E_E);		/* SUBE (ind)*/
  OPDEF(0x11A1,	IND D_BYTE S_E B_SUB);			/* CMPE (ind)*/
  OPDEF(0x11A3,	IND D_WORD S_U W_SUB);			/* CMPU (ind)*/
  OPDEF(0x11A6,	IND S_BYTE B_TST E_E);			/* LDE  (ind)*/
  OPDEF(0x11A7,	IND S_E B_TST E_BYTE);			/* STE  (ind)*/
  OPDEF(0x11AB,	IND D_BYTE S_E B_ADD E_E);		/* ADDE (ind)*/
  OPDEF(0x11AC,	IND D_WORD S_S W_SUB);			/* CMPS (ind)*/
  OPDEF(0x11AD,	IND D_BYTE B_DIV);				/* DIVD (ind)*/
  OPDEF(0x11AE,	IND D_WORD W_DIV);				/* DIVQ (ind)*/
  OPDEF(0x11AF,	IND D_WORD W_MUL);				/* MULD (ind)*/

