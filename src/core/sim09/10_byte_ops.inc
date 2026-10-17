////////////////////////////////////////////////////////////////
//	Byte OpCode

#define	TstByte()				\
    REG_CC &= ~(CC_V | CC_Z | CC_N);		\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AL & 0x80 ) REG_CC |= CC_N;

#define	BitByte(c)				\
    REG_CC &= ~(CC_C | CC_Z | CC_N);		\
    if ( (c) != 0 ) REG_CC |= CC_C;		\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AL & 0x80 ) REG_CC |= CC_N;

#define	B_NEG					\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N);	\
    if ( ACC_AL == 0x80 ) REG_CC |= CC_V;	\
    if ( ACC_AL != 0x00 ) REG_CC |= CC_C;	\
    ACC_AL = ~ACC_AL + 1;			\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AL & 0x80 ) REG_CC |= CC_N;

#define	B_COM					\
    ACC_AL = ~ACC_AL; 				\
    TstByte(); 					\
    REG_CC |= CC_C;

#define	B_LSR					\
{   int cflag = (ACC_AL & 0x01);		\
    ACC_AL >>= 1;				\
    BitByte(cflag); }

#define	B_ROR					\
{   int cflag = (ACC_AL & 0x01);		\
    int nflag = (REG_CC & CC_C);		\
    ACC_AL >>= 1;				\
    if ( nflag ) ACC_AL |= 0x80;		\
    BitByte(cflag); }

#define	B_ASR					\
{   int cflag = (ACC_AL & 0x01);		\
    int nflag = (ACC_AL & 0x80);		\
    ACC_AL >>= 1;				\
    if ( nflag ) ACC_AL |= 0x80;		\
    BitByte(cflag); }

#define	B_ASL					\
{   int cflag = (ACC_AL & 0x80);		\
    ACC_AL <<= 1;				\
    REG_CC &= ~CC_V;				\
    if ( cflag != (ACC_AL & 0x80) ) REG_CC |= CC_V; \
    BitByte(cflag); }

#define	B_ROL					\
{   int cflag = (ACC_AL & 0x80);		\
    int nflag = (REG_CC & CC_C);		\
    ACC_AL <<= 1;				\
    if ( nflag ) ACC_AL |= 0x01;		\
    REG_CC &= ~CC_V;				\
    if ( (cflag ^ (ACC_AL & 0x80)) != 0 ) REG_CC |= CC_V; \
    BitByte(cflag); }

#define	B_DEC					\
    REG_CC &= ~(CC_V | CC_Z | CC_N);		\
    if ( ACC_AL == 0x80 ) REG_CC |= CC_V;	\
    ACC_AL -= 1;				\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AL & 0x80 ) REG_CC |= CC_N;

#define	B_INC					\
    REG_CC &= ~(CC_V | CC_Z | CC_N);		\
    if ( ACC_AL == 0x7F ) REG_CC |= CC_V;	\
    ACC_AL += 1;				\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z;		\
    if ( ACC_AL & 0x80 ) REG_CC |= CC_N;

#define	B_SUB					\
{   short c;					\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N);	\
    c = ACC_AL - ACC_BL;			\
    if ( c < 0 ) REG_CC |= CC_C;		\
    c = (signed char)ACC_AL - (signed char)ACC_BL; \
    if ( c < -128 || c > 127 ) REG_CC |= CC_V;	\
    ACC_AL = (BYTE)c;				\
    if ( (ACC_AL & 0x80) != 0 ) REG_CC |= CC_N;	\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z; }

#define	B_SBC					\
{   short c;					\
    short r = ((REG_CC & CC_C) != 0 ? 1 : 0);	\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N);	\
    c = ACC_AL - ACC_BL - r;			\
    if ( c < 0 ) REG_CC |= CC_C;		\
    c = (signed char)ACC_AL - (signed char)ACC_BL - r; \
    if ( c < -128 || c > 127 ) REG_CC |= CC_V;	\
    ACC_AL = (BYTE)c;				\
    if ( (ACC_AL & 0x80) != 0 ) REG_CC |= CC_N;	\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z; }

#define	B_ADD					\
{   short c;					\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N | CC_H); \
    c = ACC_AL + ACC_BL;			\
    if ( c > 255 ) REG_CC |= CC_C;		\
    c = (signed char)ACC_AL + (signed char)ACC_BL; \
    if ( c < -128 || c > 127 ) REG_CC |= CC_V;	\
    if ( (((c) ^ ((ACC_AL) ^ (ACC_BL))) & 0x10) != 0 ) REG_CC |= CC_H;\
    ACC_AL = (BYTE)c;				\
    if ( (ACC_AL & 0x80) != 0 ) REG_CC |= CC_N;	\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z; }

#define	B_ADC					\
{   short c;					\
    short r = ((REG_CC & CC_C) != 0 ? 1 : 0);	\
    REG_CC &= ~(CC_C | CC_V | CC_Z | CC_N | CC_H); \
    c = ACC_AL + ACC_BL + r;			\
    if ( c > 255 ) REG_CC |= CC_C;		\
    c = (signed char)ACC_AL + (signed char)ACC_BL + r; \
    if ( c < -128 || c > 127 ) REG_CC |= CC_V;	\
    if ( (((c) ^ ((ACC_AL) ^ (ACC_BL + r))) & 0x10) != 0 ) REG_CC |= CC_H;\
    ACC_AL = (BYTE)c;				\
    if ( (ACC_AL & 0x80) != 0 ) REG_CC |= CC_N;	\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z; }

#define	B_TST		TstByte();
#define	B_CLR		ACC_AL = 0; TstByte(); REG_CC &= ~CC_C;
#define	B_AND		ACC_AL &= ACC_BL; TstByte();
#define	B_OR		ACC_AL |= ACC_BL; TstByte();
#define	B_EOR		ACC_AL ^= ACC_BL; TstByte();

