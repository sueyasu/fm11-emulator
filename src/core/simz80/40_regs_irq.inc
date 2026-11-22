BYTE	CSimZ80::GetRegs(int breg)
{
	switch(breg & 7) {
	case 0: return REG_B;
	case 1: return REG_C;
	case 2: return REG_D;
	case 3: return REG_E;
	case 4: return REG_H;
	case 5: return REG_L;
	case 6: return GetByte(REG_HL);
	case 7: return REG_A;
	}
	return 0;
}

void	CSimZ80::SetRegs(int breg, BYTE data)
{
	switch(breg & 7) {
	case 0: REG_B = data; break;
	case 1: REG_C = data; break;
	case 2: REG_D = data; break;
	case 3: REG_E = data; break;
	case 4: REG_H = data; break;
	case 5: REG_L = data; break;
	case 6: SetByte(REG_HL, data); break;
	case 7: REG_A = data; break;
	}
}

void CSimZ80::z80irq()
{
    // NMOS Z80 accepts maskable IRQ only with IFF1 and after the
    // instruction following EI. IFF2 is retained across NMI.
    if (!FLG_IFF1 || eiDelay_!=0) return;
    SetIFF1(FALSE);
    SetIFF2(FALSE);
    eiDelay_=0;
    if (halted_) { REG_PC=WORD(REG_PC+1); halted_=false; }
    FetchRefresh(); // interrupt acknowledge M1 cycle
    REG_SP=WORD(REG_SP-2);
    SetWord(REG_SP,REG_PC);
    if(interruptMode_==2) {
        REG_PC=GetWord(WORD((WORD(REG_I)<<8)|interruptDataBus_));
        instructionCycles_=19;
    } else {
        // IM0 assumes RST 38h on the data bus (FFh), matching the
        // machine's current default pull-up behavior. IM1 is fixed.
        REG_PC=0x0038;
        instructionCycles_=13;
    }
}

void CSimZ80::z80nmi()
{
    if(halted_) { REG_PC=WORD(REG_PC+1); halted_=false; }
    FetchRefresh();
    SetIFF1(FALSE); // preserve IFF2 for RETN
    eiDelay_=0;
    REG_SP=WORD(REG_SP-2);
    SetWord(REG_SP,REG_PC);
    REG_PC=0x0066;
    instructionCycles_=11;
}
