#define	BitCal					\
    BYTE *bs = NULL, rv = 0;				\
    switch(ACC_BL & 0xC0) {			\
    case 0x00: bs = &REG_CC; break;		\
    case 0x40: bs = &REG_A; break;		\
    case 0x80: bs = &REG_B; break;		\
    case 0xC0: bs = &rv; break; }

#define	O_BAND					\
{   BitCal					\
    if ( (ACC_AL & BitTab[(ACC_BL >> 3) & 7]) == 0 ) \
	*bs &= ~BitTab[ACC_BL & 7]; 		\
}
#define	O_IAND					\
{   BitCal					\
    if ( (ACC_AL & BitTab[(ACC_BL >> 3) & 7]) != 0 ) \
	*bs &= ~BitTab[ACC_BL & 7];		\
}
#define	O_BOR					\
{   BitCal					\
    if ( (ACC_AL & BitTab[(ACC_BL >> 3) & 7]) != 0 ) \
	*bs |= BitTab[ACC_BL & 7];		\
}
#define	O_IOR					\
{   BitCal					\
    if ( (ACC_AL & BitTab[(ACC_BL >> 3) & 7]) == 0 ) \
	*bs |= BitTab[ACC_BL & 7];		\
}
#define	O_BEOR					\
{   BitCal					\
    if ( (ACC_AL & BitTab[(ACC_BL >> 3) & 7]) != 0 ) \
	*bs ^= BitTab[ACC_BL & 7];		\
}
#define	O_IEOR					\
{   BitCal					\
    if ( (ACC_AL & BitTab[(ACC_BL >> 3) & 7]) == 0 ) \
	*bs ^= BitTab[ACC_BL & 7];		\
}
#define	O_LDBT					\
{   BitCal					\
    *bs &= ~BitTab[ACC_BL & 7];			\
    if ( (ACC_AL & BitTab[(ACC_BL >> 3) & 7]) != 0 ) \
	*bs |= BitTab[ACC_BL & 7];		\
}
#define	O_STBT					\
{   BitCal					\
    ACC_AL &= ~BitTab[ACC_BL & 7];		\
    if ( (*bs & BitTab[(ACC_BL >> 3) & 7]) != 0 ) \
	ACC_AL |= BitTab[ACC_BL & 7];		\
    REG_CC &= ~(CC_Z | CC_N);			\
    if ( ACC_AL == 0 ) REG_CC |= CC_Z;		\
    if ( (ACC_AL & 0x80) != 0 ) REG_CC |= CC_N;	\
}

#define	O_BITMD					\
    REG_CC &= ~CC_Z;				\
    if ( (REG_MD & ACC_BL) != 0 )		\
	REG_CC |= CC_Z;				\
    REG_MD &= 0x3F;

#define	O_NON

#define	O_ERR					\
    REG_MD |= MD_X;				\
    REG_CC |= CC_E;				\
    PushAll();					\
    REG_PC = GetMemWord(0xFFF0);

#define	O_TFMI					\
{   BYTE c;					\
    WORD *ws = (WORD *)RegOfs(ACC_BL >> 4);	\
    WORD *wd = (WORD *)RegOfs(ACC_BL & 0x0F);	\
    if ( ACC_BL <= 0x44 ) {			\
    	do {					\
	    c = GetMemByte(*ws); *ws += 1;	\
	    SetMemByte(*wd, c);  *wd += 1;	\
        } while ( --REG_W != 0 );		\
    } else {					\
	O_ERR;					\
    }						\
}
#define	O_TFMD					\
{   BYTE c;					\
    WORD *ws = (WORD *)RegOfs(ACC_BL >> 4);	\
    WORD *wd = (WORD *)RegOfs(ACC_BL & 0x0F);	\
    if ( ACC_BL <= 0x44 ) {			\
    	do {					\
	    c = GetMemByte(*ws); *ws -= 1;	\
	    SetMemByte(*wd, c);  *wd -= 1;	\
        } while ( --REG_W != 0 );		\
    } else {					\
	O_ERR;					\
    }						\
}
#define	O_TFMS					\
{   BYTE c;					\
    WORD *ws = (WORD *)RegOfs(ACC_BL >> 4);	\
    WORD *wd = (WORD *)RegOfs(ACC_BL & 0x0F);	\
    if ( ACC_BL <= 0x44 ) {			\
    	do {					\
	    c = GetMemByte(*ws); *ws += 1;	\
	    SetMemByte(*wd, c);			\
        } while ( --REG_W != 0 );		\
    } else {					\
	O_ERR;					\
    }						\
}
#define	O_TFML					\
{   BYTE c;					\
    WORD *ws = (WORD *)RegOfs(ACC_BL >> 4);	\
    WORD *wd = (WORD *)RegOfs(ACC_BL & 0x0F);	\
    if ( ACC_BL <= 0x44 ) {			\
    	do {					\
	    c = GetMemByte(*ws);		\
	    SetMemByte(*wd, c);  *wd += 1;	\
        } while ( --REG_W != 0 );		\
    } else {					\
	O_ERR;					\
    }						\
}

#define	DIR	ACC_PT = ((REG_DP << 8) | GetOpByte());
#define	IND	ACC_PT = GetAddrInd();
#define	EXT	ACC_PT = GetMemWord(REG_PC); REG_PC += 2;

#define	I_BYTE	ACC_BL = GetOpByte();
#define	I_WORD	ACC_BH = GetOpByte(); ACC_BL = GetOpByte();
#define	I_LONG	ACC_AQ = GetMemLong(REG_PC); REG_PC += 4;
#define	D_BYTE	ACC_BL = GetMemByte(ACC_PT);
#define	D_WORD	ACC_BX = GetMemWord(ACC_PT);
#define	D_LONG	ACC_AQ = GetMemLong(ACC_PT);
#define	D_A	ACC_BL = REG_A;
#define	D_B	ACC_BL = REG_B;
#define	D_E	ACC_BL = REG_E;
#define	D_F	ACC_BL = REG_F;
#define	D_CC	ACC_BL = REG_CC;
#define	D_DP	ACC_BL = REG_DP;
#define	D_MD	ACC_BL = REG_MD;
#define	D_D	ACC_BX = REG_D;
#define	D_W	ACC_BX = REG_W;
#define	D_X	ACC_BX = REG_X;
#define	D_Y	ACC_BX = REG_Y;
#define	D_U	ACC_BX = REG_U;
#define	D_S	ACC_BX = REG_S;
#define	D_PC	ACC_BX = REG_PC;
#define	D_V	ACC_BX = REG_V;
#define	D_PT	ACC_BX = ACC_PT;
#define	D_SX	ACC_BH = (ACC_BL & 0x80 ? 0xFF : 0x00);

#define	S_BYTE	ACC_AL = GetMemByte(ACC_PT);
#define	S_WORD	ACC_AX = GetMemWord(ACC_PT);
#define	S_LONG	ACC_AQ = GetMemLong(ACC_PT);
#define	S_A	ACC_AL = REG_A;
#define	S_B	ACC_AL = REG_B;
#define	S_E	ACC_AL = REG_E;
#define	S_F	ACC_AL = REG_F;
#define	S_CC	ACC_AL = REG_CC;
#define	S_DP	ACC_AL = REG_DP;
#define	S_MD	ACC_AL = REG_MD;
#define	S_D	ACC_AX = REG_D;
#define	S_W	ACC_AX = REG_W;
#define	S_X	ACC_AX = REG_X;
#define	S_Y	ACC_AX = REG_Y;
#define	S_U	ACC_AX = REG_U;
#define	S_S	ACC_AX = REG_S;
#define	S_PC	ACC_AX = REG_PC;
#define	S_V	ACC_AX = REG_V;
#define	S_Q	ACC_AQ = REG_Q;
#define	S_PT	ACC_AX = ACC_PT;
#define	S_BX	ACC_AX = ACC_BX;

#define	E_BYTE	SetMemByte(ACC_PT, ACC_AL);
#define	E_WORD	SetMemWord(ACC_PT, ACC_AX);
#define	E_LONG	SetMemLong(ACC_PT, ACC_AQ);
#define	E_A	REG_A = ACC_AL;
#define	E_B	REG_B = ACC_AL;
#define	E_E	REG_E = ACC_AL;
#define	E_F	REG_F = ACC_AL;
#define	E_CC	REG_CC = ACC_AL;
#define	E_DP	REG_DP = ACC_AL;
#define	E_MD	REG_MD = ACC_AL;
#define	E_D	REG_D = ACC_AX;
#define	E_W	REG_W = ACC_AX;
#define	E_X	REG_X = ACC_AX;
#define	E_Y	REG_Y = ACC_AX;
#define	E_U	REG_U = ACC_AX;
#define	E_S	REG_S = ACC_AX;
#define	E_PC	REG_PC = ACC_AX;
#define	E_V	REG_V = ACC_AX;
#define	E_Q	REG_Q = ACC_AQ;

#define	OPDEF(c,p)	case c: p; break;

