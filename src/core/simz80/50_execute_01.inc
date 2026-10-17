void	CSimZ80::z80emu()
{
	WORD ACC_A;
	BYTE ACC_R, ACC_C;
	BYTE op = GetOp();

	switch(op) {
	case 0000:  		/* NOP  		*/  	/* --X-X--- */
		break;
	case 0010:  		/* EX   AF,AF'  */  	/* --X-X--- */
		SaveFlag();
		ACC_W = REG_AF;
		REG_AF = REG_AFs;
		REG_AFs = ACC_W;
		LoadFlag();
		break;
	case 0020:  		/* DJNZ e   	*/  	/* --X-X--- */
		ACC_A = (signed char)GetOp();
		if ( --REG_B != 0 )
			REG_PC += ACC_A;
		break;
	case 0030:  		/* JR   e   	*/  	/* --X-X--- */
		ACC_A = (signed char)GetOp();
		REG_PC += ACC_A;
		break;
	case 0040:  		/* JR   NZ,e	*/  	/* --X-X--- */
		ACC_A = (signed char)GetOp();
		if ( !FLG_Z )
			REG_PC += ACC_A;
		break;
	case 0050:  		/* JR   Z,e 	*/  	/* --X-X--- */
		ACC_A = (signed char)GetOp();
		if ( FLG_Z )
			REG_PC += ACC_A;
		break;
	case 0060:  		/* JR   NC,e	*/  	/* --X-X--- */
		ACC_A = (signed char)GetOp();
		if ( !FLG_C )
			REG_PC += ACC_A;
		break;
	case 0070:  		/* JR   C,e 	*/  	/* --X-X--- */
		ACC_A = (signed char)GetOp();
		if ( FLG_C )
			REG_PC += ACC_A;
		break;

	case 0001:  		/* LD   BC,nn   */  	/* --X-X--- */
		REG_BC = GetOpW();
		break;
	case 0021:  		/* LD   DE,nn   */  	/* --X-X--- */
		REG_DE = GetOpW();
		break;
	case 0041:  		/* LD   HL,nn   */  	/* --X-X--- */
		REG_HL = GetOpW();
		break;
	case 0061:  		/* LD   SP,nn   */  	/* --X-X--- */
		REG_SP = GetOpW();
		break;

	case 0011:  		/* ADD  HL,BC   */  	/* --XXX-0* */
		ACC_L = REG_HL + REG_BC;
		SetNf(FALSE);
		SetCf(ACC_L > 0xFFFFL);
		REG_HL = (WORD)ACC_L;
		break;
	case 0031:  		/* ADD  HL,DE   */  	/* --XXX-0* */
		ACC_L = REG_HL + REG_DE;
		SetNf(FALSE);
		SetCf(ACC_L > 0xFFFFL);
		REG_HL = (WORD)ACC_L;
		break;
	case 0051:  		/* ADD  HL,HL   */  	/* --XXX-0* */
		ACC_L = REG_HL + REG_HL;
		SetNf(FALSE);
		SetCf(ACC_L > 0xFFFFL);
		REG_HL = (WORD)ACC_L;
		break;
	case 0071:  		/* ADD  HL,SP   */  	/* --XXX-0* */
		ACC_L = REG_HL + REG_SP;
		SetNf(FALSE);
		SetCf(ACC_L > 0xFFFFL);
		REG_HL = (WORD)ACC_L;
		break;

	case 0002:  		/* LD   (BC),A  */  	/* --X-X--- */
		SetByte(REG_BC, REG_A);
		break;
	case 0022:  		/* LD   (DE),A  */  	/* --X-X--- */
		SetByte(REG_DE, REG_A);
		break;
	case 0042:  		/* LD   (nn),HL */  	/* --X-X--- */
		ACC_A = GetOpW();
		SetWord(ACC_A, REG_HL);
		break;
	case 0062:  		/* LD   (nn),A  */  	/* --X-X--- */
		ACC_A = GetOpW();
		SetByte(ACC_A, REG_A);
		break;

	case 0012:  		/* LD   A,(BC)  */  	/* --X-X--- */
		REG_A = GetByte(REG_BC);
		break;
	case 0032:  		/* LD   A,(DE)  */  	/* --X-X--- */
		REG_A = GetByte(REG_DE);
		break;
	case 0052:  		/* LD   HL,(nn) */  	/* --X-X--- */
		ACC_A = GetOpW();
		REG_HL = GetWord(ACC_A);
		break;
	case 0072:  		/* LD   A,(nn)  */  	/* --X-X--- */
		ACC_A = GetOpW();
		REG_A = GetByte(ACC_A);
		break;

	case 0003:  		/* INC  BC  	*/  	/* --X-X--- */
		REG_BC++;
		break;
	case 0023:  		/* INC  DE  	*/  	/* --X-X--- */
		REG_DE++;
		break;
	case 0043:  		/* INC  HL  	*/  	/* --X-X--- */
		REG_HL++;
		break;
	case 0063:  		/* INC  SP  	*/  	/* --X-X--- */
		REG_SP++;
		break;

	case 0013:  		/* DEC  BC  	*/  	/* --X-X--- */
		REG_BC--;
		break;
	case 0033:  		/* DEC  DE  	*/  	/* --X-X--- */
		REG_DE--;
		break;
	case 0053:  		/* DEC  HL  	*/  	/* --X-X--- */
		REG_HL--;
		break;
	case 0073:  		/* DEC  SP  	*/  	/* --X-X--- */
		REG_SP--;
		break;

	case 0004:  		/* INC  B   	*/  	/* **X*XV0- */
	case 0014:  		/* INC  C   	*/  	/* **X*XV0- */
	case 0024:  		/* INC  D   	*/  	/* **X*XV0- */
	case 0034:  		/* INC  E   	*/  	/* **X*XV0- */
	case 0044:  		/* INC  H   	*/  	/* **X*XV0- */
	case 0054:  		/* INC  L   	*/  	/* **X*XV0- */
	case 0064:  		/* INC  (HL)	*/  	/* **X*XV0- */
	case 0074:  		/* INC  A   	*/  	/* **X*XV0- */
		ACC_R = GetRegs(op >> 3);
		ACC_SW = (signed char)ACC_R + 1;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((ACC_R) ^ (1))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(FALSE);
		SetRegs(op >> 3, ACC_B);
		break;

	case 0005:  		/* DEC  B   	*/  	/* **X*XV1- */
	case 0015:  		/* DEC  C   	*/  	/* **X*XV1- */
	case 0025:  		/* DEC  D   	*/  	/* **X*XV1- */
	case 0035:  		/* DEC  E   	*/  	/* **X*XV1- */
	case 0045:  		/* DEC  H   	*/  	/* **X*XV1- */
	case 0055:  		/* DEC  L   	*/  	/* **X*XV1- */
	case 0065:  		/* DEC  (HL)	*/  	/* **X*XV1- */
	case 0075:  		/* DEC  A   	*/  	/* **X*XV1- */
		ACC_R = GetRegs(op >> 3);
		ACC_SW = (signed char)ACC_R - 1;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((ACC_R) ^ (1))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(TRUE);
		SetRegs(op >> 3, ACC_B);
		break;

	case 0006:  		/* LD   B,n 	*/  	/* --X-X--- */
	case 0016:  		/* LD   C,n 	*/  	/* --X-X--- */
	case 0026:  		/* LD   D,n 	*/  	/* --X-X--- */
	case 0036:  		/* LD   E,n 	*/  	/* --X-X--- */
	case 0046:  		/* LD   H,n 	*/  	/* --X-X--- */
	case 0056:  		/* LD   L,n 	*/  	/* --X-X--- */
	case 0066:  		/* LD   (HL),n  */  	/* --X-X--- */
	case 0076:  		/* LD   A,n 	*/  	/* --X-X--- */
		ACC_B = GetOp();
		SetRegs(op >> 3, ACC_B);
		break;

	case 0007:  		/* RLCA 		*/  	/* --X0X-0* */
		SetCf(REG_A & 0x80);
		REG_A <<= 1;
		REG_A |= (FLG_C ? 0x01 : 0x00);
		SetNf(FALSE);
		SetHf(FALSE);
		break;
	case 0017:  		/* RRCA 		*/  	/* --X0X-0* */
		SetCf(REG_A & 0x01);
		REG_A >>= 1;
		REG_A |= (FLG_C ? 0x80 : 0x00);
		SetNf(FALSE);
		SetHf(FALSE);
		break;
	case 0027:  		/* RLA  		*/  	/* --X0X-0* */
		ACC_B = (FLG_C ? 0x01 : 0x00);
		SetCf(REG_A & 0x80);
		REG_A <<= 1;
		REG_A |= ACC_B;
		SetNf(FALSE);
		SetHf(FALSE);
		break;
	case 0037:  		/* RRA  		*/  	/* --X0X-0* */
		ACC_B = (FLG_C ? 0x80 : 0x00);
		SetCf(REG_A & 0x01);
		REG_A >>= 1;
		REG_A |= ACC_B;
		SetNf(FALSE);
		SetHf(FALSE);
		break;
	case 0047:  		/* DAA  		*/  	/* **X*XP-* */
		if ( FLG_H || ((REG_A & 0x0F) > 9) ) {
			REG_A += 0x06;
			FLG_H = TRUE;
		} else
			FLG_H = FALSE;
		if ( FLG_C || (REG_A > 0x9F) ) {
			REG_A += 0x60;
			FLG_C = TRUE;
		} else
			FLG_C = FALSE;
		SetSf(REG_A & 0x80);
		SetZf(REG_A == 0);
		SetPf(Parity(REG_A));
		break;
	case 0057:  		/* CPL  		*/  	/* --X1X-1- */
		REG_A ^= 0xFF;
		SetHf(TRUE);
		SetNf(TRUE);
		break;
	case 0067:  		/* SCF  		*/  	/* --X0X-01 */
		SetHf(FALSE);
		SetNf(FALSE);
		SetCf(TRUE);
		break;
	case 0077:  		/* CCF  		*/  	/* --XXX-0* */
		SetNf(FALSE);
		SetCf(!FLG_C);
		break;

	case 0100:  		/* LD   B,B 	*/  	/* --X-X--- */
	case 0101:  		/* LD   B,C 	*/  	/* --X-X--- */
	case 0102:  		/* LD   B,D 	*/  	/* --X-X--- */
	case 0103:  		/* LD   B,E 	*/  	/* --X-X--- */
	case 0104:  		/* LD   B,H 	*/  	/* --X-X--- */
	case 0105:  		/* LD   B,L 	*/  	/* --X-X--- */
	case 0106:  		/* LD   B,(HL)  */  	/* --X-X--- */
	case 0107:  		/* LD   B,A 	*/  	/* --X-X--- */

	case 0110:  		/* LD   C,B 	*/  	/* --X-X--- */
	case 0111:  		/* LD   C,C 	*/  	/* --X-X--- */
	case 0112:  		/* LD   C,D 	*/  	/* --X-X--- */
	case 0113:  		/* LD   C,E 	*/  	/* --X-X--- */
	case 0114:  		/* LD   C,H 	*/  	/* --X-X--- */
	case 0115:  		/* LD   C,L 	*/  	/* --X-X--- */
	case 0116:  		/* LD   C,(HL)  */  	/* --X-X--- */
	case 0117:  		/* LD   C,A 	*/  	/* --X-X--- */

	case 0120:  		/* LD   D,B 	*/  	/* --X-X--- */
	case 0121:  		/* LD   D,C 	*/  	/* --X-X--- */
	case 0122:  		/* LD   D,D 	*/  	/* --X-X--- */
	case 0123:  		/* LD   D,E 	*/  	/* --X-X--- */
	case 0124:  		/* LD   D,H 	*/  	/* --X-X--- */
	case 0125:  		/* LD   D,L 	*/  	/* --X-X--- */
	case 0126:  		/* LD   D,(HL)  */  	/* --X-X--- */
	case 0127:  		/* LD   D,A 	*/  	/* --X-X--- */

	case 0130:  		/* LD   E,B 	*/  	/* --X-X--- */
	case 0131:  		/* LD   E,C 	*/  	/* --X-X--- */
	case 0132:  		/* LD   E,D 	*/  	/* --X-X--- */
	case 0133:  		/* LD   E,E 	*/  	/* --X-X--- */
	case 0134:  		/* LD   E,H 	*/  	/* --X-X--- */
	case 0135:  		/* LD   E,L 	*/  	/* --X-X--- */
	case 0136:  		/* LD   E,(HL)  */  	/* --X-X--- */
	case 0137:  		/* LD   E,A 	*/  	/* --X-X--- */

	case 0140:  		/* LD   H,B 	*/  	/* --X-X--- */
	case 0141:  		/* LD   H,C 	*/  	/* --X-X--- */
	case 0142:  		/* LD   H,D 	*/  	/* --X-X--- */
	case 0143:  		/* LD   H,E 	*/  	/* --X-X--- */
	case 0144:  		/* LD   H,H 	*/  	/* --X-X--- */
	case 0145:  		/* LD   H,L 	*/  	/* --X-X--- */
	case 0146:  		/* LD   H,(HL)  */  	/* --X-X--- */
	case 0147:  		/* LD   H,A 	*/  	/* --X-X--- */

	case 0150:  		/* LD   L,B 	*/  	/* --X-X--- */
	case 0151:  		/* LD   L,C 	*/  	/* --X-X--- */
	case 0152:  		/* LD   L,D 	*/  	/* --X-X--- */
	case 0153:  		/* LD   L,E 	*/  	/* --X-X--- */
	case 0154:  		/* LD   L,H 	*/  	/* --X-X--- */
	case 0155:  		/* LD   L,L 	*/  	/* --X-X--- */
	case 0156:  		/* LD   L,(HL)  */  	/* --X-X--- */
	case 0157:  		/* LD   L,A 	*/  	/* --X-X--- */

	case 0160:  		/* LD   (HL),B  */  	/* --X-X--- */
	case 0161:  		/* LD   (HL),C  */  	/* --X-X--- */
	case 0162:  		/* LD   (HL),D  */  	/* --X-X--- */
	case 0163:  		/* LD   (HL),E  */  	/* --X-X--- */
	case 0164:  		/* LD   (HL),H  */  	/* --X-X--- */
	case 0165:  		/* LD   (HL),L  */  	/* --X-X--- */
	case 0167:  		/* LD   (HL),A  */  	/* --X-X--- */

	case 0170:  		/* LD   A,B 	*/  	/* --X-X--- */
	case 0171:  		/* LD   A,C 	*/  	/* --X-X--- */
	case 0172:  		/* LD   A,D 	*/  	/* --X-X--- */
	case 0173:  		/* LD   A,E 	*/  	/* --X-X--- */
	case 0174:  		/* LD   A,H 	*/  	/* --X-X--- */
	case 0175:  		/* LD   A,L 	*/  	/* --X-X--- */
	case 0176:  		/* LD   A,(HL)  */  	/* --X-X--- */
	case 0177:  		/* LD   A,A 	*/  	/* --X-X--- */
		ACC_B = GetRegs(op);
		SetRegs(op >> 3, ACC_B);
		break;
	case 0166:  		/* HALT 		*/  	/* --X-X--- */
		REG_PC--;
		break;

	case 0200:  		/* ADD  A,B 	*/  	/* **X*XV0* */
	case 0201:  		/* ADD  A,C 	*/  	/* **X*XV0* */
	case 0202:  		/* ADD  A,D 	*/  	/* **X*XV0* */
	case 0203:  		/* ADD  A,E 	*/  	/* **X*XV0* */
	case 0204:  		/* ADD  A,H 	*/  	/* **X*XV0* */
	case 0205:  		/* ADD  A,L 	*/  	/* **X*XV0* */
	case 0206:  		/* ADD  A,(HL)  */  	/* **X*XV0* */
	case 0207:  		/* ADD  A,A 	*/  	/* **X*XV0* */
		ACC_R = GetRegs(op);
	ADDOP:
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

	case 0210:  		/* ADC  A,B 	*/  	/* **X*XV0* */
	case 0211:  		/* ADC  A,C 	*/  	/* **X*XV0* */
	case 0212:  		/* ADC  A,D 	*/  	/* **X*XV0* */
	case 0213:  		/* ADC  A,E 	*/  	/* **X*XV0* */
	case 0214:  		/* ADC  A,H 	*/  	/* **X*XV0* */
	case 0215:  		/* ADC  A,L 	*/  	/* **X*XV0* */
	case 0216:  		/* ADC  A,(HL)  */  	/* **X*XV0* */
	case 0217:  		/* ADC  A,A 	*/  	/* **X*XV0* */
		ACC_R = GetRegs(op);
    ADCOP:
		ACC_C = (FLG_C ? 1 : 0);
		ACC_SW = REG_A + ACC_R + ACC_C;
		SetCf(ACC_SW > 255);
		ACC_SW = (signed char)REG_A + (signed char)ACC_R + ACC_C;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((REG_A) ^ (ACC_R + ACC_C))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(FALSE);
		REG_A = ACC_B;
		break;

