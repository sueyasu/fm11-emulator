////////////////////////////////////////////////////////////////
//	Word OpCode

#define	TstWord()				\
    REG_CC &= ~(CC_V | CC_Z | CC_N);		\
    if ( ACC_AX == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AX & 0x8000 ) REG_CC |= CC_N;

#define	BitWord(c)				\
    REG_CC &= ~(CC_C | CC_Z | CC_N);		\
    if ( (c) != 0 ) REG_CC |= CC_C;		\
    if ( ACC_AX == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AX & 0x8000 ) REG_CC |= CC_N;

#define	W_NEG					\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N);	\
    if ( ACC_AX == 0x8000 ) REG_CC |= CC_V;	\
    if ( ACC_AX != 0x0000 ) REG_CC |= CC_C;	\
    ACC_AX = ~ACC_AX + 1;			\
    if ( ACC_AX == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AX & 0x8000 ) REG_CC |= CC_N;

#define	W_COM					\
    ACC_AX = ~ACC_AX;				\
    TstWord();					\
    REG_CC |= CC_C;

#define	W_LSR					\
{   int cflag = (ACC_AX & 0x0001);		\
    ACC_AX >>= 1;				\
    BitWord(cflag); }

#define	W_ROR					\
{   int cflag = (ACC_AX & 0x0001);		\
    int nflag = (REG_CC & CC_C);		\
    ACC_AX >>= 1;				\
    if ( nflag ) ACC_AX |= 0x8000;		\
    BitWord(cflag); }

#define	W_ASR					\
{   int cflag = (ACC_AX & 0x0001);		\
    int nflag = (ACC_AX & 0x8000);		\
    ACC_AX >>= 1;				\
    if ( nflag ) ACC_AX |= 0x8000;		\
    BitWord(cflag); }

#define	W_ASL					\
{   int cflag = (ACC_AX & 0x8000);		\
    ACC_AX <<= 1;				\
    BitWord(cflag); }

#define	W_ROL					\
{   int cflag = (ACC_AX & 0x8000);		\
    int nflag = (REG_CC & CC_C);		\
    ACC_AX <<= 1;				\
    if ( nflag ) ACC_AX |= 0x0001;		\
    BitWord(cflag); }

#define	W_DEC					\
    REG_CC &= ~(CC_V | CC_Z | CC_N);		\
    if ( ACC_AX == 0x8000 ) REG_CC |= CC_V;	\
    ACC_AX -= 1;				\
    if ( ACC_AX == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AX & 0x8000 ) REG_CC |= CC_N;

#define	W_INC					\
    REG_CC &= ~(CC_V | CC_Z | CC_N);		\
    if ( ACC_AX == 0x7FFF ) REG_CC |= CC_V;	\
    ACC_AX += 1;				\
    if ( ACC_AX == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AX & 0x8000 ) REG_CC |= CC_N;

#define	W_SUB					\
{ long c;					\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N);	\
    c = ACC_AX - ACC_BX;			\
    if ( c < 0 ) REG_CC |= CC_C;		\
    c = (signed short)ACC_AX - (signed short)ACC_BX; \
    if ( c < -32768L || c > 32767L ) REG_CC |= CC_V; \
    ACC_AX = (WORD)c;				\
    if ( (ACC_AX & 0x8000) != 0 ) REG_CC |= CC_N; \
    if ( ACC_AX == 0x0000 ) REG_CC |= CC_Z; }

#define	W_SBC					\
{   long c;					\
    short r = ((REG_CC & CC_C) != 0 ? 1 : 0);	\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N);	\
    c = ACC_AX - ACC_BX - r;			\
    if ( c < 0 ) REG_CC |= CC_C;		\
    c = (signed short)ACC_AX - (signed short)ACC_BX - r; \
    if ( c < -32768L || c > 32767L ) REG_CC |= CC_V; \
    ACC_AX = (WORD)c;				\
    if ( (ACC_AX & 0x8000) != 0 ) REG_CC |= CC_N; \
    if ( ACC_AX == 0x0000 ) REG_CC |= CC_Z; }

#define	W_ADD					\
{   long c;					\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N);	\
    c = ACC_AX + ACC_BX;			\
    if ( c > 0xFFFFL ) REG_CC |= CC_C;		\
    c = (signed short)ACC_AX + (signed short)ACC_BX; \
    if ( c < -32768L || c > 32767L ) REG_CC |= CC_V; \
    ACC_AX = (WORD)c;				\
    if ( (ACC_AX & 0x8000) != 0 ) REG_CC |= CC_N; \
    if ( ACC_AX == 0x0000 ) REG_CC |= CC_Z; }

#define	W_ADC					\
{   long c;					\
    short r = ((REG_CC & CC_C) != 0 ? 1 : 0);	\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N);	\
    c = ACC_AX + ACC_BX + r;			\
    if ( c > 0xFFFFL ) REG_CC |= CC_C;		\
    c = (signed short)ACC_AX + (signed short)ACC_BX + r; \
    if ( c < -32768L || c > 32767L ) REG_CC |= CC_V; \
    ACC_AX = (WORD)c;				\
    if ( (ACC_AX & 0x8000) != 0 ) REG_CC |= CC_N; \
    if ( ACC_AX == 0x0000 ) REG_CC |= CC_Z; }

#define	W_TST	TstWord();
#define	W_LEA	REG_CC &= ~CC_Z; if ( ACC_AX == 0 ) REG_CC |= CC_Z;
#define	W_CLR	ACC_AX = 0; TstWord(); REG_CC &= ~CC_C;
#define	W_AND	ACC_AX &= ACC_BX; TstWord();
#define	W_EOR	ACC_AX ^= ACC_BX; TstWord();
#define	W_OR	ACC_AX |= ACC_BX; TstWord();

