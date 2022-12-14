/********************************************************************************/
/*                                                                              */
/* CZ80 opcode include source file                                              */
/* C Z80 emulator version 0.92                                                  */
/* Copyright 2004-2005 St?phane Dallongeville                                   */
/*                                                                              */
/********************************************************************************/

#if CZ80_USE_JUMPTABLE
    goto *JumpTable[Opcode];
#else
switch (Opcode)
{
#endif

    // 8 BITS LOAD
    
    OP(0x00):   // NOP

    OP(0x40):   // LD   B,B
    OP(0x49):   // LD   C,C
    OP(0x52):   // LD   D,D
    OP(0x5b):   // LD   E,E
    OP(0x64):   // LD   H,H
    OP(0x6d):   // LD   L,L
    OP(0x7f):   // LD   A,A
OP_NOP:
        RET(4)

    OP(0x41):   // LD   B,C
    OP(0x42):   // LD   B,D
    OP(0x43):   // LD   B,E
    OP(0x44):   // LD   B,H
    OP(0x45):   // LD   B,L
    OP(0x47):   // LD   B,A

    OP(0x48):   // LD   C,B
    OP(0x4a):   // LD   C,D
    OP(0x4b):   // LD   C,E
    OP(0x4c):   // LD   C,H
    OP(0x4d):   // LD   C,L
    OP(0x4f):   // LD   C,A

    OP(0x50):   // LD   D,B
    OP(0x51):   // LD   D,C
    OP(0x53):   // LD   D,E
    OP(0x54):   // LD   D,H
    OP(0x55):   // LD   D,L
    OP(0x57):   // LD   D,A

    OP(0x58):   // LD   E,B
    OP(0x59):   // LD   E,C
    OP(0x5a):   // LD   E,D
    OP(0x5c):   // LD   E,H
    OP(0x5d):   // LD   E,L
    OP(0x5f):   // LD   E,A

    OP(0x60):   // LD   H,B
    OP(0x61):   // LD   H,C
    OP(0x62):   // LD   H,D
    OP(0x63):   // LD   H,E
    OP(0x65):   // LD   H,L
    OP(0x67):   // LD   H,A

    OP(0x68):   // LD   L,B
    OP(0x69):   // LD   L,C
    OP(0x6a):   // LD   L,D
    OP(0x6b):   // LD   L,E
    OP(0x6c):   // LD   L,H
    OP(0x6f):   // LD   L,A

    OP(0x78):   // LD   A,B
    OP(0x79):   // LD   A,C
    OP(0x7a):   // LD   A,D
    OP(0x7b):   // LD   A,E
    OP(0x7c):   // LD   A,H
    OP(0x7d):   // LD   A,L
OP_LD_R_R:
        zR8((Opcode >> 3) & 7) = zR8(Opcode & 7);
        RET(4)

    OP(0x06):   // LD   B,#imm
    OP(0x0e):   // LD   C,#imm
    OP(0x16):   // LD   D,#imm
    OP(0x1e):   // LD   E,#imm
    OP(0x26):   // LD   H,#imm
    OP(0x2e):   // LD   L,#imm
    OP(0x3e):   // LD   A,#imm
OP_LD_R_imm:
	FETCH_BYTE(zR8(Opcode >> 3));
        RET(7)

    OP(0x46):   // LD   B,(HL)
    OP(0x4e):   // LD   C,(HL)
    OP(0x56):   // LD   D,(HL)
    OP(0x5e):   // LD   E,(HL)
    OP(0x66):   // LD   H,(HL)
    OP(0x6e):   // LD   L,(HL)
    OP(0x7e):   // LD   A,(HL)
OP_LD_R_mHL:
        PRE_IO
        READ_BYTE(zHL, zR8((Opcode >> 3) & 7))
        POST_IO
        RET(7)

    OP(0x70):   // LD   (HL),B
    OP(0x71):   // LD   (HL),C
    OP(0x72):   // LD   (HL),D
    OP(0x73):   // LD   (HL),E
    OP(0x74):   // LD   (HL),H
    OP(0x75):   // LD   (HL),L
    OP(0x77):   // LD   (HL),A
OP_LD_mHL_R:
        PRE_IO
        WRITE_BYTE(zHL, zR8(Opcode & 7))
        POST_IO
        RET(7)

    OP(0x36):   // LD (HL), #imm
OP_LD_mHL_imm:
        PRE_IO
	{ u8 t; FETCH_BYTE(t); WRITE_BYTE(zHL,t); }
        POST_IO
        RET(10)

    {
        u32 adr;
    
    OP(0x0a):   // LD   A,(BC)
OP_LOAD_A_mBC:
        adr = zBC;
        goto OP_LOAD_A_mxx;
        
    OP(0x1a):   // LD   A,(DE)
OP_LOAD_A_mDE:
        adr = zDE;

OP_LOAD_A_mxx:
        PRE_IO
        READ_BYTE(adr, zA)
        POST_IO
        RET(7)

    OP(0x3a):   // LD   A,(nn)
OP_LOAD_A_mNN:
        PRE_IO
        FETCH_WORD(adr);
        READ_BYTE(adr, zA)
        POST_IO
        RET(13)

    OP(0x02):   // LD   (BC),A
OP_LOAD_mBC_A:
        adr = zBC;
        goto OP_LOAD_mxx_A;

    OP(0x12):   // LD   (DE),A
OP_LOAD_mDE_A:
        adr = zDE;

OP_LOAD_mxx_A:
        PRE_IO
        WRITE_BYTE(adr, zA)
        POST_IO
        RET(7)

    OP(0x32):   // LD   (nn),A
OP_LOAD_mNN_A:
        PRE_IO
        FETCH_WORD(adr);
        WRITE_BYTE(adr, zA)
        POST_IO
        RET(13)
    }

    // 16 BITS LOAD

    OP(0x01):   // LD   BC,nn
    OP(0x11):   // LD   DE,nn
    OP(0x21):   // LD   HL,nn
OP_LOAD_RR_imm16:
        FETCH_WORD(zR16(Opcode >> 4));
        RET(10)

    OP(0x31):   // LD   SP,nn
OP_LOAD_SP_imm16:
        FETCH_WORD(zSP);
        RET(10)

    OP(0xf9):   // LD   SP,HL
OP_LD_SP_xx:
        zSP = data->W;
        RET(6)

    {
        u32 adr;

    OP(0x2a):   // LD   HL,(nn)
OP_LD_xx_mNN:
        PRE_IO
        FETCH_WORD(adr);
        READ_WORD(adr, data->W)
        POST_IO
        RET(16)

    OP(0x22):   // LD   (nn),HL
OP_LD_mNN_xx:
        PRE_IO
        FETCH_WORD(adr);
        WRITE_WORD(adr, data->W)
        POST_IO
        RET(16)
    }

    // PUSH / POP

    OP(0xf1):   // POP  AF
OP_POP_AF:
    {
        u32 res;

        PRE_IO
        POP_16(res)
        zA = res >> 8;
        zF = res & 0xFF;
        POST_IO
        RET(10)
    }

    OP(0xc1):   // POP  BC
    OP(0xd1):   // POP  DE
OP_POP_RR:
        data = pzR16((Opcode >> 4) & 3);

    OP(0xe1):   // POP  HL
OP_POP:
        PRE_IO
        POP_16(data->W)
        POST_IO
        RET(10)

    OP(0xf5):   // PUSH AF
OP_PUSH_AF:
        PRE_IO
        PUSH_16((zA << 8) | zF);
        POST_IO
        RET(11)

    OP(0xc5):   // PUSH BC
    OP(0xd5):   // PUSH DE
OP_PUSH_RR:
        data = pzR16((Opcode >> 4) & 3);

    OP(0xe5):   // PUSH HL
OP_PUSH:
        PRE_IO
        PUSH_16(data->W);
        POST_IO
        RET(11)

    // EXCHANGE & BLOCK TRANSFERT / SEARCH

    {
        u32 tmp;

    OP(0x08):   // EX   AF,AF'
OP_EX_AF_AF2:
        tmp = zFA;
        zFA = zFA2;
        zFA2 = tmp;
        RET(4)

    OP(0xeb):   // EX   DE,HL
OP_EX_DE_HL:
        tmp = zDE;
        zDE = zHL;
        zHL = tmp;
        RET(4)

    OP(0xd9):   // EXX
OP_EXX:
        tmp = zBC;
        zBC = zBC2;
        zBC2 = tmp;
        tmp = zDE;
        zDE = zDE2;
        zDE2 = tmp;
        tmp = zHL;
        zHL = zHL2;
        zHL2 = tmp;
        RET(4)
    }

    OP(0xe3):   // EX   HL,(SP)
    {
        u32 adr;
        u32 tmp;
        
OP_EX_xx_mSP:
        PRE_IO
        adr = zSP;
        tmp = data->W;
        READ_WORD(adr, data->W)
        WRITE_WORD(adr, tmp)
        POST_IO
        RET(19)
    }

    // 8 BITS ARITHMETIC

    OP(0x04):   // INC  B
    OP(0x0c):   // INC  C
    OP(0x14):   // INC  D
    OP(0x1c):   // INC  E
    OP(0x24):   // INC  H
    OP(0x2c):   // INC  L
    OP(0x3c):   // INC  A
OP_INC_R:
        zR8(Opcode >> 3)++;
        zF = (zF & CZ80_CF) | SZXYHV_inc[zR8(Opcode >> 3)];
        RET(4)

    {
        u32 adr;
        u32 res;
        
OP_INC_mIx:
	{ s8 t; FETCH_BYTE_S(t); adr = data->W + t; }
        Z80_ICount -= 11;
        goto OP_INC_m;

    OP(0x34):   // INC  (HL)
        adr = zHL;
        
OP_INC_m:
        PRE_IO
        READ_BYTE(adr, res)
        res = (res + 1) & 0xFF;
        WRITE_BYTE(adr, res)
        zF = (zF & CZ80_CF) | SZXYHV_inc[res];
        POST_IO
        RET(11)
    }

    OP(0x05):   // DEC  B
    OP(0x0d):   // DEC  C
    OP(0x15):   // DEC  D
    OP(0x1d):   // DEC  E
    OP(0x25):   // DEC  H
    OP(0x2d):   // DEC  L
    OP(0x3d):   // DEC  A
OP_DEC_R:
        zR8(Opcode >> 3)--;
        zF = (zF & CZ80_CF) | SZXYHV_dec[zR8(Opcode >> 3)];
        RET(4)

    {
        u32 adr;
        u32 res;

OP_DEC_mIx:
	{ s8 t; FETCH_BYTE_S(t); adr = data->W + t; }
        Z80_ICount -= 11;
        goto OP_DEC_m;

    OP(0x35):   // DEC  (HL)
        adr = zHL;

OP_DEC_m:
        PRE_IO
        READ_BYTE(adr, res)
        res = (res - 1) & 0xFF;
        WRITE_BYTE(adr, res)
        zF = (zF & CZ80_CF) | SZXYHV_dec[res];
        POST_IO
        RET(11)
    }

    {
        u32 val;
        u32 res;

    // ADD

OP_ADD_mIx:
        PRE_IO
	{ s8 t; FETCH_BYTE_S(t); READ_BYTE(data->W + t, val); }
        POST_IO
        Z80_ICount -= 11;
        goto OP_ADD;

    OP(0xc6):   // ADD  A,n
OP_ADD_imm:
	FETCH_BYTE(val);
        Z80_ICount -= 3;
        goto OP_ADD;

    OP(0x86):   // ADD  A,(HL)
OP_ADD_mHL:
        PRE_IO
        READ_BYTE(zHL, val)
        POST_IO
        Z80_ICount -= 3;
        goto OP_ADD;

OP_ADD_IxH:
        val = data->B.H;
        goto OP_ADD;

OP_ADD_IxL:
        val = data->B.L;
        goto OP_ADD;

    OP(0x80):   // ADD  A,B
    OP(0x81):   // ADD  A,C
    OP(0x82):   // ADD  A,D
    OP(0x83):   // ADD  A,E
    OP(0x84):   // ADD  A,H
    OP(0x85):   // ADD  A,L
    OP(0x87):   // ADD  A,A
OP_ADD_R:
        val = zR8(Opcode & 7);

OP_ADD:
// bench : maybe use src instead of zA in zF calculation
        res = zA + val;
        zF = SZXY[res & 0xFF] |                                 // S/Z/X/Y flag
            ((zA ^ res ^ val) & CZ80_HF) |                      // H flag
            (((val ^ zA ^ 0x80) & (val ^ res) & 0x80) >> 5) |   // V flag
            ((res >> 8) & CZ80_CF);                             // C flag
        zA = res;
        RET(4)

    // ADC

OP_ADC_mIx:
        PRE_IO
	{ s8 t; FETCH_BYTE_S(t); READ_BYTE(data->W + t, val); }
        POST_IO
        Z80_ICount -= 11;
        goto OP_ADC;

    OP(0xce):   // ADC  A,n
OP_ADC_imm:
	FETCH_BYTE(val);
        Z80_ICount -= 3;
        goto OP_ADC;

    OP(0x8e):   // ADC  A,(HL)
OP_ADC_mHL:
        PRE_IO
        READ_BYTE(zHL, val)
        POST_IO
        Z80_ICount -= 3;
        goto OP_ADC;

OP_ADC_IxH:
        val = data->B.H;
        goto OP_ADC;

OP_ADC_IxL:
        val = data->B.L;
        goto OP_ADC;

    OP(0x88):   // ADC  A,B
    OP(0x89):   // ADC  A,C
    OP(0x8a):   // ADC  A,D
    OP(0x8b):   // ADC  A,E
    OP(0x8c):   // ADC  A,H
    OP(0x8d):   // ADC  A,L
    OP(0x8f):   // ADC  A,A
OP_ADC_R:
        val = zR8(Opcode & 7);

OP_ADC:
// bench : maybe use src instead of zA in zF calculation
        res = (zA + val) + (zF & CZ80_CF);
        zF = SZXY[res & 0xFF] |                                 // S/Z/X/Y flag
            ((zA ^ res ^ val) & CZ80_HF) |                      // H flag
            (((val ^ zA ^ 0x80) & (val ^ res) & 0x80) >> 5) |   // V flag
            ((res >> 8) & CZ80_CF);                             // C flag
        zA = res;
        RET(4)

    // SUB

OP_SUB_mIx:
        PRE_IO
	{ s8 t; FETCH_BYTE_S(t); READ_BYTE(data->W + t, val); }
        POST_IO
        Z80_ICount -= 11;
        goto OP_SUB;

    OP(0xd6):   // SUB  A,n
OP_SUB_imm:
	FETCH_BYTE(val);
        Z80_ICount -= 3;
        goto OP_SUB;

    OP(0x96):   // SUB  (HL)
OP_SUB_mHL:
        PRE_IO
        READ_BYTE(zHL, val)
        POST_IO
        Z80_ICount -= 3;
        goto OP_SUB;

OP_SUB_IxH:
        val = data->B.H;
        goto OP_SUB;

OP_SUB_IxL:
        val = data->B.L;
        goto OP_SUB;

    OP(0x90):   // SUB  B
    OP(0x91):   // SUB  C
    OP(0x92):   // SUB  D
    OP(0x93):   // SUB  E
    OP(0x94):   // SUB  H
    OP(0x95):   // SUB  L
    OP(0x97):   // SUB  A
OP_SUB_R:
        val = zR8(Opcode & 7);

OP_SUB:
// bench : maybe use src instead of zA in zF calculation
        res = zA - val;
        zF = SZXY[res & 0xFF] |                             // S/Z/X/Y flag
            ((zA ^ res ^ val) & CZ80_HF) |                  // H flag
            (((val ^ zA) & (zA ^ res) & 0x80) >> 5) |       // V flag
            ((res >> 8) & CZ80_CF) | CZ80_NF;               // C/N flag
        zA = res;
        RET(4)

    // SBC

OP_SBC_mIx:
        PRE_IO
	{ s8 t; FETCH_BYTE_S(t); READ_BYTE(data->W + t, val); }
        POST_IO
        Z80_ICount -= 11;
        goto OP_SBC;

    OP(0xde):   // SBC  A,n
OP_SBC_imm:
	FETCH_BYTE(val);
        Z80_ICount -= 3;
        goto OP_SBC;

    OP(0x9e):   // SBC  A,(HL)
OP_SBC_mHL:
        PRE_IO
        READ_BYTE(zHL, val)
        POST_IO
        Z80_ICount -= 3;
        goto OP_SBC;

OP_SBC_IxH:
        val = data->B.H;
        goto OP_SBC;

OP_SBC_IxL:
        val = data->B.L;
        goto OP_SBC;

    OP(0x98):   // SBC  A,B
    OP(0x99):   // SBC  A,C
    OP(0x9a):   // SBC  A,D
    OP(0x9b):   // SBC  A,E
    OP(0x9c):   // SBC  A,H
    OP(0x9d):   // SBC  A,L
    OP(0x9f):   // SBC  A,A
OP_SBC_R:
        val = zR8(Opcode & 7);

OP_SBC:
// bench : maybe use src instead of zA in zF calculation
        res = zA - (val + (zF & CZ80_CF));
        zF = SZXY[res & 0xFF] |                             // S/Z/X/Y flag
            ((zA ^ res ^ val) & CZ80_HF) |                  // H flag
            (((val ^ zA) & (zA ^ res) & 0x80) >> 5) |       // V flag
            ((res >> 8) & CZ80_CF) | CZ80_NF;               // C/N flag
        zA = res;
        RET(4)

    // CP

OP_CP_mIx:
        PRE_IO
	{ s8 t; FETCH_BYTE_S(t); READ_BYTE(data->W + t, val); }
        POST_IO
        Z80_ICount -= 11;
        goto OP_CP;

    OP(0xfe):   // CP   n
OP_CP_imm:
        FETCH_BYTE(val);
        Z80_ICount -= 3;
        goto OP_CP;

    OP(0xbe):   // CP   (HL)
OP_CP_mHL:
        PRE_IO
        READ_BYTE(zHL, val)
        POST_IO
        Z80_ICount -= 3;
        goto OP_CP;

OP_CP_IxH:
        val = data->B.H;
        goto OP_CP;

OP_CP_IxL:
        val = data->B.L;
        goto OP_CP;

    OP(0xb8):   // CP   B
    OP(0xb9):   // CP   C
    OP(0xba):   // CP   D
    OP(0xbb):   // CP   E
    OP(0xbc):   // CP   H
    OP(0xbd):   // CP   L
    OP(0xbf):   // CP   A
OP_CP_R:
        val = zR8(Opcode & 7);

OP_CP:
// bench : maybe use src instead of zA in zF calculation
        res = zA - val;
#if CZ80_DEBUG
        zF = SZXY[res & 0xFF] |                             // S/Z/X/Y flag
            ((zA ^ res ^ val) & CZ80_HF) |                  // H flag
            (((val ^ zA) & (zA ^ res) & 0x80) >> 5) |       // V flag
            ((res >> 8) & CZ80_CF) | CZ80_NF;               // C/N flag
#else
        zF = (SZXY[res & 0xFF] & ~(CZ80_XF | CZ80_YF)) |    // S/Z flag
            (val & (CZ80_XF | CZ80_YF)) |                   // X/Y flag
            ((zA ^ res ^ val) & CZ80_HF) |                  // H flag
            (((val ^ zA) & (zA ^ res) & 0x80) >> 5) |       // V flag
            ((res >> 8) & CZ80_CF) | CZ80_NF;               // C/N flag
#endif
        RET(4)
    }
    
    // AND

    {
        u32 val;

    OP(0xa6):   // AND  (HL)
OP_AND_mHL:
        PRE_IO
        READ_BYTE(zHL, val)
        POST_IO
        goto OP_AND_;

    OP(0xe6):   // AND  A,n
OP_AND_imm:
        FETCH_BYTE(val);

OP_AND_:
        zA = zA & val;
        zF = SZXYP[zA] | CZ80_HF;
        RET(7)

OP_AND_IxL:
        val = data->B.L;
        goto OP_AND;

OP_AND_IxH:
        val = data->B.H;
        goto OP_AND;

    OP(0xa0):   // AND  B
    OP(0xa1):   // AND  C
    OP(0xa2):   // AND  D
    OP(0xa3):   // AND  E
    OP(0xa4):   // AND  H
    OP(0xa5):   // AND  L
OP_AND_R:
        val = zR8(Opcode & 7);

OP_AND:
        zA = zA & val;
        
    OP(0xa7):   // AND  A
OP_AND_A:
        zF = SZXYP[zA] | CZ80_HF;
        RET(4)

    // XOR

    OP(0xae):   // XOR  (HL)
OP_XOR_mHL:
        PRE_IO
        READ_BYTE(zHL, val)
        POST_IO
        goto OP_XOR_;

    OP(0xee):   // XOR  A,n
OP_XOR_imm:
        FETCH_BYTE(val);
        
OP_XOR_:
        zA = zA ^ val;
        zF = SZXYP[zA];
        RET(7)

OP_XOR_IxL:
        val = data->B.L;
        goto OP_XOR;

OP_XOR_IxH:
        val = data->B.H;
        goto OP_XOR;

    OP(0xa8):   // XOR  B
    OP(0xa9):   // XOR  C
    OP(0xaa):   // XOR  D
    OP(0xab):   // XOR  E
    OP(0xac):   // XOR  H
    OP(0xad):   // XOR  L
OP_XOR_R:
        val = zR8(Opcode & 7);

OP_XOR:
        zA = zA ^ val;
        zF = SZXYP[zA];
        RET(4)

    OP(0xaf):   // XOR  A
OP_XOR_A:
        zA = 0;
        zF = SZXYP[zA];
        RET(4)
        
    // OR

    OP(0xb6):   // OR   (HL)
OP_OR_mHL:
        PRE_IO
        READ_BYTE(zHL, val)
        POST_IO
        goto OP_OR_;
    
    OP(0xf6):   // OR   A,n
OP_OR_imm:
        FETCH_BYTE(val);

OP_OR_:
        zA = zA | val;
        zF = SZXYP[zA];
        RET(7)

OP_OR_IxL:
        val = data->B.L;
        goto OP_OR;

OP_OR_IxH:
        val = data->B.H;
        goto OP_OR;

    OP(0xb0):   // OR   B
    OP(0xb1):   // OR   C
    OP(0xb2):   // OR   D
    OP(0xb3):   // OR   E
    OP(0xb4):   // OR   H
    OP(0xb5):   // OR   L
OP_OR_R:
        val = zR8(Opcode & 7);

OP_OR:
        zA = zA | val;

    OP(0xb7):   // OR   A
OP_OR_A:
        zF = SZXYP[zA];
        RET(4)
    }


    // MISC ARITHMETIC & CPU CONTROL

    OP(0x27):   // DAA
OP_DAA:
    {
        u8 _F;
        u8 cf, nf, hf, lo, hi, diff;
        
        _F = zF;
        cf = _F & CZ80_CF;
        nf = _F & CZ80_NF;
        hf = _F & CZ80_HF;
        lo = zA & 0x0F;
        hi = zA >> 4;

        if (cf)
        {
        	diff = (lo <= 9 && !hf) ? 0x60 : 0x66;
        }
        else
        {
        	if (lo >= 10)
        	{
        		diff = hi <= 8 ? 0x06 : 0x66;
        	}
        	else
        	{
        		if (hi >= 10)
        		{
        			diff = hf ? 0x66 : 0x60;
        		}
        		else
        		{
        			diff = hf ? 0x06 : 0x00;
        		}
        	}
        }
        if (nf) zA -= diff;
        else zA += diff;

        _F = SZXYP[zA] | (_F & CZ80_NF);
        if (cf || (lo <= 9 ? hi >= 10 : hi >= 9)) _F |= CZ80_CF;
        if (nf ? hf && lo <= 5 : lo >= 10) _F |= CZ80_HF;
        zF = _F;
        RET(4)
    }

    OP(0x2f):   // CPL
OP_CPL:
        zA ^= 0xFF;
        zF = (zF & (CZ80_SF | CZ80_ZF | CZ80_PF | CZ80_CF)) |
            CZ80_HF | CZ80_NF |
            (zA & (CZ80_XF | CZ80_YF));
        RET(4)

    OP(0x37):   // SCF
OP_SCF:
        zF = (zF & (CZ80_SF | CZ80_ZF | CZ80_PF)) |
            (zA & (CZ80_XF | CZ80_YF)) |
            CZ80_CF;
        RET(4)

    OP(0x3f):   // CCF
OP_CCF:
        zF = ((zF & (CZ80_SF | CZ80_ZF | CZ80_PF | CZ80_CF)) |
            ((zF & CZ80_CF) << 4) |
            (zA & (CZ80_XF | CZ80_YF))) ^
            CZ80_CF;
        RET(4)

    OP(0x76):   // HALT
OP_HALT:
        // HALTED state
        CPU->Status |= CZ80_HALTED;
        // release remaining cycles...
        Z80_ICount = 0;
        goto Cz80_Exec_Really_End;

    OP(0xf3):   // DI
OP_DI:
        zIFF = 0;
#if CZ80_DEBUG
        RET(4)
#else
        Z80_ICount -= 4;
        // can't take interrupt after DI so we force next instruction execution
        goto Cz80_Exec;
#endif

    OP(0xfb):   // EI
    OP_EI:
        zIFF = CZ80_IFF | (CZ80_IFF << 8);
#if CZ80_DEBUG
        RET(4)
#else
        // release remaining cycles...
        CPU->CycleSup += Z80_ICount - 4;
        Z80_ICount = 0;
        // can't take interrupt after EI so we force next instruction execution
        goto Cz80_Exec;
#endif

    // 16 BITS ARITHMETIC

    OP(0x03):   // INC  BC
OP_INC_BC:
        zBC++;
        RET(6)
        
    OP(0x13):   // INC  DE
OP_INC_DE:
        zDE++;
        RET(6)
        
    OP(0x23):   // INC  HL
OP_INC_xx:
        data->W++;
        RET(6)

    OP(0x33):   // INC  SP
OP_INC_SP:
        zSP++;
        RET(6)

    OP(0x0b):   // DEC  BC
OP_DEC_BC:
        zBC--;
        RET(6)

    OP(0x1b):   // DEC  DE
OP_DEC_DE:
        zDE--;
        RET(6)

    OP(0x2b):   // DEC  HL
OP_DEC_xx:
        data->W--;
        RET(6)

    OP(0x3b):   // DEC  SP
OP_DEC_SP:
        zSP--;
        RET(6)

    // ADD16

    {
        u32 src;
        u32 res;

    OP(0x39):   // ADD  xx,SP
OP_ADD16_xx_SP:
        src = zSP;
        goto OP_ADD16;

    OP(0x29):   // ADD  xx,xx
OP_ADD16_xx_xx:
        src = data->W;
        goto OP_ADD16;

    OP(0x09):   // ADD  xx,BC
OP_ADD16_xx_BC:
        src = zBC;
        goto OP_ADD16;

    OP(0x19):   // ADD  xx,DE
OP_ADD16_xx_DE:
        src = zDE;

OP_ADD16:
        res = src + data->W;
#if CZ80_DEBUG
        zF = (zF & (CZ80_SF | CZ80_ZF | CZ80_VF)) |     // S/Z/V flag
            (((src ^ data->W ^ res) >> 8) & CZ80_HF) |  // H flag
            ((res >> 16) & CZ80_CF);                    // C flag
#else
        zF = (zF & (CZ80_SF | CZ80_ZF | CZ80_VF)) |     // S/Z/V flag
            (((src ^ data->W ^ res) >> 8) & CZ80_HF) |  // H flag
            ((res >> 8) & (CZ80_XF | CZ80_YF)) |        // X/Y flag
            ((res >> 16) & CZ80_CF);                    // C flag
#endif
        data->W = res;
        RET(11)
    }

    // ROTATE

    {
        u8 A;
        u8 F;
        
    OP(0x07):   // RLCA
OP_RLCA:
        A = zA;
        zA = (A << 1) | (A >> 7);
        zF = (zF & (CZ80_SF | CZ80_ZF | CZ80_PF)) |     // S/Z/P flag
            (zA & (CZ80_XF | CZ80_YF | CZ80_CF));       // X/Y/C
        RET(4)

    OP(0x0f):   // RRCA
OP_RRCA:
        A = zA;
        zA = (A >> 1) | (A << 7);
        zF = (zF & (CZ80_SF | CZ80_ZF | CZ80_PF)) |     // S/Z/P flag
            (zA & (CZ80_XF | CZ80_YF)) |                // X/Y flag
            (A & CZ80_CF);                              // C flag
        RET(4)

    OP(0x17):   // RLA
OP_RLA:
        A = zA;
        F = zF;
        zA = (A << 1) | (F & CZ80_CF);
        zF = (F & (CZ80_SF | CZ80_ZF | CZ80_PF)) |      // S/Z/P flag
            (zA & (CZ80_XF | CZ80_YF)) |                // X/Y flag
            (A >> 7);                                   // C flag
        RET(4)

    OP(0x1f):   // RRA
OP_RRA:
        A = zA;
        F = zF;
        zA = (A >> 1) | (F << 7);
        zF = (F & (CZ80_SF | CZ80_ZF | CZ80_PF)) |      // S/Z/P flag
            (zA & (CZ80_XF | CZ80_YF)) |                // X/Y flag
            (A & CZ80_CF);                              // C flag
        RET(4)
    }

    // JUMP
    {
        u32 newPC;

    OP(0xd2):   // JP   NC,nn
OP_JP_NC:
        if (!(zF & CZ80_CF)) goto OP_JP;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10);

    OP(0xda):   // JP   C,nn
OP_JP_C:
        if (zF & CZ80_CF) goto OP_JP;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10);

    OP(0xe2):   // JP   PO,nn
OP_JP_PO:
        if (!(zF & CZ80_VF)) goto OP_JP;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10);

    OP(0xea):   // JP   PE,nn
OP_JP_PE:
        if (zF & CZ80_VF) goto OP_JP;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10);

    OP(0xf2):   // JP   P,nn
OP_JP_P:
        if (!(zF & CZ80_SF)) goto OP_JP;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10);

    OP(0xfa):   // JP   M,nn
OP_JP_M:
        if (zF & CZ80_SF) goto OP_JP;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10);

    OP(0xca):   // JP   Z,nn
OP_JP_Z:
        if (zF & CZ80_ZF) goto OP_JP;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10);

    OP(0xc2):   // JP   NZ,nn
OP_JP_NZ:
        if (!(zF & CZ80_ZF)) goto OP_JP;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10);


    OP(0xc3):   // JP   nn
OP_JP:
	FETCH_WORD(newPC);
        SET_PC(newPC);
        RET(10)

    OP(0xe9):   // JP   (xx)
OP_JP_xx:
        newPC = data->W;
        SET_PC(newPC);
        RET(4)

    
    OP(0x38):   // JR   C,n
OP_JR_C:
        if (zF & CZ80_CF) goto OP_JR;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+1;
	SET_PC(newPC);
        RET(7)

    OP(0x30):   // JR   NC,n
OP_JR_NC:
        if (!(zF & CZ80_CF)) goto OP_JR;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+1;
	SET_PC(newPC);
        RET(7)

    OP(0x28):   // JR   Z,n
OP_JR_Z:
        if (zF & CZ80_ZF) goto OP_JR;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+1;
	SET_PC(newPC);
        RET(7)

    OP(0x20):   // JR   NZ,n
OP_JR_NZ:
        if (!(zF & CZ80_ZF)) goto OP_JR;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+1;
	SET_PC(newPC);
        RET(7)

    OP(0x10):   // DJNZ n
OP_DJNZ:
        Z80_ICount--;
        if (--zB) goto OP_JR;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+1;
	SET_PC(newPC);
        RET(9)

    OP(0x18):   // JR   n
OP_JR:
    {
        u32 adr;

        FETCH_BYTE_S(adr);
        // no rebase needed here...
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+adr;
	SET_PC(newPC);
        RET(12)
    }

    // CALL & RETURN

    OP(0xd4):   // CALL NC,nn
OP_CALL_NC:
        if (!(zF & CZ80_CF)) goto OP_CALL;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10)

    OP(0xdc):   // CALL C,nn
OP_CALL_C:
        if (zF & CZ80_CF) goto OP_CALL;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10)

    OP(0xe4):   // CALL PO,nn
OP_CALL_PO:
        if (!(zF & CZ80_VF)) goto OP_CALL;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10)

    OP(0xec):   // CALL PE,nn
OP_CALL_PE:
        if (zF & CZ80_VF) goto OP_CALL;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10)

    OP(0xf4):   // CALL P,nn
OP_CALL_P:
        if (!(zF & CZ80_SF)) goto OP_CALL;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10)

    OP(0xfc):   // CALL M,nn
OP_CALL_M:
        if (zF & CZ80_SF) goto OP_CALL;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10)

    OP(0xcc):   // CALL Z,nn
OP_CALL_Z:
        if (zF & CZ80_ZF) goto OP_CALL;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10)

    OP(0xc4):   // CALL NZ,nn
OP_CALL_NZ:
        if (!(zF & CZ80_ZF)) goto OP_CALL;
	newPC=(((unsigned)zPC)-((unsigned)mame4all_cz80_rom))+2;
	SET_PC(newPC);
        RET(10)
    }

    OP(0xcd):   // CALL nn
OP_CALL:
    {
        u32 oldRPC;
        u32 newPC;

        PRE_IO
        FETCH_WORD(newPC);
        oldRPC = zRealPC;
        PUSH_16(oldRPC);
        SET_PC(newPC);
        POST_IO
        RET(17)
    }

    OP(0xd0):   // RET  NC
OP_RET_NC:
        if (!(zF & CZ80_CF)) goto OP_RET_COND;
        RET(5)

    OP(0xd8):   // RET  C
OP_RET_C:
        if (zF & CZ80_CF) goto OP_RET_COND;
        RET(5)

    OP(0xe0):   // RET  PO
OP_RET_PO:
        if (!(zF & CZ80_VF)) goto OP_RET_COND;
        RET(5)

    OP(0xe8):   // RET  PE
OP_RET_PE:
        if (zF & CZ80_VF) goto OP_RET_COND;
        RET(5)

    OP(0xf0):   // RET  P
OP_RET_P:
        if (!(zF & CZ80_SF)) goto OP_RET_COND;
        RET(5)

    OP(0xf8):   // RET  M
OP_RET_M:
        if (zF & CZ80_SF) goto OP_RET_COND;
        RET(5)

    OP(0xc0):   // RET  NZ
OP_RET_NZ:
        if (!(zF & CZ80_ZF)) goto OP_RET_COND;
        RET(5)

    OP(0xc8):   // RET  Z
OP_RET_Z:
        if (zF & CZ80_ZF) goto OP_RET_COND;
        RET(5)

OP_RET_COND:
        Z80_ICount -= 7;

    OP(0xc9):   // RET
OP_RET:
    {
        u32 newPC;

        PRE_IO
        POP_16(newPC);
        SET_PC(newPC);
        POST_IO
        RET(10)
    }


    OP(0xc7):   // RST  0
    OP(0xcf):   // RST  1
    OP(0xd7):   // RST  2
    OP(0xdf):   // RST  3
    OP(0xe7):   // RST  4
    OP(0xef):   // RST  5
    OP(0xf7):   // RST  6
    OP(0xff):   // RST  7
OP_RST:
    {
        u32 src;
        u32 newPC;

        src = zRealPC;
        PUSH_16(src);
        newPC = Opcode & 0x38;
        SET_PC(newPC);
        RET(11)
    }

    // INPUT & OUTPUT

    {
        u32 adr;
        
    OP(0xd3):   // OUT  (n),A
OP_OUT_mN_A:
	{ u8 t; FETCH_BYTE(t); adr = (zA << 8) | t; }
        OUT(adr, zA)
        RET(11)

    OP(0xdb):   // IN   A,(n)
OP_IN_A_mN:
	{ u8 t; FETCH_BYTE(t); adr = (zA << 8) | t; }
        IN(adr, zA)
        RET(11)
    }
    
    // PREFIXE
    
    OP(0xcb):   // CB PREFIXE (BIT & SHIFT INSTRUCTIONS)
CB_PREFIXE:
        Opcode = REAL_FETCH_BYTE;
        #include "cz80_opCB.inc"

    OP(0xed):   // ED PREFIXE
ED_PREFIXE:
        Z80_ICount -= 4;
        Opcode = REAL_FETCH_BYTE;
        #include "cz80_opED.inc"

    OP(0xdd):   // DD PREFIXE (IX)
DD_PREFIXE:
        data = pzIX;
        goto XY_PREFIXE;

    OP(0xfd):   // FD PREFIXE (IY)
FD_PREFIXE:
        data = pzIY;
        
XY_PREFIXE:
        Z80_ICount -= 4;
        Opcode = REAL_FETCH_BYTE;
        #include "cz80_opXY.inc"

#if CZ80_USE_JUMPTABLE
#else
}
#endif
