	case 0220:  		/* SUB  B   	*/  	/* **X*XV1* */
	case 0221:  		/* SUB  C   	*/  	/* **X*XV1* */
	case 0222:  		/* SUB  D   	*/  	/* **X*XV1* */
	case 0223:  		/* SUB  E   	*/  	/* **X*XV1* */
	case 0224:  		/* SUB  H   	*/  	/* **X*XV1* */
	case 0225:  		/* SUB  L   	*/  	/* **X*XV1* */
	case 0226:  		/* SUB  (HL)	*/  	/* **X*XV1* */
	case 0227:  		/* SUB  A   	*/  	/* **X*XV1* */
		ACC_R = GetRegs(op);
    SUBOP:
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

	case 0230:  		/* SBC  A,B 	*/  	/* **X*XV1* */
	case 0231:  		/* SBC  A,C 	*/  	/* **X*XV1* */
	case 0232:  		/* SBC  A,D 	*/  	/* **X*XV1* */
	case 0233:  		/* SBC  A,E 	*/  	/* **X*XV1* */
	case 0234:  		/* SBC  A,H 	*/  	/* **X*XV1* */
	case 0235:  		/* SBC  A,L 	*/  	/* **X*XV1* */
	case 0236:  		/* SBC  A,(HL)  */  	/* **X*XV1* */
	case 0237:  		/* SBC  A,A 	*/  	/* **X*XV1* */
		ACC_R = GetRegs(op);
    SBCOP:
		ACC_C = (FLG_C ? 1 : 0);
		ACC_SW = REG_A - ACC_R - ACC_C;
		SetCf(ACC_SW < 0);
		ACC_SW = (signed char)REG_A - (signed char)ACC_R - ACC_C;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((REG_A) ^ (ACC_R + ACC_C))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(TRUE);
		REG_A = ACC_B;
		break;

	case 0240:  		/* AND  B   	*/  	/* **X1XP00 */
	case 0241:  		/* AND  C   	*/  	/* **X1XP00 */
	case 0242:  		/* AND  D   	*/  	/* **X1XP00 */
	case 0243:  		/* AND  E   	*/  	/* **X1XP00 */
	case 0244:  		/* AND  H   	*/  	/* **X1XP00 */
	case 0245:  		/* AND  L   	*/  	/* **X1XP00 */
	case 0246:  		/* AND  (HL)	*/  	/* **X1XP00 */
	case 0247:  		/* AND  A   	*/  	/* **X1XP00 */
		ACC_R = GetRegs(op);
    ANDOP:
		REG_A &= ACC_R;
		SetSf(REG_A & 0x80);
		SetZf(REG_A == 0);
		SetHf(TRUE);
		SetPf(Parity(REG_A));
		SetNf(FALSE);
		SetCf(FALSE);
		break;

	case 0250:  		/* XOR  B   	*/  	/* **X0XP00 */
	case 0251:  		/* XOR  C   	*/  	/* **X0XP00 */
	case 0252:  		/* XOR  D   	*/  	/* **X0XP00 */
	case 0253:  		/* XOR  E   	*/  	/* **X0XP00 */
	case 0254:  		/* XOR  H   	*/  	/* **X0XP00 */
	case 0255:  		/* XOR  L   	*/  	/* **X0XP00 */
	case 0256:  		/* XOR  (HL)	*/  	/* **X0XP00 */
	case 0257:  		/* XOR  A   	*/  	/* **X0XP00 */
		ACC_R = GetRegs(op);
    XOROP:
		REG_A ^= ACC_R;
		SetSf(REG_A & 0x80);
		SetZf(REG_A == 0);
		SetHf(FALSE);
		SetPf(Parity(REG_A));
		SetNf(FALSE);
		SetCf(FALSE);
		break;

	case 0260:  		/* OR   B   	*/  	/* **X0XP00 */
	case 0261:  		/* OR   C   	*/  	/* **X0XP00 */
	case 0262:  		/* OR   D   	*/  	/* **X0XP00 */
	case 0263:  		/* OR   E   	*/  	/* **X0XP00 */
	case 0264:  		/* OR   H   	*/  	/* **X0XP00 */
	case 0265:  		/* OR   L   	*/  	/* **X0XP00 */
	case 0266:  		/* OR   (HL)	*/  	/* **X0XP00 */
	case 0267:  		/* OR   A   	*/  	/* **X0XP00 */
		ACC_R = GetRegs(op);
	OROP:
		REG_A |= ACC_R;
		SetSf(REG_A & 0x80);
		SetZf(REG_A == 0);
		SetHf(FALSE);
		SetPf(Parity(REG_A));
		SetNf(FALSE);
		SetCf(FALSE);
		break;

	case 0270:  		/* CP   B   	*/  	/* **X*XV1* */
	case 0271:  		/* CP   C   	*/  	/* **X*XV1* */
	case 0272:  		/* CP   D   	*/  	/* **X*XV1* */
	case 0273:  		/* CP   E   	*/  	/* **X*XV1* */
	case 0274:  		/* CP   H   	*/  	/* **X*XV1* */
	case 0275:  		/* CP   L   	*/  	/* **X*XV1* */
	case 0276:  		/* CP   (HL)	*/  	/* **X*XV1* */
	case 0277:  		/* CP   A   	*/  	/* **X*XV1* */
		ACC_R = GetRegs(op);
    CPOP:
		ACC_SW = REG_A - ACC_R;
		SetCf(ACC_SW < 0);
		ACC_SW = (signed char)REG_A - (signed char)ACC_R;
		SetVf(ACC_SW < -128 || ACC_SW > 127);
		SetHf((((ACC_B) ^ ((REG_A) ^ (ACC_R))) & 0x10) != 0);
		SetZf(ACC_B == 0);
		SetSf(ACC_B & 0x80);
		SetNf(TRUE);
		break;

	case 0300:  		/* RET  NZ  	*/  	/* --X-X--- */
		if ( !FLG_Z ) {
			REG_PC = GetWord(REG_SP);
			REG_SP += 2;
		}
		break;
	case 0310:  		/* RET  Z   	*/  	/* --X-X--- */
		if ( FLG_Z ) {
			REG_PC = GetWord(REG_SP);
			REG_SP += 2;
		}
		break;
	case 0320:  		/* RET  NC  	*/  	/* --X-X--- */
		if ( !FLG_C ) {
			REG_PC = GetWord(REG_SP);
			REG_SP += 2;
		}
		break;
	case 0330:  		/* RET  C   	*/  	/* --X-X--- */
		if ( FLG_C ) {
			REG_PC = GetWord(REG_SP);
			REG_SP += 2;
		}
		break;
	case 0340:  		/* RET  PO  	*/  	/* --X-X--- */
		if ( !FLG_P ) {
			REG_PC = GetWord(REG_SP);
			REG_SP += 2;
		}
		break;
	case 0350:  		/* RET  PE  	*/  	/* --X-X--- */
		if ( FLG_P ) {
			REG_PC = GetWord(REG_SP);
			REG_SP += 2;
		}
		break;
	case 0360:  		/* RET  P   	*/  	/* --X-X--- */
		if ( !FLG_S ) {
			REG_PC = GetWord(REG_SP);
			REG_SP += 2;
		}
		break;
	case 0370:  		/* RET  M   	*/  	/* --X-X--- */
		if ( FLG_S ) {
			REG_PC = GetWord(REG_SP);
			REG_SP += 2;
		}
		break;

	case 0301:  		/* POP  BC  	*/  	/* --X-X--- */
		REG_BC = GetWord(REG_SP);
		REG_SP += 2;
		break;
	case 0321:  		/* POP  DE  	*/  	/* --X-X--- */
		REG_DE = GetWord(REG_SP);
		REG_SP += 2;
		break;
	case 0341:  		/* POP  HL  	*/  	/* --X-X--- */
		REG_HL = GetWord(REG_SP);
		REG_SP += 2;
		break;
	case 0361:  		/* POP  AF  	*/  	/* --X-X--- */
		REG_AF = GetWord(REG_SP);
		REG_SP += 2;
		LoadFlag();
		break;

	case 0311:  		/* RET  		*/  	/* --X-X--- */
		REG_PC = GetWord(REG_SP);
		REG_SP += 2;
		break;
	case 0331:  		/* EXX  		*/  	/* --X-X--- */
		ACC_W = REG_BC; REG_BC = REG_BCs; REG_BCs = ACC_W;
		ACC_W = REG_DE; REG_DE = REG_DEs; REG_DEs = ACC_W;
		ACC_W = REG_HL; REG_HL = REG_HLs; REG_HLs = ACC_W;
		break;
	case 0351:  		/* JP   (HL)	*/  	/* --X-X--- */
		REG_PC = REG_HL;
		break;
	case 0371:  		/* LD   SP,HL   */  	/* --X-X--- */
		REG_SP = REG_HL;
		break;

	case 0302:  		/* JP   NZ,nn   */  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( !FLG_Z )
			REG_PC = ACC_A;
		break;
	case 0312:  		/* JP   Z,nn	*/  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( FLG_Z )
			REG_PC = ACC_A;
		break;
	case 0322:  		/* JP   NC,nn   */  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( !FLG_C )
			REG_PC = ACC_A;
		break;
	case 0332:  		/* JP   C,nn	*/  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( FLG_C )
			REG_PC = ACC_A;
		break;
	case 0342:  		/* JP   PO,nn   */  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( !FLG_P )
			REG_PC = ACC_A;
		break;
	case 0352:  		/* JP   PE,nn   */  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( FLG_P )
			REG_PC = ACC_A;
		break;
	case 0362:  		/* JP   P,nn	*/  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( !FLG_S )
			REG_PC = ACC_A;
		break;
	case 0372:  		/* JP   M,nn	*/  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( FLG_S )
			REG_PC = ACC_A;
		break;

	case 0303:  		/* JP   nn  	*/  	/* --X-X--- */
		REG_PC = GetOpW();
		break;
	case 0313:  		/* CB ...   	*/  	/* --X-X--- */
		z80emuCB(NULL);
		break;
	case 0323:  		/* OUT  (n),A   */  	/* --X-X--- */
		ACC_R = GetOp();
		SetIO(ACC_R, REG_A);
		break;
	case 0333:  		/* IN   A,(n)   */  	/* --X-X--- */
		ACC_R = GetOp();
		REG_A = GetIO(ACC_R);
		break;
	case 0343:  		/* EX   (SP),HL */  	/* --X-X--- */
		ACC_W = GetWord(REG_SP);
		SetWord(REG_SP, REG_HL);
		REG_HL = ACC_W;
		break;
	case 0353:  		/* EX   DE,HL   */  	/* --X-X--- */
		ACC_W = REG_HL;
		REG_HL = REG_DE;
		REG_DE = ACC_W;
		break;
	case 0363:  		/* DI   		*/  	/* --X-X--- */
		SetIFF1(FALSE);
		SetIFF2(FALSE);
		break;
	case 0373:  		/* EI   		*/  	/* --X-X--- */
		SetIFF1(TRUE);
		SetIFF2(TRUE);
		break;

	case 0304:  		/* CALL NZ,nn   */  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( !FLG_Z ) {
			REG_SP -= 2;
			SetWord(REG_SP, REG_PC);
			REG_PC = ACC_A;
		}
		break;
	case 0314:  		/* CALL Z,nn	*/  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( FLG_Z ) {
			REG_SP -= 2;
			SetWord(REG_SP, REG_PC);
			REG_PC = ACC_A;
		}
		break;
	case 0324:  		/* CALL NC,nn   */  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( !FLG_C ) {
			REG_SP -= 2;
			SetWord(REG_SP, REG_PC);
			REG_PC = ACC_A;
		}
		break;
	case 0334:  		/* CALL C,nn	*/  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( FLG_C ) {
			REG_SP -= 2;
			SetWord(REG_SP, REG_PC);
			REG_PC = ACC_A;
		}
		break;
	case 0344:  		/* CALL PO,nn   */  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( !FLG_P ) {
			REG_SP -= 2;
			SetWord(REG_SP, REG_PC);
			REG_PC = ACC_A;
		}
		break;
	case 0354:  		/* CALL PE,nn   */  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( FLG_P ) {
			REG_SP -= 2;
			SetWord(REG_SP, REG_PC);
			REG_PC = ACC_A;
		}
		break;
	case 0364:  		/* CALL P,nn	*/  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( !FLG_S ) {
			REG_SP -= 2;
			SetWord(REG_SP, REG_PC);
			REG_PC = ACC_A;
		}
		break;
	case 0374:  		/* CALL M,nn	*/  	/* --X-X--- */
		ACC_A = GetOpW();
		if ( FLG_S ) {
			REG_SP -= 2;
			SetWord(REG_SP, REG_PC);
			REG_PC = ACC_A;
		}
		break;

	case 0305:  		/* PUSH BC  	*/  	/* --X-X--- */
		REG_SP -= 2;
		SetWord(REG_SP, REG_BC);
		break;
	case 0325:  		/* PUSH DE  	*/  	/* --X-X--- */
		REG_SP -= 2;
		SetWord(REG_SP, REG_DE);
		break;
	case 0345:  		/* PUSH HL  	*/  	/* --X-X--- */
		REG_SP -= 2;
		SetWord(REG_SP, REG_HL);
		break;
	case 0365:  		/* PUSH AF  	*/  	/* --X-X--- */
		SaveFlag();
		REG_SP -= 2;
		SetWord(REG_SP, REG_AF);
		break;

	case 0315:  		/* CALL nn  	*/  	/* --X-X--- */
		ACC_A = GetOpW();
		REG_SP -= 2;
		SetWord(REG_SP, REG_PC);
		REG_PC = ACC_A;
		break;
	case 0335:  		/* DD ...   	*/  	/* --X-X--- */
		z80emuIndex(&(REG_IX));
		break;
	case 0355:  		/* ED ...   	*/  	/* --X-X--- */
		z80emuED();
		break;
	case 0375:  		/* FD ...   	*/  	/* --X-X--- */
		z80emuIndex(&(REG_IY));
		break;

	case 0306:  		/* ADD  A,n 	*/  	/* **X*XV0* */
		ACC_R = GetOp();
		goto ADDOP;
	case 0316:  		/* ADC  A,n 	*/  	/* **X*XV0* */
		ACC_R = GetOp();
		goto ADCOP;
	case 0326:  		/* SUB  n   	*/  	/* **X*XV1* */
		ACC_R = GetOp();
		goto SUBOP;
	case 0336:  		/* SBC  A,n 	*/  	/* **X*XV1* */
		ACC_R = GetOp();
		goto SBCOP;
	case 0346:  		/* AND  n   	*/  	/* **X1XP00 */
		ACC_R = GetOp();
		goto ANDOP;
	case 0356:  		/* XOR  n   	*/  	/* **X0XP00 */
		ACC_R = GetOp();
		goto XOROP;
	case 0366:  		/* OR   n   	*/  	/* **X0XP00 */
		ACC_R = GetOp();
		goto OROP;
	case 0376:  		/* CP   n   	*/  	/* **X*XV1* */
		ACC_R = GetOp();
		goto CPOP;

	case 0307:  		/* RST  0   	*/  	/* --X-X--- */
	case 0317:  		/* RST  1   	*/  	/* --X-X--- */
	case 0327:  		/* RST  2   	*/  	/* --X-X--- */
	case 0337:  		/* RST  3   	*/  	/* --X-X--- */
	case 0347:  		/* RST  4   	*/  	/* --X-X--- */
	case 0357:  		/* RST  5   	*/  	/* --X-X--- */
	case 0367:  		/* RST  6   	*/  	/* --X-X--- */
	case 0377:  		/* RST  7   	*/  	/* --X-X--- */
		REG_SP -= 2;
		SetWord(REG_SP, REG_PC);
		REG_PC = ((op >> 3) & 7) * 8;
		break;
	}

}
