// Sim09.cpp : 実装ファイル
//

#include "Sim09.h"

#include <cstdio>
#include <cstring>

/*
	unsigned char	F,E;		0,1
	unsigned char	B,A;		2,3
	unsigned char	Xl,Xh;		4,5
	unsigned char	Yl,Yh;		6,7
	unsigned char	Ul,Uh;		8,9
	unsigned char	Sl,Sh;		10,11
	unsigned char	Vl,Vh;		12,13
	unsigned char	PCl,PCh;	14,15
	unsigned char	CC;			16
	unsigned char	DP;			17
	unsigned char	MD;			18
	unsigned char	RV;			19
*/

//									D X Y U  S PC W  V A B CC DP RV RV E F */
static	const int	RegOfsTab[] = { 2,4,6,8,10,14,0,12,3,2,16,17,19,19,1,0 };
static	const BYTE	BitTab[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

#define	GetMemWord(a)	(((WORD)GetMemByte((a)    ) << 8) | \
					          (WORD)GetMemByte((a) + 1))
#define	GetMemLong(a)	(((LONG)GetMemByte((a)    ) << 24) | \
					         ((LONG)GetMemByte((a) + 1) << 16) | \
							 ((LONG)GetMemByte((a) + 2) << 8)  | \
							  (LONG)GetMemByte((a) + 3))

#define	SetMemWord(a,d)	{ SetMemByte((a),     (BYTE)((d) >> 8)); \
						  SetMemByte((a) + 1, (BYTE)(d)); }
#define	SetMemLong(a,d)	{ SetMemByte((a),     (BYTE)((d) >> 24)); \
						  SetMemByte((a) + 1, (BYTE)((d) >> 16)); \
						  SetMemByte((a) + 2, (BYTE)((d) >> 8));  \
						  SetMemByte((a) + 3, (BYTE)(d)); }

#define	REG_A		regs.r.b.A
#define	REG_B		regs.r.b.B
#define	REG_E		regs.r.b.E
#define	REG_F		regs.r.b.F
#define	REG_Xh		regs.r.b.Xh
#define	REG_Xl		regs.r.b.Xl
#define	REG_Yh		regs.r.b.Yh
#define	REG_Yl		regs.r.b.Yl
#define	REG_Uh		regs.r.b.Uh
#define	REG_Ul		regs.r.b.Ul
#define	REG_Sh		regs.r.b.Sh
#define	REG_Sl		regs.r.b.Sl
#define	REG_Vh		regs.r.b.Vh
#define	REG_Vl		regs.r.b.Vl
#define	REG_PCh		regs.r.b.PCh
#define	REG_PCl		regs.r.b.PCl
#define	REG_D		regs.r.w.D
#define	REG_W		regs.r.w.W
#define	REG_X		regs.r.w.X
#define	REG_Y		regs.r.w.Y
#define	REG_U		regs.r.w.U
#define	REG_S		regs.r.w.S
#define	REG_V		regs.r.w.V
#define	REG_PC		regs.r.w.PC
#define	REG_Q		regs.r.q.Q
#define	REG_CC		regs.CC
#define	REG_DP		regs.DP
#define	REG_MD		regs.MD
#define	REG_ST		regs.ST

#define	ACC_AH		accA.b.AH
#define	ACC_AL		accA.b.AL
#define	ACC_AX		accA.AX
#define	ACC_BH		accB.b.AH
#define	ACC_BL		accB.b.AL
#define	ACC_BX		accB.AX
#define	ACC_AQ		accL
#define	ACC_PT		accP

#define	CC_C		0001
#define	CC_V		0002
#define	CC_Z		0004
#define	CC_N		0010
#define	CC_I		0020
#define	CC_H		0040
#define	CC_F		0100
#define	CC_E		0200

#define	MD_N		0001
#define	MD_F		0002
#define	MD_X		0100
#define	MD_D		0200

#define	ST_W		0001
#define	ST_N		0002

#define	RegOfs(n)	((char *)(&regs) + RegOfsTab[n])
#define	GetOpByte()	GetMemByte(REG_PC++)


////////////////////////////////////////////////////////////////
// CSim09

CSim09::CSim09()
{
	SimRESET();
}

CSim09::~CSim09()
{
}

////////////////////////////////////////////////////////////////
// CSim09 メンバー関数

BYTE CSim09::GetMemByte(WORD addr)
{
	return 0;
}
void CSim09::SetMemByte(WORD addr, BYTE data)
{
}

////////////////////////////////////////////////////////////////
// Addressing Mode

WORD	CSim09::GetAddrInd()
{
    BYTE op = GetOpByte();
    WORD ofs = 0;
    WORD *reg = NULL;

    illegalOperand_ = false;
    indexedCycleKnown_ = false;
    indexedCycleExtra_ = 0;
    if ((op & 0x80) == 0) {
        indexedCycleKnown_ = true;
        indexedCycleExtra_ = 1; // 5-bit constant offset
    } else {
        switch (op & 0x0f) {
        case 0x00: case 0x02: indexedCycleExtra_ = 2; indexedCycleKnown_ = true; break;
        case 0x01: case 0x03: indexedCycleExtra_ = 3; indexedCycleKnown_ = true; break;
        case 0x04: indexedCycleKnown_ = true; break;
        case 0x05: case 0x06: case 0x08: case 0x0c:
            indexedCycleExtra_ = 1; indexedCycleKnown_ = true; break;
        case 0x09: case 0x0b: case 0x0d:
            indexedCycleExtra_ = 4; indexedCycleKnown_ = true; break;
        case 0x0f: indexedCycleExtra_ = 4; indexedCycleKnown_ = true; break;
        default: break;
        }
        // These extra modes are specific to 6309 and require separate timing.
        if (Is6309Enabled() && ((op & 0x0f) == 0x07 ||
            (op & 0x0f) == 0x0a || (op & 0x0f) == 0x0e))
            indexedCycleKnown_ = false;
        if ((op & 0x10) != 0 && indexedCycleKnown_)
            indexedCycleExtra_ += 3;
    }
    if (!IsIndexedPostbyteAllowed(op)) {
        illegalOperand_ = true;
        return 0;
    }

    switch(op & 0x60) {
	case 0x00:	/* x00x xxxx X */
		reg = &REG_X;
		break;
	case 0x20:	/* x01x xxxx Y */
		reg = &REG_Y;
		break;
	case 0x40:	/* x10x xxxx U */
		reg = &REG_U;
		break;
    case 0x60:	/* x11x xxxx S */
		reg = &REG_S;
		break;
    }

    if ( (op & 0x80) == 0 ) {	/* 5 bit constant offset */
		ofs = op & 0x0F;
		if ( (op & 0x10) != 0 )
			ofs |= 0xFFF0;
		return (*reg + ofs);
    }

    switch(op & 0x0F) {
    case 0x00:	/* 1RR0 0000 auto inc R+	*/
		switch(op) {
		case 0x90:	/* 1001 0000 [zero W offset	*/
			ofs = REG_W;
			break;
		case 0xB0:	/* 1011 0000 [16 bit W offset	*/
		    ofs = REG_W + GetMemWord(REG_PC);
			REG_PC += 2;
			break;
		case 0xD0:	/* 1101 0000 [auto inc W++	*/
			ofs = REG_W;
			REG_W += 2;
			break;
		case 0xF0:	/* 1111 0000 [auto dec --W	*/
			REG_W -= 2;
			ofs = REG_W;
			break;
		default:
			ofs = *reg;
			*reg += 1;
			break;
		}
		break;
    case 0x01:	/* 1RR0 0001 auto inc R++	*/
		ofs = *reg;
		*reg += 2;
		break;
    case 0x02:	/* 1RR0 0010 auto dec -R	*/
		*reg -= 1;
		ofs = *reg;
		break;
    case 0x03:	/* 1RR0 0011 auto dec --R	*/
		*reg -= 2;
		ofs = *reg;
		break;
    case 0x04:	/* 1RR0 0100 non offset		*/
		ofs = *reg;
		break;

    case 0x05:	/* 1RR0 0101 acc B offset	*/
		ofs = *reg + (signed char)(REG_B);
		break;
    case 0x06:	/* 1RR0 0110 acc A offset	*/
		ofs = *reg + (signed char)(REG_A);
		break;
    case 0x07:	/* 1RR0 0111 acc E offset	*/
		ofs = *reg + (signed char)(REG_E);
		break;
    case 0x0a:	/* 1RR0 1010 acc F offset	*/
		ofs = *reg + (signed char)(REG_F);
		break;
    case 0x0b:	/* 1RR0 1011 acc D offset	*/
		ofs = *reg + REG_D;
		break;
    case 0x0e:	/* 1RR0 1110 acc W offset	*/
		ofs = *reg + REG_W;
		break;

    case 0x0c:	/* 1RR0 1100 8 bit PC offset	*/
		reg = &REG_PC;
    case 0x08:	/* 1RR0 1000 8 bit const ofs	*/
		ofs = (signed char)GetOpByte();
		ofs += *reg;
		break;

    case 0x0d:	/* 1RR0 1101 16 bit PC offset	*/
		reg = &REG_PC;
    case 0x09:	/* 1RR0 1001 16 bit const ofs	*/
		ofs = GetMemWord(REG_PC); REG_PC += 2;
		ofs += *reg;
		break;

    case 0x0f:	/* 1RR0 1111 extendid in direct	*/
		switch(op) {
		case 0x8F:	/* 1000 1111 zero W offset	*/
			ofs = REG_W;
			break;
		case 0xAF:	/* 1010 1111 16 bit W offset	*/
			ofs = REG_W + GetMemWord(REG_PC);
			REG_PC += 2;
			break;
		case 0xCF:	/* 1100 1111 auto inc W++	*/
			ofs = REG_W;
			REG_W += 2;
			break;
		case 0xEF:	/* 1110 1111 auto dec --W	*/
			REG_W -= 2;
			ofs = REG_W;
			break;
		default:
			ofs = GetMemWord(REG_PC);
			REG_PC += 2;
			break;
		}
    }

    if ( (op & 0x10) != 0 ) 	/* in direct */
		ofs = GetMemWord(ofs);

    return ofs;
}

