void	CSimZ80::z80emuCB(WORD *ix)
{
	int op, bit, ofs;
	BYTE *reg = NULL;
	BYTE tmp;

	if ( ix != NULL )
		ofs = (signed char)(GetOp());

	op = GetOp();
	bit = (op & 0070) >> 3;

	switch(op & 0007) {
	case 0000: 
		reg = &(REG_B); 
		break;
	case 0001: 
		reg = &(REG_C); 
		break;
	case 0002: 
		reg = &(REG_D); 
		break;
	case 0003: 
		reg = &(REG_E); 
		break;
	case 0004: 
		reg = &(REG_H); 
		break;
	case 0005: 
		reg = &(REG_L); 
		break;
	case 0006:
		if ( ix == NULL )
			tmp = GetByte(REG_HL);
		else
			tmp = GetByte(*ix + ofs);
		reg = &tmp;
		break;
	case 0007:
		reg = &(REG_A);
		break;
	}

	switch(op & 0300) {
	case 0000:
		switch(op & 0070) {
		case 0000:  	/* RLC  r   	*/
			SetCf(*reg & 0x80);
			*reg <<= 1;
			*reg |= FLG_C;
			SetSf(*reg & 0x80);
			SetZf(*reg == 0);
			SetHf(FALSE);
			SetPf(Parity(*reg));
			SetNf(FALSE);
			break;
		case 0010:  	/* RRC  r   	*/
			SetCf(*reg & 0x01);
			*reg >>= 1;
			*reg |= (FLG_C ? 0x80 : 0);
			SetSf(*reg & 0x80);
			SetZf(*reg == 0);
			SetHf(FALSE);
			SetPf(Parity(*reg));
			SetNf(FALSE);
			break;
		case 0020:  	/* RL   r   	*/
			bit = FLG_C;
			SetCf(*reg & 0x80);
			*reg <<= 1;
			*reg |= bit;
			SetSf(*reg & 0x80);
			SetZf(*reg == 0);
			SetHf(FALSE);
			SetPf(Parity(*reg));
			SetNf(FALSE);
			break;
		case 0030:  	/* RR   r   	*/
			bit = FLG_C;
			SetCf(*reg & 0x01);
			*reg >>= 1;
			*reg |= (bit ? 0x80 : 0);
			SetSf(*reg & 0x80);
			SetZf(*reg == 0);
			SetHf(FALSE);
			SetPf(Parity(*reg));
			SetNf(FALSE);
			break;
		case 0040:  	/* SLA  r   	*/
			SetCf(*reg & 0x80);
			*reg <<= 1;
			SetSf(*reg & 0x80);
			SetZf(*reg == 0);
			SetHf(FALSE);
			SetPf(Parity(*reg));
			SetNf(FALSE);
			break;
		case 0050:  	/* SRA  r   	*/
			SetCf(*reg & 0x01);
			*reg >>= 1;
			*reg |= ((*reg & 0x40) ? 0x80 : 0);
			SetSf(*reg & 0x80);
			SetZf(*reg == 0);
			SetHf(FALSE);
			SetPf(Parity(*reg));
			SetNf(FALSE);
			break;
		case 0060:  	/* Not Code 	*/
			break;
		case 0070:  	/* SRL  r   	*/
			SetCf(*reg & 0x01);
			*reg >>= 1;
			SetSf(*reg & 0x80);
			SetZf(*reg == 0);
			SetHf(FALSE);
			SetPf(Parity(*reg));
			SetNf(FALSE);
			break;
		}
		break;
	case 0100:  		/* BIT  n,r 	*/
		SetZf((*reg & bittab[bit]) == 0);
		SetHf(TRUE);
		SetNf(FALSE);
		break;
	case 0200:  		/* RES  n,r 	*/
		*reg &= ~bittab[bit];
		break;
	case 0300:  		/* SET  n,r 	*/
		*reg |= bittab[bit];
		break;
	}

	if ( reg == &tmp ) {
		if ( *ix == NULL )
			SetByte(REG_HL, tmp);
		else
			SetByte(*ix, tmp);
	}
}
