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

void	CSimZ80::z80irq()
{
	if ( FLG_IFF1 && FLG_IFF2 ) {
		SetIFF1(FALSE);
		SetIFF2(FALSE);
		REG_SP -= 2;
		SetWord(REG_SP, REG_PC);
		REG_PC = 0x0038;
	}
}

void	CSimZ80::z80nmi()
{
	SetIFF1(FALSE);
	REG_SP -= 2;
	SetWord(REG_SP, REG_PC);
	REG_PC = 0x0066;
}

