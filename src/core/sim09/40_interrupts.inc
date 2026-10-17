void	CSim09::SimRESET()
{
	memset(&regs, 0, sizeof(REGS));
	REG_PC = GetMemWord(0xFFFE);
}
void	CSim09::SimFIRQ()
{
    if ( (REG_CC & CC_F) != 0 )
		return;
    if ( (REG_ST & ST_W) == 0 ) {
		REG_CC &= ~CC_E;
		SetMemByte(--REG_S, REG_PCl);
		SetMemByte(--REG_S, REG_PCh);
		SetMemByte(--REG_S, REG_CC);
    } else
		REG_ST &= ~ST_W;
    REG_CC |= (CC_F | CC_I);
    REG_PC = GetMemWord(0xFFF6);
}
void	CSim09::SimIRQ()
{
    if ( (REG_CC & CC_I) != 0 )
		return;
    if ( (REG_ST & ST_W) == 0 ) {
    	REG_CC |= CC_E;
    	PushAll();
    } else
		REG_ST &= ~ST_W;
    REG_CC |= CC_I;
    REG_PC = GetMemWord(0xFFF8);
}
void	CSim09::SimNMI()
{
    if ( (REG_ST & ST_N) != 0 )
		return;
    if ( (REG_ST & ST_W) == 0 ) {
		REG_CC |= CC_E;
		PushAll();
    } else
		REG_ST &= ~ST_W;
    REG_CC |= (CC_F | CC_I);
    REG_ST |= ST_N;
    REG_PC = GetMemWord(0xFFFC);
}
