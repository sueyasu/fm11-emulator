void	CSimZ80::z80emuED()
{
	BYTE op;
	BYTE *reg = NULL;
	WORD ACC_R, ACC_C;

	op = GetOp();

	switch(op & 0070) {
	case 0000: reg = &(REG_B); break;
	case 0010: reg = &(REG_C); break;
	case 0020: reg = &(REG_D); break;
	case 0030: reg = &(REG_E); break;
	case 0040: reg = &(REG_H); break;
	case 0050: reg = &(REG_L); break;
	case 0060: reg = NULL; break;
	case 0070: reg = &(REG_A); break;
	}

	switch(op) {
	case 0x40:
	case 0x50:
	case 0x60:
	case 0x48:
	case 0x58:
	case 0x68:
	case 0x78:  		/* IN   r,(C)   */
		if ( reg == NULL )
			break;
		*reg = GetIO(REG_C);
		SetSf(*reg & 0x80);
		SetZf(*reg == 0);
		SetHf(FALSE);
		SetPf(Parity(*reg));
		SetNf(FALSE);
		break;

	case 0x41:
	case 0x51:
	case 0x61:
	case 0x49:
	case 0x59:
	case 0x69:
	case 0x79:  		/* OUT  (C),r   */
		SetIO(REG_C, *reg);
		break;

	case 0x42:  		/* SBC  HL,BC   */
		ACC_R = REG_BC;
		goto SBCOP;
	case 0x52:  		/* SBC  HL,DE   */
		ACC_R = REG_DE;
		goto SBCOP;
	case 0x62:  		/* SBC  HL,HL   */
		ACC_R = REG_HL;
		goto SBCOP;
	case 0x72:  		/* SBC  HL,SP		**X*XV1* */
		ACC_R = REG_SP;
	SBCOP:
		ACC_C = (FLG_C ? 1 : 0);
		ACC_L = REG_HL - ACC_R - ACC_C;
		SetCf(ACC_L < 0);
		ACC_L = (signed short)REG_HL - (signed short)ACC_R - ACC_C;
		SetVf(ACC_L < -32768 || ACC_L > 32767);
		SetHf((((ACC_W) ^ ((REG_HL) ^ (ACC_R + ACC_C))) & 0x10) != 0);
		SetZf(ACC_W == 0);
		SetSf(ACC_W & 0x8000);
		SetNf(TRUE);
		REG_HL = ACC_W;
		break;

	case 0x4A:  		/* ADC  HL,BC   */
		ACC_R = REG_BC;
		goto ADCOP;
	case 0x5A:  		/* ADC  HL,DE   */
		ACC_R = REG_DE;
		goto ADCOP;
	case 0x6A:  		/* ADC  HL,HL   */
		ACC_R = REG_HL;
		goto ADCOP;
	case 0x7A:  		/* ADC  HL,SP   */
		ACC_R = REG_SP;
	ADCOP:
		ACC_C = (FLG_C ? 1 : 0);
		ACC_L = REG_HL + ACC_R + ACC_C;
		SetCf(ACC_L > 0xFFFF);
		ACC_L = (signed char)REG_HL + (signed char)ACC_R + ACC_C;
		SetVf(ACC_L < -32768 || ACC_L > 32767);
		SetHf((((ACC_W) ^ ((REG_HL) ^ (ACC_R + ACC_C))) & 0x10) != 0);
		SetZf(ACC_W == 0);
		SetSf(ACC_W & 0x80);
		SetNf(FALSE);
		REG_HL = ACC_W;
		break;

	case 0x43:  		/* LD   (nn),BC */
		ACC_W = GetOpW();
		SetWord(ACC_W, REG_BC);
		break;
	case 0x53:  		/* LD   (nn),DE */
		ACC_W = GetOpW();
		SetWord(ACC_W, REG_DE);
		break;
	case 0x63:  		/* LD   (nn),HL */
		ACC_W = GetOpW();
		SetWord(ACC_W, REG_HL);
		break;
	case 0x73:  		/* LD   (nn),SP */
		ACC_W = GetOpW();
		SetWord(ACC_W, REG_SP);
		break;

	case 0x4B:  		/* LD   BC,(nn) */
		ACC_W = GetOpW();
		REG_BC = GetWord(ACC_W);
		break;
	case 0x5B:  		/* LD   DE,(nn) */
		ACC_W = GetOpW();
		REG_DE = GetWord(ACC_W);
		break;
	case 0x6B:  		/* LD   HL,(nn) */
		ACC_W = GetOpW();
		REG_HL = GetWord(ACC_W);
		break;
	case 0x7B:  		/* LD   SP,(nn) */
		ACC_W = GetOpW();
		REG_SP = GetWord(ACC_W);
		break;

	case 0x44:  		/* NEG  		*/
		ACC_SW = 0 - REG_A;
		SetCf(ACC_SW < 0);
		ACC_SW = 0 - (signed char)REG_A;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((0) ^ (REG_A))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(TRUE);
		REG_A = ACC_B;
		break;

	case 0x45:  		/* RETN 		*/
		SetIFF1(FLG_IFF2);
		REG_PC = GetWord(REG_SP);
		REG_SP += 2;
		break;
	case 0x4D:  		/* RETI 		*/
		REG_PC = GetWord(REG_SP);
		REG_SP += 2;
		break;

	case 0x46:  		/* IM   0   	*/
	case 0x56:  		/* IM   1   	*/
	case 0x5E:  		/* IM   2   	*/
		break;

	case 0x47:  		/* LD   I,A 	*/
		REG_I = REG_A;
		break;
	case 0x4F:  		/* LD   R,A 	*/
		REG_R = REG_A;
		break;
	case 0x57:  		/* LD   A,I 	*/
		REG_A = REG_I;
		break;
	case 0x5F:  		/* LD   A,R 	*/
		REG_A = REG_R;
		break;

	case 0x67:  		/* RRD  		*/
		ACC_B = GetByte(REG_HL);
		ACC_BH = REG_A;
		SetByte(REG_HL, (ACC_BH << 4) | (ACC_B >> 4));
		REG_A = (ACC_BH & 0xF0) | (ACC_B & 0x0F);
		SetSf(REG_A & 0x80);
		SetZf(REG_A == 0);
		SetHf(FALSE);
		SetPf(Parity(REG_A));
		SetNf(FALSE);
		break;
	case 0x6F:  		/* RLD  		*/
		ACC_B = GetByte(REG_HL);
		ACC_BH = REG_A;
		SetByte(REG_HL, (ACC_BH & 0x0F) | (ACC_B << 4));
		REG_A = (ACC_BH & 0xF0) | (ACC_B >> 4);
		SetSf(REG_A & 0x80);
		SetZf(REG_A == 0);
		SetHf(FALSE);
		SetPf(Parity(REG_A));
		SetNf(FALSE);
		break;

	case 0xA0:  		/* LDI  		*/
		ACC_B = GetByte(REG_HL);
		SetByte(REG_DE, ACC_B);
		REG_DE++;
		REG_HL++;
		SetPf(--REG_BC != 0);
		SetHf(FALSE);
		SetNf(FALSE);
		break;
	case 0xB0:  		/* LDIR 		*/
		do {
			ACC_B = GetByte(REG_HL);
			SetByte(REG_DE, ACC_B);
			REG_DE++;
			REG_HL++;
		} while ( --REG_BC != 0 );
		SetPf(FALSE);
		SetHf(FALSE);
		SetNf(FALSE);
		break;

	case 0xA8:  		/* LDD  		*/
		ACC_B = GetByte(REG_HL);
		SetByte(REG_DE, ACC_B);
		REG_DE--;
		REG_HL--;
		SetPf(--REG_BC != 0);
		SetHf(FALSE);
		SetNf(FALSE);
		break;
	case 0xB8:  		/* LDDR 		*/
		do {
			ACC_B = GetByte(REG_HL);
			SetByte(REG_DE, ACC_B);
			REG_DE--;
			REG_HL--;
		} while ( --REG_BC != 0 );
		SetPf(FALSE);
		SetHf(FALSE);
		SetNf(FALSE);
		break;

	case 0xA1:  		/* CPI  		*/
		ACC_R = GetByte(REG_HL++);
		ACC_W = REG_A - ACC_R;
		SetSf(ACC_B & 0x80);
		SetZf(ACC_B == 0);
		SetHVSub(ACC_B, REG_A, ACC_R);
		SetNf(TRUE);
		SetPf(--REG_BC != 0);
		break;
	case 0xB1:  		/* CPIR 		*/
		do {
			ACC_R = GetByte(REG_HL++);
			ACC_W = REG_A - ACC_R;
		} while ( --REG_BC != 0 && ACC_B != 0 );
		SetSf(ACC_B & 0x80);
		SetZf(ACC_B == 0);
		SetHVSub(ACC_B, REG_A, ACC_R);
		SetNf(TRUE);
		SetPf(REG_BC != 0);
		break;

	case 0xA9:  		/* CPD  		*/
		ACC_R = GetByte(REG_HL--);
		ACC_W = REG_A - ACC_R;
		SetSf(ACC_B & 0x80);
		SetZf(ACC_B == 0);
		SetHVSub(ACC_B, REG_A, ACC_R);
		SetNf(TRUE);
		SetPf(--REG_BC != 0);
		break;
	case 0xB9:  		/* CPDR 		*/
		do {
			ACC_R = GetByte(REG_HL--);
			ACC_W = REG_A - ACC_R;
		} while ( --REG_BC != 0 && ACC_B != 0 );
		SetSf(ACC_B & 0x80);
		SetZf(ACC_B == 0);
		SetHVSub(ACC_B, REG_A, ACC_R);
		SetNf(TRUE);
		SetPf(REG_BC != 0);
		break;

	case 0xA2:  		/* INI  		*/
		SetByte(REG_HL, GetIO(REG_C));
		REG_HL++;
		SetZf(--REG_B == 0);
		SetNf(TRUE);
		break;  
	case 0xB2:  		/* INIR 		*/
		do {
			SetByte(REG_HL, GetIO(REG_C));
			REG_HL++;
		} while ( --REG_B != 0 );
		SetZf(TRUE);
		SetNf(TRUE);
		break;  

	case 0xAA:  		/* IND  		*/
		SetByte(REG_HL, GetIO(REG_C));
		REG_HL--;
		SetZf(--REG_B == 0);
		SetNf(TRUE);
		break;  
	case 0xBA:  		/* INDR 		*/
		do {
			SetByte(REG_HL, GetIO(REG_C));
			REG_HL--;
		} while ( --REG_B != 0 );
		SetZf(TRUE);
		SetNf(TRUE);
		break;  

	case 0xA3:  		/* OUTI 		*/
		SetIO(REG_C, GetByte(REG_HL++));
		SetZf(--REG_B == 0);
		SetNf(TRUE);
		break;  
	case 0xB3:  		/* OTIR 		*/
		do {
			SetIO(REG_C, GetByte(REG_HL++));
		} while ( --REG_B != 0 );
		SetZf(TRUE);
		SetNf(TRUE);
		break;  
	case 0xBB:  		/* OTDR 		*/
		SetIO(REG_C, GetByte(REG_HL++));
		SetZf(--REG_B == 0);
		SetNf(TRUE);
	case 0xAB:  		/* OUTD 		*/
		do {
			SetIO(REG_C, GetByte(REG_HL--));
		} while ( --REG_B != 0 );
		SetZf(TRUE);
		SetNf(TRUE);
		break;
	}
}
