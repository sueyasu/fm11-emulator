// SimZ80.cpp : 実装ファイル
//

#include "SimZ80.h"

#include <cstdio>
#include <cstring>

////////////////////////////////////////////////////////////////
// CSimZ80

static	const BYTE	partab[256 / 8] = {
		0x96, 0x69, 0x69, 0x96, 0x69, 0x96, 0x96, 0x69, 
		0x69, 0x96, 0x96, 0x69, 0x96, 0x69, 0x69, 0x96, 
		0x69, 0x96, 0x96, 0x69, 0x96, 0x69, 0x69, 0x96, 
		0x96, 0x69, 0x69, 0x96, 0x69, 0x96, 0x96, 0x69 };
static	const BYTE	bittab[8] = {
		0001, 0002, 0004, 0010, 0020, 0040, 0100, 0200 };

CSimZ80::CSimZ80()
{
	Memory = NULL;
	ZeroMemory(&regs, sizeof(regs));
	ZeroMemory(&ACC, sizeof(ACC));
}

CSimZ80::~CSimZ80()
{
}

////////////////////////////////////////////////////////////////
// CSimZ80 メンバー関数

BYTE CSimZ80::GetMem(WORD addr)
{
	return Memory != nullptr ? Memory[addr] : 0;
}

void CSimZ80::SetMem(WORD addr, BYTE data)
{
	if (Memory != nullptr)
		Memory[addr] = data;
}

BYTE CSimZ80::GetIO(BYTE port)
{
	return 0;
}

void CSimZ80::SetIO(BYTE port, BYTE data)
{
}

//////////////////////////////////////////////////////////////////////

#define REG_A           regs.r.b.A
#define REG_F           regs.r.b.F
#define REG_B           regs.r.b.B
#define REG_C           regs.r.b.C
#define REG_D           regs.r.b.D
#define REG_E           regs.r.b.E
#define REG_H           regs.r.b.H
#define REG_L           regs.r.b.L
#define REG_R           regs.r.b.R
#define REG_I           regs.r.b.I
#define REG_As          regs.r.b.As
#define REG_Fs          regs.r.b.Fs
#define REG_AF          regs.r.w.AF
#define REG_BC          regs.r.w.BC
#define REG_DE          regs.r.w.DE
#define REG_HL          regs.r.w.HL
#define REG_AFs         regs.r.w.AFs
#define REG_BCs         regs.r.w.BCs
#define REG_DEs         regs.r.w.DEs
#define REG_HLs         regs.r.w.HLs
#define REG_IX          regs.IX
#define REG_IY          regs.IY
#define REG_SP          regs.SP
#define REG_PC          regs.PC
#define FLG_S           regs.f.S
#define FLG_Z           regs.f.Z
#define FLG_H           regs.f.H
#define FLG_P           regs.f.P
#define FLG_N           regs.f.N
#define FLG_C           regs.f.C
#define FLG_IFF1		regs.f.IFF1
#define FLG_IFF2		regs.f.IFF2

#define	SetSf(f)		FLG_S = ((f) ? 1 : 0)
#define	SetZf(f)		FLG_Z = ((f) ? 1 : 0)
#define	SetHf(f)		FLG_H = ((f) ? 1 : 0)
#define	SetPf(f)		FLG_P = ((f) ? 1 : 0)
#define	SetVf(f)		FLG_P = ((f) ? 1 : 0)
#define	SetNf(f)		FLG_N = ((f) ? 1 : 0)
#define	SetCf(f)		FLG_C = ((f) ? 1 : 0)
#define	SetIFF1(f)		FLG_IFF1 = ((f) ? 1 : 0)
#define	SetIFF2(f)		FLG_IFF2 = ((f) ? 1 : 0)

#define	SetHVAdd(a,b,c)		SetHf(((a) ^ ((b) ^ (c))) & 0x10);\
							SetVf(((a) ^ (b)) & ((a) ^ (c)) & 0x80);

#define	SetHVSub(a,b,c)		SetHf(((a) ^ ((b) ^ (c))) & 0x10);\
							SetVf(((b) ^ (c)) & ((b) ^ (a)) & 0x80);

#define	SaveFlag()		(REG_F = (FLG_S ? 0x80 : 0) |\
								 (FLG_Z ? 0x40 : 0) |\
								 (FLG_H ? 0x10 : 0) |\
								 (FLG_P ? 0x04 : 0) |\
								 (FLG_N ? 0x02 : 0) |\
								 (FLG_C ? 0x01 : 0))

#define	LoadFlag()		SetSf(REG_F & 0x80);\
						SetZf(REG_F & 0x40);\
						SetHf(REG_F & 0x10);\
						SetPf(REG_F & 0x04);\
						SetNf(REG_F & 0x02);\
						SetCf(REG_F & 0x01);

#define	Parity(c)		(partab[(c) / 8] & bittab[(c) % 8])

#define	ACC_B	ACC.b.AL
#define	ACC_BH	ACC.b.AH
#define	ACC_W	ACC.w.AW
#define	ACC_SW	ACC.sw.SAW
#define	ACC_WH	ACC.w.BW
#define	ACC_L	ACC.RL

///////////////////////////////////////////////////////////////

