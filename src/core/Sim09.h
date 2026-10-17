#pragma once

#include "portable_types.h"

// CSim09 コマンド ターゲット

typedef	struct _09Resg	{
	union {
	    struct {
		BYTE	F,E;
		BYTE	B,A;
		BYTE	Xl,Xh;
		BYTE	Yl,Yh;
		BYTE	Ul,Uh;
		BYTE	Sl,Sh;
		BYTE	Vl,Vh;
		BYTE	PCl,PCh;
	    } b;
	    struct {
		WORD	W;
		WORD	D;
		WORD	X;
		WORD	Y;
		WORD	U;
		WORD	S;
		WORD	V;
		WORD	PC;
	    } w;
	    struct {
		LONG	Q;
		LONG	XY;
		LONG	US;
		LONG	VP;
	    } q;
	} r;
	BYTE	CC;
	BYTE	DP;
	BYTE	MD;
	BYTE	RV;
	WORD	ST;
	BYTE	HD;
} REGS;

typedef	union _09Acc {
	struct	{
		BYTE	AL, AH;
	} b;
	WORD	AX;
} ACC;

class CSim09
{
public:
	REGS				regs;

public:
	CSim09();
	virtual ~CSim09();

	virtual BYTE GetMemByte(WORD addr);
	virtual void SetMemByte(WORD addr, BYTE data);

	void Set6309Enabled(bool enabled);
	bool Is6309Enabled() const;

	WORD GetAddrInd();
	void SimRESET();
	void SimFIRQ();
	void SimIRQ();
	void SimNMI();
	void SimONE();
	// Zero denotes an instruction without verified timing data.
	std::uint32_t LastInstructionCycles() const { return lastInstructionCycles_; }

	char *DisInd(WORD *ptr);
	char *DisPhs(BYTE pt, BYTE op);
	char *DisReg(BYTE pt);
	WORD Dis09(char *tmp, WORD addr);

private:
	void BeginInstructionTiming(WORD opcode);
	void FinishInstructionTiming(WORD opcode);
	bool IsOpcodeAllowed(WORD opcode) const;
	bool IsIndexedPostbyteAllowed(BYTE postbyte) const;
	bool IsTransferPostbyteAllowed(BYTE postbyte) const;
	bool ConsumeIllegalOperand();

	std::uint32_t lastInstructionCycles_ = 0;
	std::uint32_t indexedCycleExtra_ = 0;
	bool indexedCycleKnown_ = false;
	BYTE stackTimingMask_ = 0;
	bool enable6309_ = true;
	bool illegalOperand_ = false;
};

