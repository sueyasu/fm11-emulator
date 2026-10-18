////////////////////////////////////////////////////////////////
//	Inh OpCode

#define	L_TST								\
    REG_CC &= ~(CC_V | CC_Z | CC_N);		\
    if ( ACC_AQ == 0L ) REG_CC |= CC_Z;		\
    if ( ACC_AQ & 0x80000000L ) REG_CC |= CC_N;

#define	O_SYNC REG_ST |= 0004;

#define	O_SEXW										\
	REG_D = ((REG_W & 0x8000) != 0 ? 0xFFFF : 0x0000);	\
	if ( REG_W == 0 ) REG_CC |= CC_Z;					\
	if ( REG_W & 0x8000 ) REG_CC |= CC_N;
#define	O_SEX									\
	REG_A = ((REG_B & 0x80) != 0 ? 0xFF : 0x00);	\
	if ( REG_B == 0 ) REG_CC |= CC_Z;				\
	if ( REG_B & 0x80 ) REG_CC |= CC_N;

#define	O_DAA					\
{   int n = ((REG_CC & CC_H) != 0 || (REG_A & 0x0F) > 0x09 ? 0x06 : 0x00);\
    n |= ((REG_CC & CC_C) != 0 || (REG_A & 0xF0) > 0x90 ||\
	  REG_A > 0x89 ? 0x60 : 0x00);		\
    n += REG_A;					\
    REG_A = (BYTE)n;				\
    REG_CC &= ~(CC_Z | CC_N);			\
    if ( n > 255 ) REG_CC |= CC_C;		\
    if ( REG_A == 0 ) REG_CC |= CC_Z;		\
    if ( REG_A & 0x80 ) REG_CC |= CC_N; }

#define	O_EXG					\
{   BYTE *bs = (BYTE *)RegOfs(ACC_BL >> 4);	\
    BYTE *bd = (BYTE *)RegOfs(ACC_BL & 0x0F);	\
    WORD *ws = (WORD *)bs;			\
    WORD *wd = (WORD *)bd;			\
    switch(ACC_BL & 0x88) {			\
    case 0x00: ACC_AX = *ws; *ws = *wd; *wd = ACC_AX; break;		\
    case 0x08: ACC_AX = *ws; *ws = (WORD)*bd; *bd = ACC_AL; break;	\
    case 0x80: ACC_AL = *bs; *bs = (BYTE)*wd; *wd = ACC_AX; break;	\
    case 0x88: ACC_AL = *bs; *bs = *bd; *bd = ACC_AL; break;		\
    } }

#define	O_TFR					\
{   BYTE *bs = (BYTE *)RegOfs(ACC_BL >> 4);	\
    BYTE *bd = (BYTE *)RegOfs(ACC_BL & 0x0F);	\
    WORD *ws = (WORD *)bs;			\
    WORD *wd = (WORD *)bd;			\
    switch(ACC_BL & 0x88) {			\
    case 0x00: *wd = *ws; break;		\
    case 0x08: *wd = (WORD)*bs; break;		\
    case 0x80: *bd = (BYTE)*ws; break;		\
    case 0x88: *bd = *bs; break;		\
    } }

#define	O_JSR					\
    SetMemByte(--REG_S, REG_PCl);		\
    SetMemByte(--REG_S, REG_PCh);

#define	O_BSR					\
    SetMemByte(--REG_S, REG_PCl);		\
    SetMemByte(--REG_S, REG_PCh);		\
    ACC_AX += ACC_BX;

#define	O_BRA	ACC_AX += ACC_BX;
#define	O_BRN

#define	O_BHI					\
    if ( (REG_CC & (CC_C | CC_Z)) == 0 )	\
	ACC_AX += ACC_BX;

#define	O_BLS					\
    if ( (REG_CC & (CC_C | CC_Z)) != 0 )	\
	ACC_AX += ACC_BX;

#define	O_BHS					\
    if ( (REG_CC & CC_C) == 0 )			\
	ACC_AX += ACC_BX;

#define	O_BLO					\
    if ( (REG_CC & CC_C) != 0 )			\
	ACC_AX += ACC_BX;

#define	O_BNE					\
    if ( (REG_CC & CC_Z) == 0 )			\
	ACC_AX += ACC_BX;

#define	O_BEQ					\
    if ( (REG_CC & CC_Z) != 0 )			\
	ACC_AX += ACC_BX;

#define	O_BVC					\
    if ( (REG_CC & CC_V) == 0 )			\
	ACC_AX += ACC_BX;

#define	O_BVS					\
    if ( (REG_CC & CC_V) != 0 )			\
	ACC_AX += ACC_BX;

#define	O_BPL					\
    if ( (REG_CC & CC_N) == 0 )			\
	ACC_AX += ACC_BX;

#define	O_BMI					\
    if ( (REG_CC & CC_N) != 0 )			\
	ACC_AX += ACC_BX;

#define	O_BGE					\
    if ( (REG_CC & CC_N ? 1 : 0) == (REG_CC & CC_V ? 1 : 0) ) \
	ACC_AX += ACC_BX;

#define	O_BLT					\
    if ( (REG_CC & CC_N ? 1 : 0) != (REG_CC & CC_V ? 1 : 0) ) \
	ACC_AX += ACC_BX;

#define	O_BGT					\
    if ( (REG_CC & CC_Z) == 0 &&		\
	 (REG_CC & CC_N ? 1 : 0) == (REG_CC & CC_V ? 1 : 0) ) \
	ACC_AX += ACC_BX;

#define	O_BLE					\
    if ( (REG_CC & CC_Z) != 0 ||		\
	 (REG_CC & CC_N ? 1 : 0) != (REG_CC & CC_V ? 1 : 0) ) \
	ACC_AX += ACC_BX;

#define	O_PSHS					\
    if ( (ACC_BL & 0x80) != 0 ) {		\
	SetMemByte(--REG_S, REG_PCl);		\
	SetMemByte(--REG_S, REG_PCh); }		\
    if ( (ACC_BL & 0x40) != 0 ) {		\
	SetMemByte(--REG_S, REG_Ul);		\
	SetMemByte(--REG_S, REG_Uh); }		\
    if ( (ACC_BL & 0x20) != 0 ) {		\
	SetMemByte(--REG_S, REG_Yl);		\
	SetMemByte(--REG_S, REG_Yh); }		\
    if ( (ACC_BL & 0x10) != 0 ) {		\
	SetMemByte(--REG_S, REG_Xl);		\
	SetMemByte(--REG_S, REG_Xh); }		\
    if ( (ACC_BL & 0x08) != 0 )			\
	SetMemByte(--REG_S, REG_DP);		\
    if ( (ACC_BL & 0x04) != 0 )			\
	SetMemByte(--REG_S, REG_B);		\
    if ( (ACC_BL & 0x02) != 0 )			\
	SetMemByte(--REG_S, REG_A);		\
    if ( (ACC_BL & 0x01) != 0 )			\
	SetMemByte(--REG_S, REG_CC);

#define	O_PULS					\
    if ( (ACC_BL & 0x01) != 0 )			\
	REG_CC = GetMemByte(REG_S++);		\
    if ( (ACC_BL & 0x02) != 0 )			\
	REG_A = GetMemByte(REG_S++);		\
    if ( (ACC_BL & 0x04) != 0 )			\
	REG_B = GetMemByte(REG_S++);		\
    if ( (ACC_BL & 0x08) != 0 )			\
	REG_DP = GetMemByte(REG_S++);		\
    if ( (ACC_BL & 0x10) != 0 ) {		\
	REG_Xh = GetMemByte(REG_S++);		\
	REG_Xl = GetMemByte(REG_S++); }		\
    if ( (ACC_BL & 0x20) != 0 ) {		\
	REG_Yh = GetMemByte(REG_S++);		\
	REG_Yl = GetMemByte(REG_S++); }		\
    if ( (ACC_BL & 0x40) != 0 ) {		\
	REG_Uh = GetMemByte(REG_S++);		\
	REG_Ul = GetMemByte(REG_S++); }		\
    if ( (ACC_BL & 0x80) != 0 ) {		\
	REG_PCh = GetMemByte(REG_S++);		\
	REG_PCl = GetMemByte(REG_S++); }

#define	O_PSHU					\
    if ( (ACC_BL & 0x80) != 0 ) {		\
	SetMemByte(--REG_U, REG_PCl);		\
	SetMemByte(--REG_U, REG_PCh); }		\
    if ( (ACC_BL & 0x40) != 0 ) {		\
	SetMemByte(--REG_U, REG_Sl);		\
	SetMemByte(--REG_U, REG_Sh); }		\
    if ( (ACC_BL & 0x20) != 0 ) {		\
	SetMemByte(--REG_U, REG_Yl);		\
	SetMemByte(--REG_U, REG_Yh); }		\
    if ( (ACC_BL & 0x10) != 0 ) {		\
	SetMemByte(--REG_U, REG_Xl);		\
	SetMemByte(--REG_U, REG_Xh); }		\
    if ( (ACC_BL & 0x08) != 0 )			\
	SetMemByte(--REG_U, REG_DP);		\
    if ( (ACC_BL & 0x04) != 0 )			\
	SetMemByte(--REG_U, REG_B);		\
    if ( (ACC_BL & 0x02) != 0 )			\
	SetMemByte(--REG_U, REG_A);		\
    if ( (ACC_BL & 0x01) != 0 )			\
	SetMemByte(--REG_U, REG_CC);

#define	O_PULU					\
    if ( (ACC_BL & 0x01) != 0 )			\
	REG_CC = GetMemByte(REG_U++);		\
    if ( (ACC_BL & 0x02) != 0 )			\
	REG_A = GetMemByte(REG_U++);		\
    if ( (ACC_BL & 0x04) != 0 )			\
	REG_B = GetMemByte(REG_U++);		\
    if ( (ACC_BL & 0x08) != 0 )			\
	REG_DP = GetMemByte(REG_U++);		\
    if ( (ACC_BL & 0x10) != 0 ) {		\
	REG_Xh = GetMemByte(REG_U++);		\
	REG_Xl = GetMemByte(REG_U++); }		\
    if ( (ACC_BL & 0x20) != 0 ) {		\
	REG_Yh = GetMemByte(REG_U++);		\
	REG_Yl = GetMemByte(REG_U++); }		\
    if ( (ACC_BL & 0x40) != 0 ) {		\
	REG_Sh = GetMemByte(REG_U++);		\
	REG_Sl = GetMemByte(REG_U++); }		\
    if ( (ACC_BL & 0x80) != 0 ) {		\
	REG_PCh = GetMemByte(REG_U++);		\
	REG_PCl = GetMemByte(REG_U++); }

#define	O_PSHSW					\
    SetMemByte(--REG_S, REG_F);			\
    SetMemByte(--REG_S, REG_E);

#define	O_PULSW					\
    REG_E = GetMemByte(REG_S++);		\
    REG_F = GetMemByte(REG_S++);

#define	O_PSHUW					\
    SetMemByte(--REG_U, REG_F);			\
    SetMemByte(--REG_U, REG_E);

#define	O_PULUW					\
    REG_E = GetMemByte(REG_U++);		\
    REG_F = GetMemByte(REG_U++);

#define	O_RTS					\
    REG_PCh = GetMemByte(REG_S++);		\
    REG_PCl = GetMemByte(REG_S++);

#define	O_RTI					\
    REG_CC = GetMemByte(REG_S++);		\
    if ( (REG_CC & CC_E) != 0 ) {		\
	REG_A = GetMemByte(REG_S++);		\
	REG_B = GetMemByte(REG_S++);		\
    if ( (REG_MD & MD_N) != 0 ) {		\
	REG_E = GetMemByte(REG_S++);		\
	REG_F = GetMemByte(REG_S++); }		\
	REG_DP = GetMemByte(REG_S++);		\
	REG_Xh = GetMemByte(REG_S++);		\
	REG_Xl = GetMemByte(REG_S++);		\
	REG_Yh = GetMemByte(REG_S++);		\
	REG_Yl = GetMemByte(REG_S++);		\
	REG_Uh = GetMemByte(REG_S++);		\
	REG_Ul = GetMemByte(REG_S++); }		\
    REG_PCh = GetMemByte(REG_S++);		\
    REG_PCl = GetMemByte(REG_S++);		\
    REG_ST &= ~ST_N;

#define	O_ABX		REG_X += REG_B;

#define	PushAll()				\
    SetMemByte(--REG_S, REG_PCl);		\
    SetMemByte(--REG_S, REG_PCh);		\
    SetMemByte(--REG_S, REG_Ul);		\
    SetMemByte(--REG_S, REG_Uh);		\
    SetMemByte(--REG_S, REG_Yl);		\
    SetMemByte(--REG_S, REG_Yh);		\
    SetMemByte(--REG_S, REG_Xl);		\
    SetMemByte(--REG_S, REG_Xh);		\
    SetMemByte(--REG_S, REG_DP);		\
    if ( (REG_MD & MD_N) != 0 ) {		\
		SetMemByte(--REG_S, REG_F);		\
		SetMemByte(--REG_S, REG_E); }	\
    SetMemByte(--REG_S, REG_B);			\
    SetMemByte(--REG_S, REG_A);			\
    SetMemByte(--REG_S, REG_CC);

#define	O_CWAI					\
    REG_CC &= ACC_BL;				\
    REG_CC |= CC_E;				\
    PushAll();					\
    REG_ST |= ST_W;

#define	O_MUL					\
    REG_D = (WORD)REG_A * (WORD)REG_B;		\
    REG_CC &= ~(CC_Z | CC_C);			\
    if ( REG_D == 0 ) REG_CC |= CC_Z;		\
    if ((REG_B & 0x80) != 0 ) REG_CC |= CC_C;

#define	W_MUL					\
    REG_Q = (LONG)REG_D * (LONG)ACC_BX;		\
    REG_CC &= ~(CC_Z | CC_N);			\
    if ( REG_Q == 0L ) REG_CC |= CC_Z;		\
    if ((REG_Q & 0x80000000L) != 0 ) REG_CC |= CC_N;

#define	B_DIV							\
    if ( ACC_BL == 0 ) {				\
		REG_MD |= MD_D;					\
		REG_CC |= CC_E;					\
		PushAll();						\
		REG_PC = GetMemWord(0xFFF0); 	\
	} else {							\
		int d, m;						\
		d = REG_D / ACC_BL;				\
	    m = REG_D % ACC_BL;				\
		REG_B = (BYTE)d;				\
		REG_A = (BYTE)m;				\
		REG_CC &= (CC_C | CC_V | CC_Z | CC_N);		\
		if ( (REG_B & 1) != 0 ) REG_CC |= CC_C;		\
		if ( -128 > d || d > 127 ) REG_CC |= CC_V;	\
		if ( REG_B == 0 ) REG_CC |= CC_Z;			\
		if ( (REG_B & 0x80) != 0 ) REG_CC |= CC_N;	\
	}

#define	W_DIV							\
    if ( ACC_BX == 0 ) {				\
		REG_MD |= MD_D;					\
		REG_CC |= CC_E;					\
		PushAll();						\
		REG_PC = GetMemWord(0xFFF0);	\
	} else {							\
		long d, m;						\
		d = REG_Q / ACC_BX;				\
		m = REG_Q % ACC_BX;				\
		REG_W = (WORD)d;				\
		REG_D = (WORD)m;				\
		REG_CC &= (CC_C | CC_V | CC_Z | CC_N);				\
		if ( (REG_W & 1) != 0 ) REG_CC |= CC_C;				\
		if ( -32768L > d || d > 32767L ) REG_CC |= CC_V;	\
		if ( REG_W == 0 ) REG_CC |= CC_Z;					\
		if ( (REG_W & 0x8000) != 0 ) REG_CC |= CC_N;		\
	}

#define	O_SWI					\
    REG_CC |= CC_E;				\
    PushAll();					\
    REG_CC |= (CC_I | CC_F);			\
    REG_PC = GetMemWord(0xFFFA);

#define	O_SWI2					\
    REG_CC |= CC_E;				\
    PushAll();					\
    REG_PC = GetMemWord(0xFFF4);

#define	O_SWI3					\
    REG_CC |= CC_E;				\
    PushAll();					\
    REG_PC = GetMemWord(0xFFF2);

#define	RegCal(bca, wca)			\
{   BYTE *bs = (BYTE *)RegOfs(ACC_BL >> 4);	\
    BYTE *bd = (BYTE *)RegOfs(ACC_BL & 0x0F);	\
    WORD *ws = (WORD *)bs;			\
    WORD *wd = (WORD *)bd;			\
    switch(ACC_BL & 0x88) {			\
    case 0x00: ACC_BX = *ws; ACC_AX = *wd; wca; *wd = ACC_AX; break; \
    case 0x08: ACC_BX = *ws; ACC_AL = *bd; bca; *bd = ACC_AL; break; \
    case 0x80: ACC_BX = *bs; ACC_AX = *wd; wca; *wd = ACC_AX; break; \
    case 0x88: ACC_BL = *bs; ACC_AL = *bd; bca; *bd = ACC_AL; break; \
    } }

#define	O_ADDR		RegCal(B_ADD, W_ADD)
#define	O_ADCR		RegCal(B_ADC, W_ADC)
#define	O_SUBR		RegCal(B_SUB, W_SUB)
#define	O_SBCR		RegCal(B_SBC, W_SBC)
#define	O_ANDR		RegCal(B_AND, W_AND)
#define	O_ORR		RegCal(B_OR, W_OR)
#define	O_EORR		RegCal(B_EOR, W_EOR)

#define	O_CMPR					\
{   BYTE *bs = (BYTE *)RegOfs(ACC_BL >> 4);	\
    BYTE *bd = (BYTE *)RegOfs(ACC_BL & 0x0F);	\
    WORD *ws = (WORD *)bs;			\
    WORD *wd = (WORD *)bd;			\
    switch(ACC_BL & 0x88) {			\
    case 0x00: ACC_BX = *ws; ACC_AX = *wd; W_SUB; break; \
    case 0x08: ACC_BX = *ws; ACC_AL = *bd; B_SUB; break; \
    case 0x80: ACC_BX = *bs; ACC_AX = *wd; W_SUB; break; \
    case 0x88: ACC_BL = *bs; ACC_AL = *bd; B_SUB; break; \
    } }

