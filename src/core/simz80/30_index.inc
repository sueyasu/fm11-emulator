void	CSimZ80::z80emuIndex(WORD *ix)
{
	WORD ACC_A, ACC_O;
	BYTE ACC_R, ACC_C;

	switch(GetOp()) {
	case 0x09:			/* ADD	IX,BC		--XXX-0* */
		ACC_W = REG_BC;
		goto ADDOP;
	case 0x19:			/* ADD	IX,DE		--XXX-0* */
		ACC_W = REG_DE;
		goto ADDOP;
	case 0x29:			/* ADD	IX,IX		--XXX-0* */
		ACC_W = REG_IX;
		goto ADDOP;
	case 0x39:			/* ADD	IX,SP		--XXX-0* */
		ACC_W = REG_SP;
	ADDOP:
		ACC_L = *ix + ACC_W;
		SetCf(ACC_L > 0xFFFF);
		SetNf(FALSE);
		*ix = ACC_W;
		break;

	case 0x21:  		/* LD   IX,nn   */
		*ix = GetOpW();
		break;
	case 0x22:  		/* LD   (nn),IX */
		ACC_A = GetOpW();
		SetWord(ACC_A, *ix);
		break;
	case 0x2A:  		/* LD   IX,(nn) */
		ACC_A = GetOpW();
		*ix = GetWord(ACC_A);
		break;

	case 0x23:  		/* INC  IX  	*/
		*ix += 1;
		break;
	case 0x2B:  		/* DEC  IX  	*/
		*ix -= 1;
		break;

	case 0x34:  		/* INC  (IX+d)		**X*XV0- */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		ACC_SW = (signed char)ACC_R + 1;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((ACC_R) ^ (1))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(FALSE);
		SetByte(*ix + ACC_O, ACC_B);
		break;
	case 0x35:  		/* DEC  (IX+d)		**X*XV1- */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		ACC_SW = (signed char)ACC_R - 1;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((ACC_R) ^ (1))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(TRUE);
		SetByte(*ix + ACC_O, ACC_B);
		break;

	case 0x36:  		/* LD   (IX+d),n*/
		ACC_O = (signed char)GetOp();
		ACC_B = GetOp();
		SetByte(*ix + ACC_O, ACC_B);
		break;

	case 0x46:  		/* LD   B,(IX+d)*/
		ACC_O = (signed char)GetOp();
		REG_B = GetByte(*ix + ACC_O);
		break;
	case 0x4E:  		/* LD   C,(IX+d)*/
		ACC_O = (signed char)GetOp();
		REG_C = GetByte(*ix + ACC_O);
		break;
	case 0x56:  		/* LD   D,(IX+d)*/
		ACC_O = (signed char)GetOp();
		REG_D = GetByte(*ix + ACC_O);
		break;
	case 0x5E:  		/* LD   E,(IX+d)*/
		ACC_O = (signed char)GetOp();
		REG_E = GetByte(*ix + ACC_O);
		break;
	case 0x66:  		/* LD   H,(IX+d)*/
		ACC_O = (signed char)GetOp();
		REG_L = GetByte(*ix + ACC_O);
		break;
	case 0x6E:  		/* LD   L,(IX+d)*/
		ACC_O = (signed char)GetOp();
		REG_L = GetByte(*ix + ACC_O);
		break;
	case 0x7E:  		/* LD   A,(IX+d)*/
		ACC_O = (signed char)GetOp();
		REG_A = GetByte(*ix + ACC_O);
		break;

	case 0x70:  		/* LD   (IX+d),B*/
		ACC_O = (signed char)GetOp();
		SetByte(*ix + ACC_O, REG_B);
		break;
	case 0x71:  		/* LD   (IX+d),C*/
		ACC_O = (signed char)GetOp();
		SetByte(*ix + ACC_O, REG_C);
		break;
	case 0x72:  		/* LD   (IX+d),D*/
		ACC_O = (signed char)GetOp();
		SetByte(*ix + ACC_O, REG_D);
		break;
	case 0x73:  		/* LD   (IX+d),E*/
		ACC_O = (signed char)GetOp();
		SetByte(*ix + ACC_O, REG_E);
		break;
	case 0x74:  		/* LD   (IX+d),H*/
		ACC_O = (signed char)GetOp();
		SetByte(*ix + ACC_O, REG_H);
		break;
	case 0x75:  		/* LD   (IX+d),L*/
		ACC_O = (signed char)GetOp();
		SetByte(*ix + ACC_O, REG_L);
		break;
	case 0x77:  		/* LD   (IX+d),A*/
		ACC_O = (signed char)GetOp();
		SetByte(*ix + ACC_O, REG_A);
		break;

	case 0x86:  		/* ADD  (IX+d)  */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		ACC_SW = REG_A + ACC_R;
		SetCf(ACC_SW > 255);
		ACC_SW = (signed char)REG_A + (signed char)ACC_R;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((REG_A) ^ (ACC_R))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(FALSE);
		REG_A = ACC_B;
		break;
	case 0x8E:  		/* ADC  (IX+d)  */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		ACC_C = (FLG_C ? 1 : 0);
		ACC_SW = REG_A + ACC_R + ACC_C;
		SetCf(ACC_SW > 255);
		ACC_SW = (signed char)REG_A + (signed char)ACC_R + ACC_C;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((REG_A) ^ (ACC_R))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(FALSE);
		REG_A = ACC_B;
		break;
	case 0x96:  		/* SUB  (IX+d)  */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		ACC_SW = REG_A - ACC_R;
		SetCf(ACC_SW < 0);
		ACC_SW = (signed char)REG_A - (signed char)ACC_R;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((REG_A) ^ (ACC_R))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(TRUE);
		REG_A = ACC_B;
		break;
	case 0x9E:  		/* SBC  (IX+d)  */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		ACC_C = (FLG_C ? 1 : 0);
		ACC_SW = REG_A - ACC_R - ACC_C;
		SetCf(ACC_SW < 0);
		ACC_SW = (signed char)REG_A - (signed char)ACC_R - ACC_C;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((REG_A) ^ (ACC_R))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(TRUE);
		REG_A = ACC_B;
		break;
	case 0xA6:  		/* AND  (IX+d)  */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		REG_A &= ACC_R;
		SetSf(REG_A & 0x80);
		SetZf(REG_A == 0);
		SetHf(TRUE);
		SetPf(Parity(REG_A));
		SetNf(FALSE);
		SetCf(FALSE);
		break;
	case 0xAE:  		/* XOR  (IX+d)  */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		REG_A ^= ACC_R;
		SetSf(REG_A & 0x80);
		SetZf(REG_A == 0);
		SetHf(FALSE);
		SetPf(Parity(REG_A));
		SetNf(FALSE);
		SetCf(FALSE);
		break;
	case 0xB6:  		/* OR   (IX+d)  */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		REG_A |= ACC_R;
		SetSf(REG_A & 0x80);
		SetZf(REG_A == 0);
		SetHf(FALSE);
		SetPf(Parity(REG_A));
		SetNf(FALSE);
		SetCf(FALSE);
		break;
	case 0xBE:  		/* CP   (IX+d)  */
		ACC_O = (signed char)GetOp();
		ACC_R = GetByte(*ix + ACC_O);
		ACC_SW = REG_A - ACC_R;
		SetCf(ACC_SW < 0);
		ACC_SW = (signed char)REG_A - (signed char)ACC_R;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((REG_A) ^ (ACC_R))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(TRUE);
		break;

	case 0xE1:  		/* POP  IX  	*/
		*ix = GetWord(REG_SP);
		REG_SP += 2;
		break;
	case 0xE5:  		/* PUSH IX  	*/
		REG_SP -= 2;
		SetWord(REG_SP, *ix);
		break;
		
	case 0xE3:  		/* EX   (SP),IX */
		ACC_W = GetWord(REG_SP);
		SetWord(REG_SP, *ix);
		*ix = ACC_W;
		break;
	case 0xE9:  		/* JP   (IX)	*/
		REG_PC = *ix;
		break;
	case 0xF9:  		/* LD   SP,IX   */
		REG_SP = *ix;
		break;

	case 0xCB:
		z80emuCB(ix);
		break;
	}
}
