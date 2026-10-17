#pragma once

#include "portable_types.h"

// CSimZ80 コマンド ターゲット

typedef struct _z80Regs {
        union {
            struct {
                unsigned char   F,A;
                unsigned char   C,B;
                unsigned char   E,D;
                unsigned char   L,H;
                unsigned char   I,R;
                unsigned char   Fs,As;
                unsigned char   Cs,Bs;
                unsigned char   Es,Ds;
                unsigned char   Ls,Hs;
            } b;
            struct {
                unsigned short  AF;
                unsigned short  BC;
                unsigned short  DE;
                unsigned short  HL;
                unsigned short  RI;
                unsigned short  AFs;
                unsigned short  BCs;
                unsigned short  DEs;
                unsigned short  HLs;
            } w;
        } r;
        unsigned short  IX;
        unsigned short  IY;
        unsigned short  SP;
        unsigned short  PC;
        struct {
            unsigned char S,Z,H,P,N,C;
			unsigned char IFF1,IFF2;
        } f;
		BYTE	HD;
} ZREGS;


class CSimZ80
{
public:
	CSimZ80();
	virtual ~CSimZ80();

public:
	BYTE *Memory;
	ZREGS regs;
	union {
		struct {
			BYTE	AL, AH;
			BYTE	BL, BH;
		} b;
		struct {
			WORD	AW;
			WORD	BW;
		} w;
		struct {
			signed short SAW;
			signed short SBW;
		} sw;
		LONG	RL;
	} ACC;

public:
	virtual BYTE GetMem(WORD addr);
	virtual void SetMem(WORD addr, BYTE data);
	virtual BYTE GetIO(BYTE port);
	virtual void SetIO(BYTE port, BYTE data);

	BYTE GetRegs(int breg);
	void SetRegs(int breg, BYTE data);
	void z80emuCB(WORD *ix);
	void z80emuED();
	void z80emuIndex(WORD *ix);
	void z80emu();
	void z80irq();
	void z80nmi();
	void z80dis(char *tmp, unsigned char *mem, unsigned short pc);

	inline BYTE	GetOp() {
		BYTE b = GetMem(regs.PC++);
		return b;
	}
	inline WORD GetOpW() {
		WORD w =  (WORD)GetMem(regs.PC + 0) |
			     ((WORD)GetMem(regs.PC + 1) << 8);
		regs.PC += 2;
		return w;
	}

	inline BYTE	GetByte(WORD addr) { return GetMem(addr); }
	inline WORD GetWord(WORD addr) {
		WORD w =  (WORD)GetMem(addr + 0) |
			     ((WORD)GetMem(addr + 1) << 8);
		return w;
	}

	inline void SetByte(WORD addr, BYTE data) { SetMem(addr, data); }
	inline void SetWord(WORD addr, WORD data) {
		SetMem(addr + 0, (BYTE)data);
		SetMem(addr + 1, (BYTE)(data >> 8));
	}
};

