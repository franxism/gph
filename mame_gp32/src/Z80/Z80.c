/*****************************************************************************
 *
 *	 z80.c
 *	 Portable Z80 emulator
 *
 *	 Copyright (c) 1998 Juergen Buchmueller, all rights reserved.
 *
 *	 - This source code is released as freeware for non-commercial purposes.
 *	 - You are free to use and redistribute this code in modified or
 *	   unmodified form, provided you list me in the credits.
 *	 - If you modify this source code, you must add a notice to each modified
 *	   source file that it has been changed.  If you're a nice person, you
 *	   will clearly mark each change too.  :)
 *	 - If you wish to use this for commercial purposes, please contact me at
 *	   pullmoll@t-online.de
 *	 - The author of this copywritten work reserves the right to change the
 *     terms of its usage and license at any time, including retroactively
 *   - This entire notice must remain in the source code.
 *
 *****************************************************************************/

#include "driver.h"
#include "cpuintrf.h"
#include "z80.h"

#define VERBOSE 0

#ifndef INLINE
#define INLINE	static __inline__
#endif

#if VERBOSE
#define LOG(x)  if( errorlog ) { fprintf x; fclose(errorlog); errorlog = fopen("error.log", "a"); }
#else
#define LOG(x)
#endif

#ifdef  MAME_DEBUG
extern int mame_debug;
#define CALL_MAME_DEBUG if (mame_debug) MAME_Debug()
#else
#define CALL_MAME_DEBUG
#endif

extern int previouspc;

#define BIG_SWITCH			1
#define INC_R_ONCE			1	/* compatibility with old Z80 core */
#define BUSY_LOOP_HACKS 	1
#define BIG_FLAGS_ARRAY 	1	/* big flags array for ADD/ADC/SUB/SBC results */

#ifdef X86_ASM
#undef  BIG_FLAGS_ARRAY
#define BIG_FLAGS_ARRAY     0
#endif

#define CF  0x01
#define NF	0x02
#define PF	0x04
#define VF	PF
#define XF	0x08
#define HF	0x10
#define YF	0x20
#define ZF	0x40
#define SF	0x80

#define INT_IRQ 0x01
#define NMI_IRQ 0x02

#define _BC     Z80.BC.W.l
#define _B      Z80.BC.B.h
#define _C      Z80.BC.B.l

#define _DE     Z80.DE.W.l
#define _D      Z80.DE.B.h
#define _E      Z80.DE.B.l

#define _HL     Z80.HL.W.l
#define _H      Z80.HL.B.h
#define _L      Z80.HL.B.l

#define _AF     Z80.AF.W.l
#define _A      Z80.AF.B.h
#define _F      Z80.AF.B.l

#define _IX     Z80.IX.W.l
#define _HX     Z80.IX.B.h
#define _LX     Z80.IX.B.l

#define _IY     Z80.IY.W.l
#define _HY     Z80.IY.B.h
#define _LY     Z80.IY.B.l

#define _PCD    Z80.PC.D
#define _PC     Z80.PC.W.l

#define _SPD    Z80.SP.D
#define _SP     Z80.SP.W.l

#define _I      Z80.I
#define _R      Z80.R
#define _R2     Z80.R2
#define _IM     Z80.IM
#define _IFF1	Z80.IFF1
#define _IFF2	Z80.IFF2
#define _HALT	Z80.HALT

int Z80_ICount;
static Z80_Regs Z80;
static UINT16 EA;
static UINT8 SZ[256];		/* zero and sign flags */
static UINT8 SZP[256];		/* zero, sign and parity flags */
static UINT8 SZHV_inc[256]; /* zero, sign, half carry and overflow flags INC r8 */
static UINT8 SZHV_dec[256]; /* zero, sign, half carry and overflow flags DEC r8 */
#include "z80daa.h"
#if BIG_FLAGS_ARRAY
#include <signal.h>
static UINT8 *SZHVC_add = 0;
static UINT8 *SZHVC_sub = 0;
#endif

static UINT8 cc_op[0x100] = {
 4,10, 7, 6, 4, 4, 7, 4, 4,11, 7, 6, 4, 4, 7, 4,
 8,10, 7, 6, 4, 4, 7, 4, 7,11, 7, 6, 4, 4, 7, 4,
 7,10,16, 6, 4, 4, 7, 4, 7,11,16, 6, 4, 4, 7, 4,
 7,10,13, 6,11,11,10, 4, 7,11,13, 6, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 7, 7, 7, 7, 7, 7, 4, 7, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
 5,10,10,10,10,11, 7,11, 5, 4,10, 0,10,10, 7,11,
 5,10,10,11,10,11, 7,11, 5, 4,10,11,10, 0, 7,11,
 5,10,10,19,10,11, 7,11, 5, 4,10, 4,10, 0, 7,11,
 5,10,10, 4,10,11, 7,11, 5, 6,10, 4,10, 0, 7,11};


static UINT8 cc_cb[0x100] = {
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,
 8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8};

static UINT8 cc_dd[0x100] = {
 4, 4, 4, 4, 4, 4, 4, 4, 4,15, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4,15, 4, 4, 4, 4, 4, 4,
 4,14,20,10, 9, 9, 9, 4, 4,15,20,10, 9, 9, 9, 4,
 4, 4, 4, 4,23,23,19, 4, 4,15, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 9, 9, 9, 9, 9, 9,19, 9, 9, 9, 9, 9, 9, 9,19, 9,
19,19,19,19,19,19, 4,19, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
 4,14, 4,23, 4,15, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4,10, 4, 4, 4, 4, 4, 4};

static UINT8 cc_fd[0x100] = {
 4, 4, 4, 4, 4, 4, 4, 4, 4,15, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4,15, 4, 4, 4, 4, 4, 4,
 4,14,20,10, 9, 9, 9, 4, 4,15,20,10, 9, 9, 9, 4,
 4, 4, 4, 4,23,23,19, 4, 4,15, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 9, 9, 9, 9, 9, 9,19, 9, 9, 9, 9, 9, 9, 9,19, 9,
19,19,19,19,19,19, 4,19, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 9, 9,19, 4, 4, 4, 4, 4, 9, 9,19, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
 4,14, 4,23, 4,15, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, 4,10, 4, 4, 4, 4, 4, 4};

static UINT8 cc_xxcb[0x100] = {
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23};

static UINT8 cc_ed[0x100] = {
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
12,12,15,20, 8, 8, 8, 9,12,12,15,20, 8, 8, 8, 9,
12,12,15,20, 8, 8, 8, 9,12,12,15,20, 8, 8, 8, 9,
12,12,15,20, 8, 8, 8,18,12,12,15,20, 8, 8, 8,18,
12,12,15,20, 8,18, 8, 8,12,12,15,20, 8,18, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
16,16,16,16, 8, 8, 8, 8,16,16,16,16, 8, 8, 8, 8,
16,16,16,16, 8, 8, 8, 8,16,16,16,16, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

static void (*Z80op[0x100])(void);
static void (*Z80cb[0x100])(void);
static void (*Z80dd[0x100])(void);
static void (*Z80ed[0x100])(void);
static void (*Z80fd[0x100])(void);
static void (*Z80xxcb[0x100])(void);

/***************************************************************
 * define an opcode function
 ***************************************************************/
#define OP(prefix,opcode)  INLINE void prefix##_##opcode(void)

/***************************************************************
 * adjust cycle count by n T-states and R by n/4 M-cycles
 ***************************************************************/
#if INC_R_ONCE
/* old behaviour; cycle count independent of R register */
#define CY(cycles) Z80_ICount -= cycles
#define R_INC _R++
#else
/* new behaviour: R register counts T-States; get/put adjust to M-Cycles */
#define CY(cycles) Z80_ICount -= cycles; _R += cycles
#define R_INC
#endif

/***************************************************************
 * execute an opcode
 ***************************************************************/
#define EXEC(prefix,opcode) {									\
	unsigned op = opcode;										\
	CY(cc_##prefix[op]);										\
	(*Z80##prefix[op])();										\
}

#if BIG_SWITCH
#define EXEC_INLINE(prefix,opcode) {							\
	unsigned op = opcode;										\
	CY(cc_##prefix[op]);										\
	switch(op) {												\
	case 0x00:prefix##_##00();break; case 0x01:prefix##_##01();break; case 0x02:prefix##_##02();break; case 0x03:prefix##_##03();break; \
	case 0x04:prefix##_##04();break; case 0x05:prefix##_##05();break; case 0x06:prefix##_##06();break; case 0x07:prefix##_##07();break; \
	case 0x08:prefix##_##08();break; case 0x09:prefix##_##09();break; case 0x0a:prefix##_##0a();break; case 0x0b:prefix##_##0b();break; \
	case 0x0c:prefix##_##0c();break; case 0x0d:prefix##_##0d();break; case 0x0e:prefix##_##0e();break; case 0x0f:prefix##_##0f();break; \
	case 0x10:prefix##_##10();break; case 0x11:prefix##_##11();break; case 0x12:prefix##_##12();break; case 0x13:prefix##_##13();break; \
	case 0x14:prefix##_##14();break; case 0x15:prefix##_##15();break; case 0x16:prefix##_##16();break; case 0x17:prefix##_##17();break; \
	case 0x18:prefix##_##18();break; case 0x19:prefix##_##19();break; case 0x1a:prefix##_##1a();break; case 0x1b:prefix##_##1b();break; \
	case 0x1c:prefix##_##1c();break; case 0x1d:prefix##_##1d();break; case 0x1e:prefix##_##1e();break; case 0x1f:prefix##_##1f();break; \
	case 0x20:prefix##_##20();break; case 0x21:prefix##_##21();break; case 0x22:prefix##_##22();break; case 0x23:prefix##_##23();break; \
	case 0x24:prefix##_##24();break; case 0x25:prefix##_##25();break; case 0x26:prefix##_##26();break; case 0x27:prefix##_##27();break; \
	case 0x28:prefix##_##28();break; case 0x29:prefix##_##29();break; case 0x2a:prefix##_##2a();break; case 0x2b:prefix##_##2b();break; \
	case 0x2c:prefix##_##2c();break; case 0x2d:prefix##_##2d();break; case 0x2e:prefix##_##2e();break; case 0x2f:prefix##_##2f();break; \
	case 0x30:prefix##_##30();break; case 0x31:prefix##_##31();break; case 0x32:prefix##_##32();break; case 0x33:prefix##_##33();break; \
	case 0x34:prefix##_##34();break; case 0x35:prefix##_##35();break; case 0x36:prefix##_##36();break; case 0x37:prefix##_##37();break; \
	case 0x38:prefix##_##38();break; case 0x39:prefix##_##39();break; case 0x3a:prefix##_##3a();break; case 0x3b:prefix##_##3b();break; \
	case 0x3c:prefix##_##3c();break; case 0x3d:prefix##_##3d();break; case 0x3e:prefix##_##3e();break; case 0x3f:prefix##_##3f();break; \
	case 0x40:prefix##_##40();break; case 0x41:prefix##_##41();break; case 0x42:prefix##_##42();break; case 0x43:prefix##_##43();break; \
	case 0x44:prefix##_##44();break; case 0x45:prefix##_##45();break; case 0x46:prefix##_##46();break; case 0x47:prefix##_##47();break; \
	case 0x48:prefix##_##48();break; case 0x49:prefix##_##49();break; case 0x4a:prefix##_##4a();break; case 0x4b:prefix##_##4b();break; \
	case 0x4c:prefix##_##4c();break; case 0x4d:prefix##_##4d();break; case 0x4e:prefix##_##4e();break; case 0x4f:prefix##_##4f();break; \
	case 0x50:prefix##_##50();break; case 0x51:prefix##_##51();break; case 0x52:prefix##_##52();break; case 0x53:prefix##_##53();break; \
	case 0x54:prefix##_##54();break; case 0x55:prefix##_##55();break; case 0x56:prefix##_##56();break; case 0x57:prefix##_##57();break; \
	case 0x58:prefix##_##58();break; case 0x59:prefix##_##59();break; case 0x5a:prefix##_##5a();break; case 0x5b:prefix##_##5b();break; \
	case 0x5c:prefix##_##5c();break; case 0x5d:prefix##_##5d();break; case 0x5e:prefix##_##5e();break; case 0x5f:prefix##_##5f();break; \
	case 0x60:prefix##_##60();break; case 0x61:prefix##_##61();break; case 0x62:prefix##_##62();break; case 0x63:prefix##_##63();break; \
	case 0x64:prefix##_##64();break; case 0x65:prefix##_##65();break; case 0x66:prefix##_##66();break; case 0x67:prefix##_##67();break; \
	case 0x68:prefix##_##68();break; case 0x69:prefix##_##69();break; case 0x6a:prefix##_##6a();break; case 0x6b:prefix##_##6b();break; \
	case 0x6c:prefix##_##6c();break; case 0x6d:prefix##_##6d();break; case 0x6e:prefix##_##6e();break; case 0x6f:prefix##_##6f();break; \
	case 0x70:prefix##_##70();break; case 0x71:prefix##_##71();break; case 0x72:prefix##_##72();break; case 0x73:prefix##_##73();break; \
	case 0x74:prefix##_##74();break; case 0x75:prefix##_##75();break; case 0x76:prefix##_##76();break; case 0x77:prefix##_##77();break; \
	case 0x78:prefix##_##78();break; case 0x79:prefix##_##79();break; case 0x7a:prefix##_##7a();break; case 0x7b:prefix##_##7b();break; \
	case 0x7c:prefix##_##7c();break; case 0x7d:prefix##_##7d();break; case 0x7e:prefix##_##7e();break; case 0x7f:prefix##_##7f();break; \
	case 0x80:prefix##_##80();break; case 0x81:prefix##_##81();break; case 0x82:prefix##_##82();break; case 0x83:prefix##_##83();break; \
	case 0x84:prefix##_##84();break; case 0x85:prefix##_##85();break; case 0x86:prefix##_##86();break; case 0x87:prefix##_##87();break; \
	case 0x88:prefix##_##88();break; case 0x89:prefix##_##89();break; case 0x8a:prefix##_##8a();break; case 0x8b:prefix##_##8b();break; \
	case 0x8c:prefix##_##8c();break; case 0x8d:prefix##_##8d();break; case 0x8e:prefix##_##8e();break; case 0x8f:prefix##_##8f();break; \
	case 0x90:prefix##_##90();break; case 0x91:prefix##_##91();break; case 0x92:prefix##_##92();break; case 0x93:prefix##_##93();break; \
	case 0x94:prefix##_##94();break; case 0x95:prefix##_##95();break; case 0x96:prefix##_##96();break; case 0x97:prefix##_##97();break; \
	case 0x98:prefix##_##98();break; case 0x99:prefix##_##99();break; case 0x9a:prefix##_##9a();break; case 0x9b:prefix##_##9b();break; \
	case 0x9c:prefix##_##9c();break; case 0x9d:prefix##_##9d();break; case 0x9e:prefix##_##9e();break; case 0x9f:prefix##_##9f();break; \
	case 0xa0:prefix##_##a0();break; case 0xa1:prefix##_##a1();break; case 0xa2:prefix##_##a2();break; case 0xa3:prefix##_##a3();break; \
	case 0xa4:prefix##_##a4();break; case 0xa5:prefix##_##a5();break; case 0xa6:prefix##_##a6();break; case 0xa7:prefix##_##a7();break; \
	case 0xa8:prefix##_##a8();break; case 0xa9:prefix##_##a9();break; case 0xaa:prefix##_##aa();break; case 0xab:prefix##_##ab();break; \
	case 0xac:prefix##_##ac();break; case 0xad:prefix##_##ad();break; case 0xae:prefix##_##ae();break; case 0xaf:prefix##_##af();break; \
	case 0xb0:prefix##_##b0();break; case 0xb1:prefix##_##b1();break; case 0xb2:prefix##_##b2();break; case 0xb3:prefix##_##b3();break; \
	case 0xb4:prefix##_##b4();break; case 0xb5:prefix##_##b5();break; case 0xb6:prefix##_##b6();break; case 0xb7:prefix##_##b7();break; \
	case 0xb8:prefix##_##b8();break; case 0xb9:prefix##_##b9();break; case 0xba:prefix##_##ba();break; case 0xbb:prefix##_##bb();break; \
	case 0xbc:prefix##_##bc();break; case 0xbd:prefix##_##bd();break; case 0xbe:prefix##_##be();break; case 0xbf:prefix##_##bf();break; \
	case 0xc0:prefix##_##c0();break; case 0xc1:prefix##_##c1();break; case 0xc2:prefix##_##c2();break; case 0xc3:prefix##_##c3();break; \
	case 0xc4:prefix##_##c4();break; case 0xc5:prefix##_##c5();break; case 0xc6:prefix##_##c6();break; case 0xc7:prefix##_##c7();break; \
	case 0xc8:prefix##_##c8();break; case 0xc9:prefix##_##c9();break; case 0xca:prefix##_##ca();break; case 0xcb:prefix##_##cb();break; \
	case 0xcc:prefix##_##cc();break; case 0xcd:prefix##_##cd();break; case 0xce:prefix##_##ce();break; case 0xcf:prefix##_##cf();break; \
	case 0xd0:prefix##_##d0();break; case 0xd1:prefix##_##d1();break; case 0xd2:prefix##_##d2();break; case 0xd3:prefix##_##d3();break; \
	case 0xd4:prefix##_##d4();break; case 0xd5:prefix##_##d5();break; case 0xd6:prefix##_##d6();break; case 0xd7:prefix##_##d7();break; \
	case 0xd8:prefix##_##d8();break; case 0xd9:prefix##_##d9();break; case 0xda:prefix##_##da();break; case 0xdb:prefix##_##db();break; \
	case 0xdc:prefix##_##dc();break; case 0xdd:prefix##_##dd();break; case 0xde:prefix##_##de();break; case 0xdf:prefix##_##df();break; \
	case 0xe0:prefix##_##e0();break; case 0xe1:prefix##_##e1();break; case 0xe2:prefix##_##e2();break; case 0xe3:prefix##_##e3();break; \
	case 0xe4:prefix##_##e4();break; case 0xe5:prefix##_##e5();break; case 0xe6:prefix##_##e6();break; case 0xe7:prefix##_##e7();break; \
	case 0xe8:prefix##_##e8();break; case 0xe9:prefix##_##e9();break; case 0xea:prefix##_##ea();break; case 0xeb:prefix##_##eb();break; \
	case 0xec:prefix##_##ec();break; case 0xed:prefix##_##ed();break; case 0xee:prefix##_##ee();break; case 0xef:prefix##_##ef();break; \
	case 0xf0:prefix##_##f0();break; case 0xf1:prefix##_##f1();break; case 0xf2:prefix##_##f2();break; case 0xf3:prefix##_##f3();break; \
	case 0xf4:prefix##_##f4();break; case 0xf5:prefix##_##f5();break; case 0xf6:prefix##_##f6();break; case 0xf7:prefix##_##f7();break; \
	case 0xf8:prefix##_##f8();break; case 0xf9:prefix##_##f9();break; case 0xfa:prefix##_##fa();break; case 0xfb:prefix##_##fb();break; \
	case 0xfc:prefix##_##fc();break; case 0xfd:prefix##_##fd();break; case 0xfe:prefix##_##fe();break; case 0xff:prefix##_##ff();break; \
	}																																	\
}
#else
#define EXEC_INLINE EXEC
#endif


/***************************************************************
 * Input a byte from given I/O port
 ***************************************************************/
#define IN(port)   ((UINT8)cpu_readport(port))

/***************************************************************
 * Output a byte to given I/O port
 ***************************************************************/
#define OUT(port,value) cpu_writeport(port,value)

/***************************************************************
 * Read a byte from given memory location
 ***************************************************************/
#define RM(Addr) cpu_readmem16(Addr)

INLINE UINT16 RM16(unsigned Addr)
{
	return RM(Addr) | (RM((Addr+1) & 0xffff) << 8);
}

/***************************************************************
 * Write a byte to given memory location
 ***************************************************************/
#define WM(Addr,Value) cpu_writemem16(Addr,Value)

INLINE void WM16(unsigned Addr, Z80_pair *r )
{
	WM( Addr, r->B.l );
	WM( Addr+1, r->B.h );
}

/***************************************************************
 * ROP() is identical to RM() except it is used for
 * reading opcodes. In case of system with memory mapped I/O,
 * this function can be used to greatly speed up emulation
 ***************************************************************/
INLINE UINT8 ROP(void)
{
	unsigned pc = _PCD;
	_PC++;
	return cpu_readop(pc);
}

/****************************************************************
 * ARG() is identical to ROP() except it is used
 * for reading opcode arguments. This difference can be used to
 * support systems that use different encoding mechanisms for
 * opcodes and opcode arguments
 ***************************************************************/
INLINE UINT8 ARG(void)
{
	unsigned pc = _PCD;
    _PC++;
	return cpu_readop_arg(pc);
}

#ifdef	LSB_FIRST
INLINE UINT16 ARG16(void)
{
	unsigned pc = _PCD;
	_PC += 2;
	return cpu_readop_arg16(pc);
}
#else
INLINE UINT16 ARG16(void)
{
	unsigned pc = _PCD;
    _PC += 2;
	return cpu_readop_arg(pc)|(cpu_readop_arg(pc+1)<<8);
}
#endif

/***************************************************************
 * Calculate the effective address EA of an opcode using
 * IX+offset resp. IY+offset addressing.
 ***************************************************************/
#define EAX EA = (UINT16)(_IX+(INT8)ARG())
#define EAY EA = (UINT16)(_IY+(INT8)ARG())

/***************************************************************
 * POP
 ***************************************************************/
#define POP(DR) { Z80.DR.D = RM16( _SPD ); _SP += 2; }

/***************************************************************
 * PUSH
 ***************************************************************/
#define PUSH(SR) { _SP -= 2; WM16( _SPD, &Z80.SR ); }

/***************************************************************
 * JP
 ***************************************************************/
#if BUSY_LOOP_HACKS
#define JP {													\
	unsigned oldpc = _PCD-1;									\
	_PCD = ARG16(); 											\
	/* speed up busy loop */									\
	if (_PCD == oldpc) {										\
		if (Z80_ICount > 0) Z80_ICount = 0; 					\
	} else														\
	/* NOP - JP */												\
	if (_PCD == oldpc-1 && cpu_readop(_PCD) == 0x00) {			\
		if (Z80_ICount > 0) Z80_ICount = 0; 					\
	} else														\
	/* LD SP,#xxxx - Galaga */									\
	if (_PCD == oldpc-3 && cpu_readop(_PCD) == 0x31) {			\
		if (Z80_ICount > 10) Z80_ICount = 10;					\
	} else														\
	/* EI - JP */												\
	if (_PCD == oldpc-1 && cpu_readop(_PCD) == 0xfb && Z80.pending_irq == 0) { \
		if (Z80_ICount > 4) Z80_ICount = 4; 					\
	}															\
	change_pc16(_PCD);											\
}
#else
#define JP {													\
	_PCD = ARG16(); 											\
	change_pc16(_PCD);											\
}
#endif

/***************************************************************
 * JP_COND
 ***************************************************************/

#define JP_COND(cond)											\
    if (cond) {                                                 \
		_PCD = ARG16(); 										\
		change_pc16(_PCD);										\
    } else {                                                    \
		_PC += 2;												\
    }

/***************************************************************
 * JR
 ***************************************************************/
#define JR(cond)												\
	if (cond) { 												\
		_PC += (INT8)ARG(); 									\
        CY(5);                                                  \
		change_pc16(_PCD);										\
	} else _PC++;												\

/***************************************************************
 * CALL
 ***************************************************************/
#define CALL(cond)												\
	if (cond) { 												\
		EA = ARG16();											\
		PUSH( PC ); 											\
		_PCD = EA;												\
        CY(7);                                                  \
		change_pc16(_PCD);										\
	} else {													\
		_PC+=2; 												\
	}

/***************************************************************
 * RET
 ***************************************************************/
#define RET(cond)												\
	if (cond) { 												\
		POP(PC);												\
		change_pc16(_PCD);										\
		CY(6);													\
	}

/***************************************************************
 * RETN
 ***************************************************************/
#if NEW_INTERRUPT_SYSTEM
#define RETN	{												\
	if (!_IFF1 && _IFF2) {										\
        if (Z80.irq_state != CLEAR_LINE) {                      \
            Z80.pending_irq |= INT_IRQ;                         \
        }                                                       \
	}															\
	_IFF1 = _IFF2;												\
	RET(1); 													\
}
#else
#define RETN    {                                               \
    _IFF1 = _IFF2;                                              \
	RET(1); 													\
}
#endif

/***************************************************************
 * RETI
 ***************************************************************/
#define RETI	{												\
	int device = Z80.service_irq;								\
	if (device >= 0) {											\
		Z80.irq[device].interrupt_reti(Z80.irq[device].irq_param); \
	}															\
	RET(1); 													\
}

/***************************************************************
 * LD	R,A
 ***************************************************************/
#if INC_R_ONCE
#define LD_R_A {												\
	_R = _A;													\
	_R2 = _A & 0x80;				/* keep bit 7 of R */		\
}
#else
#define LD_R_A {                                                \
	/* R register counts T-States (M-cycles = T-States / 4) */	\
    _R = _A << 2;                                               \
	_R2 = _A & 0x80;				/* keep bit 7 of R */		\
}
#endif

/***************************************************************
 * LD	A,R
 ***************************************************************/
#if INC_R_ONCE
#define LD_A_R {												\
	_A = (_R & 0x7f) | _R2; 									\
	_F = (_F & CF) | SZ[_A] | (_IFF2 << 2); 					\
}
#else
#define LD_A_R {                                                \
	/* our R counts T-States (M-cycles = T-States / 4) */		\
	_A = ((_R >> 2) & 0x7f) | _R2;								\
	_F = (_F & CF) | SZ[_A] | (_IFF2 << 2); 					\
}
#endif

/***************************************************************
 * LD	I,A
 ***************************************************************/
#define LD_I_A {												\
	_I = _A;													\
}

/***************************************************************
 * LD	A,I
 ***************************************************************/
#define LD_A_I {												\
	_A = _I;													\
	_F = (_F & CF) | SZ[_A] | (_IFF2 << 2); 					\
}

/***************************************************************
 * RST
 ***************************************************************/
#define RST(Addr)												\
	PUSH( PC ); 												\
	_PCD = Addr;												\
	change_pc16(_PCD)

/***************************************************************
 * INC	r8
 ***************************************************************/
INLINE UINT8 INC(UINT8 Value)
{
	unsigned res = (Value + 1) & 0xff;
	_F = (_F & CF) | SZHV_inc[res];
	return (UINT8)res;
}

/***************************************************************
 * DEC	r8
 ***************************************************************/
INLINE UINT8 DEC(UINT8 Value)
{
	unsigned res = (Value - 1) & 0xff;
	_F = (_F & CF) | SZHV_dec[res];
    return res;
}

/***************************************************************
 * RLCA
 ***************************************************************/
#define RLCA													\
	_A = (_A << 1) | (_A >> 7); 								\
	_F = (_F & (SF | ZF | YF | XF | PF)) | (_A & CF)

/***************************************************************
 * RRCA
 ***************************************************************/
#define RRCA													\
	_F = (_F & (SF | ZF | YF | XF | PF)) | (_A & CF);			\
	_A = (_A >> 1) | (_A << 7)

/***************************************************************
 * RLA
 ***************************************************************/
#define RLA {													\
	UINT8 res = (_A << 1) | (_F & CF);							\
	UINT8 c = (_A & 0x80) ? CF : 0; 							\
	_F = (_F & (SF | ZF | YF | XF | PF)) | c;					\
	_A = res;													\
}

/***************************************************************
 * RRA
 ***************************************************************/
#define RRA {													\
	UINT8 res = (_A >> 1) | (_F << 7);							\
	UINT8 c = (_A & 0x01) ? CF : 0; 							\
    _F = (_F & (SF | ZF | YF | XF | PF)) | c;                   \
	_A = res;													\
}

/***************************************************************
 * RRD
 ***************************************************************/
#define RRD {													\
	UINT8 n = RM(_HL);											\
	WM( _HL,(n >> 4) | (_A << 4) ); 							\
	_A = (_A & 0xf0) | (n & 0x0f);								\
	_F = (_F & CF) | SZP[_A];									\
}

/***************************************************************
 * RLD
 ***************************************************************/
#define RLD {                                                   \
    UINT8 n = RM(_HL);                                          \
    WM( _HL,(n << 4) | (_A & 0x0f) );                           \
    _A = (_A & 0xf0) | (n >> 4);                                \
	_F = (_F & CF) | SZP[_A];									\
}

/***************************************************************
 * ADD	A,n
 ***************************************************************/
#ifdef X86_ASM
#define ADD(Value)												\
 asm (															\
 " addb %2,%0           \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n" /* al = 1 if overflow */            \
 " shlb $2,%%al         \n" /* shift to P/V bit position */     \
 " andb $0xd1,%%ah      \n" /* sign, zero, half carry, carry */ \
 " orb %%ah,%%al        \n"                                     \
 :"=g" (_A), "=a" (_F)                                          \
 :"r" (Value), "0" (_A)                                         \
 )
#else
#if BIG_FLAGS_ARRAY
#define ADD(Value) {											\
	unsigned val = Value;										\
	unsigned res = (_A + val) & 0xff;							\
	_F = SZHVC_add[(_A << 8) | res];							\
    _A = res;                                                   \
}
#else
#define ADD(Value) {											\
	unsigned val = Value;										\
    unsigned res = _A + val;                                    \
    _F = SZ[(UINT8)res] | ((res >> 8) & CF) |                   \
        ((_A ^ res ^ val) & HF) |                               \
        (((val ^ _A ^ 0x80) & (val ^ res) & 0x80) >> 5);        \
    _A = (UINT8)res;                                            \
}
#endif
#endif

/***************************************************************
 * ADC	A,n
 ***************************************************************/
#ifdef X86_ASM
#define ADC(Value)												\
 asm (															\
 " shrb $1,%%al         \n"                                     \
 " adcb %2,%0           \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n" /* al = 1 if overflow */            \
 " shlb $2,%%al         \n" /* shift to P/V bit position */     \
 " andb $0xd1,%%ah      \n" /* sign, zero, half carry, carry */ \
 " orb %%ah,%%al        \n" /* combine with P/V */              \
 :"=g" (_A), "=a" (_F)                                          \
 :"r" (Value), "a" (_F), "0" (_A)                               \
 )
#else
#if BIG_FLAGS_ARRAY
#define ADC(Value) {											\
	unsigned val = Value;										\
	unsigned res = (_A + val + (_F & CF)) & 0xff;				\
	_F = SZHVC_add[((_F & CF) << 16) | (_A << 8) | res];		\
    _A = res;                                                   \
}
#else
#define ADC(Value) {                                            \
	unsigned val = Value;										\
	unsigned res = _A + val + (_F & CF);						\
	_F = SZ[res & 0xff] | ((res >> 8) & CF) |					\
		((_A ^ res ^ val) & HF) |								\
		(((val ^ _A ^ 0x80) & (val ^ res) & 0x80) >> 5);		\
	_A = res;													\
}
#endif
#endif

/***************************************************************
 * SUB	n
 ***************************************************************/
#ifdef X86_ASM
#define SUB(Value)												\
 asm (															\
 " subb %2,%0           \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n" /* al = 1 if overflow */            \
 " shlb $2,%%al         \n" /* shift to P/V bit position */     \
 " andb $0xd1,%%ah      \n" /* sign, zero, half carry, carry */ \
 " orb $2,%%al          \n" /* set N flag */                    \
 " orb %%ah,%%al        \n" /* combine with P/V */              \
 :"=g" (_A), "=a" (_F)                                          \
 :"r" (Value), "0" (_A)                                         \
 )
#else
#if BIG_FLAGS_ARRAY
#define SUB(Value) {											\
	unsigned val = Value;										\
	unsigned res = (_A - val) & 0xff;							\
	_F = SZHVC_sub[(_A << 8) | res];							\
    _A = res;                                                   \
}
#else
#define SUB(Value) {                                            \
	unsigned val = Value;										\
	unsigned res = _A - val;									\
	_F = SZ[res & 0xff] | ((res >> 8) & CF) | NF |				\
		((_A ^ res ^ val) & HF) |								\
		(((val ^ _A) & (_A ^ res) & 0x80) >> 5);				\
	_A = res;													\
}
#endif
#endif

/***************************************************************
 * SBC	A,n
 ***************************************************************/
#ifdef X86_ASM
#define SBC(Value)												\
 asm (															\
 " shrb $1,%%al         \n"                                     \
 " sbbb %2,%0           \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n" /* al = 1 if overflow */            \
 " shlb $2,%%al         \n" /* shift to P/V bit position */     \
 " andb $0xd1,%%ah      \n" /* sign, zero, half carry, carry */ \
 " orb $2,%%al          \n" /* set N flag */                    \
 " orb %%ah,%%al        \n" /* combine with P/V */              \
 :"=g" (_A), "=a" (_F)                                          \
 :"r" (Value), "a" (_F), "0" (_A)                               \
 )
#else
#if BIG_FLAGS_ARRAY
#define SBC(Value) {											\
	unsigned val = Value;										\
	unsigned res = (_A - val - (_F & CF)) & 0xff;				\
	_F = SZHVC_sub[((_F & CF) << 16) | (_A << 8) | res];		\
    _A = res;                                                   \
}
#else
#define SBC(Value) {                                            \
	unsigned val = Value;										\
	unsigned res = _A - val - (_F & CF);						\
	_F = SZ[res & 0xff] | ((res >> 8) & CF) | NF |				\
		((_A ^ res ^ val) & HF) |								\
		(((val ^ _A) & (_A ^ res) & 0x80) >> 5);				\
	_A = res;													\
}
#endif
#endif

/***************************************************************
 * NEG
 ***************************************************************/
#define NEG {                                                   \
	unsigned Value = _A;										\
	_A = 0; 													\
	SUB(Value); 												\
}

/***************************************************************
 * DAA
 ***************************************************************/
#define DAA {													\
	int idx = _A;												\
	if (_F & CF) idx |= 0x100;									\
	if (_F & HF) idx |= 0x200;									\
	if (_F & NF) idx |= 0x400;									\
	_AF = DAATable[idx];										\
}

/***************************************************************
 * AND	n
 ***************************************************************/
#define AND(Value)												\
	_A &= Value;												\
	_F = SZP[_A] | HF

/***************************************************************
 * OR	n
 ***************************************************************/
#define OR(Value)												\
	_A |= Value;												\
	_F = SZP[_A]

/***************************************************************
 * XOR	n
 ***************************************************************/
#define XOR(Value)												\
	_A ^= Value;												\
	_F = SZP[_A]

/***************************************************************
 * CP	n
 ***************************************************************/
#ifdef X86_ASM
#define CP(Value)												\
 asm (															\
 " cmpb %2,%0          \n"                                      \
 " lahf                \n"                                      \
 " setob %%al          \n" /* al = 1 if overflow */             \
 " shlb $2,%%al        \n" /* shift to P/V bit position */      \
 " andb $0xd1,%%ah     \n" /* sign, zero, half carry, carry */  \
 " orb $2,%%al         \n" /* set N flag */                     \
 " orb %%ah,%%al       \n" /* combine with P/V */               \
 :"=g" (_A), "=a" (_F)                                          \
 :"r" (Value), "0" (_A)                                         \
 )
#else
#if BIG_FLAGS_ARRAY
#define CP(Value) { 											\
	unsigned val = Value;										\
	unsigned res = (_A - val) & 0xff;							\
	_F = SZHVC_sub[(_A << 8) | res];							\
}
#else
#define CP(Value) {                                             \
	unsigned val = Value;										\
	unsigned res = _A - val;									\
	_F = SZ[res & 0xff] | ((res >> 8) & CF) | NF |				\
		((_A ^ res ^ val) & HF) |								\
		((((val ^ _A) & (_A ^ res)) >> 5) & VF);				\
}
#endif
#endif

/***************************************************************
 * EX   AF,AF'
 ***************************************************************/
#define EX_AF {                                                 \
    Z80_pair tmp;                                               \
    tmp = Z80.AF; Z80.AF = Z80.AF2; Z80.AF2 = tmp;              \
}

/***************************************************************
 * EX   DE,HL
 ***************************************************************/
#define EX_DE_HL {                                              \
    Z80_pair tmp;                                               \
    tmp = Z80.DE; Z80.DE = Z80.HL; Z80.HL = tmp;                \
}

/***************************************************************
 * EXX
 ***************************************************************/
#define EXX {                                                   \
    Z80_pair tmp;                                               \
    tmp = Z80.BC; Z80.BC = Z80.BC2; Z80.BC2 = tmp;              \
    tmp = Z80.DE; Z80.DE = Z80.DE2; Z80.DE2 = tmp;              \
    tmp = Z80.HL; Z80.HL = Z80.HL2; Z80.HL2 = tmp;              \
}

/***************************************************************
 * EX   (SP),r16
 ***************************************************************/
#define EXSP(reg) {                                             \
    Z80_pair tmp;                                               \
	tmp.D = RM16( _SPD );										\
	WM16( _SPD, &Z80.reg ); 									\
	Z80.reg = tmp;												\
}


/***************************************************************
 * ADD16
 ***************************************************************/
#ifdef	X86_ASM
#define ADD16(DR,SR)											\
 asm (															\
 " andb $0xc4,%1        \n"                                     \
 " addb %%al,%%cl       \n"                                     \
 " adcb %%ah,%%ch       \n"                                     \
 " lahf                 \n"                                     \
 " andb $0x11,%%ah      \n"                                     \
 " orb %%ah,%1          \n"                                     \
 :"=c" (Z80.DR.D), "=g" (Z80.AF.B.l)                            \
 :"0" (Z80.DR.D), "1" (Z80.AF.B.l), "a" (Z80.SR.D)              \
 )
#else
#define ADD16(DR,SR) {                                          \
	UINT32 res = Z80.DR.D + Z80.SR.D;							\
	_F = (_F & (SF | ZF | VF)) |								\
		(((Z80.DR.D ^ res ^ Z80.SR.D) >> 8) & HF) | 			\
		((res >> 16) & CF); 									\
	Z80.DR.W.l = (UINT16)res;									\
}
#endif

/***************************************************************
 * ADC	r16,r16
 ***************************************************************/
#ifdef	X86_ASM
#define ADC16(DR,SR)											\
 asm (                                                          \
 " shrb $1,%%al         \n"                                     \
 " adcb %%dl,%%cl       \n"                                     \
 " adcb %%dh,%%ch       \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n"                                     \
 " andb $0x91,%%ah      \n" /* sign, half carry and carry */    \
 " shlb $2,%%al         \n"                                     \
 " orb %%ah,%%al        \n" /* overflow into P/V */             \
 " orl %%ecx,%%ecx      \n"                                     \
 " lahf                 \n"                                     \
 " andb $0x40,%%ah      \n" /* zero */                          \
 " orb %%ah,%%al        \n"                                     \
 :"=c" (DR), "=a" (Z80.AF.B.l)                                  \
 :"0" (DR), "d" (SR), "a" (Z80.AF.B.l)                          \
 )
#else
#define ADC16(DR,SR) {                                          \
	UINT32 res = DR + SR + (_F & CF);							\
	_F = ( ((DR ^ res ^ SR) >> 8) & HF) |						\
		   ((res >> 16) & CF) | 								\
		   ((res >> 8) & SF) |									\
		   ((res & 0xffff) ? 0 : ZF) |							\
		   (((SR ^ DR ^ 0x8000) & (SR ^ res) & 0x8000) >> 13);	\
	DR = (UINT16)res;											\
}
#endif

/***************************************************************
 * SBC	r16,r16
 ***************************************************************/
#ifdef	X86_ASM
#define SBC16(DR,SR)											\
asm (															\
 " shrb $1,%%al         \n"                                     \
 " sbbb %%dl,%%cl       \n"                                     \
 " sbbb %%dh,%%ch       \n"                                     \
 " lahf                 \n"                                     \
 " setob %%al           \n"                                     \
 " andb $0x91,%%ah      \n" /* sign, half carry and carry */    \
 " shlb $2,%%al         \n"                                     \
 " orb %%ah,%%al        \n" /* overflow into P/V */             \
 " orl %%ecx,%%ecx      \n"                                     \
 " lahf                 \n"                                     \
 " orb $2,%%al          \n"                                     \
 " andb $0x40,%%ah      \n" /* zero */                          \
 " orb %%ah,%%al        \n"                                     \
 :"=c" (DR), "=a" (Z80.AF.B.l)                                  \
 :"0" (DR), "d" (SR),  "a" (Z80.AF.B.l)                         \
 )
#else
#define SBC16(DR,SR) {                                          \
	UINT32 res = DR - SR - (_F & CF);							\
	_F = ( ((DR ^ res ^ SR) >> 8) & HF) |						\
		   ((res >> 16) & CF) | 								\
		   ((res >> 8) & SF) |									\
		   ((res & 0xffff) ? 0 : ZF) |							\
		   (((SR ^ DR) & (DR ^ res) & 0x8000) >> 13);			\
    DR = (UINT16)res;                                           \
}
#endif

/***************************************************************
 * RLC	r8
 ***************************************************************/
INLINE UINT8 RLC(UINT8 Value)
{
	unsigned res = Value;
	unsigned c = (res & 0x80) ? CF : 0;
	res = ((res << 1) | (res >> 7)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * RRC	r8
 ***************************************************************/
INLINE UINT8 RRC(UINT8 Value)
{
	unsigned res = Value;
	unsigned c = (res & 0x01) ? CF : 0;
	res = ((res >> 1) | (res << 7)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * RL	r8
 ***************************************************************/
INLINE UINT8 RL(UINT8 Value)
{
	unsigned res = Value;
	unsigned c = (res & 0x80) ? CF : 0;
	res = ((res << 1) | (_F & CF)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * RR	r8
 ***************************************************************/
INLINE UINT8 RR(UINT8 Value)
{
	unsigned res = Value;
	unsigned c = (res & 0x01) ? CF : 0;
	res = ((res >> 1) | (_F << 7)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * SLA	r8
 ***************************************************************/
INLINE UINT8 SLA(UINT8 Value)
{
	unsigned res = Value;
	unsigned c = (res & 0x80) ? CF : 0;
	res = (res << 1) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * SRA	r8
 ***************************************************************/
INLINE UINT8 SRA(UINT8 Value)
{
	unsigned res = Value;
	unsigned c = (res & 0x01) ? CF : 0;
	res = ((res >> 1) | (res & 0x80)) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * SLL	r8
 ***************************************************************/
INLINE UINT8 SLL(UINT8 Value)
{
	unsigned res = Value;
	unsigned c = (res & 0x80) ? CF : 0;
	res = ((res << 1) | 0x01) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * SRL	r8
 ***************************************************************/
INLINE UINT8 SRL(UINT8 Value)
{
	unsigned res = Value;
	unsigned c = (res & 0x01) ? CF : 0;
	res = (res >> 1) & 0xff;
	_F = SZP[res] | c;
	return res;
}

/***************************************************************
 * BIT  bit,r8
 ***************************************************************/
#define BIT(bit,reg)                                            \
	_F = (_F & CF) | HF | SZ[reg & (1<<bit)]

/***************************************************************
 * RES	bit,r8
 ***************************************************************/
INLINE UINT8 RES(UINT8 bit, UINT8 Value)
{
	return Value & ~(1<<bit);
}

/***************************************************************
 * SET  bit,r8
 ***************************************************************/
INLINE UINT8 SET(UINT8 bit, UINT8 Value)
{
	return Value | (1<<bit);
}

/***************************************************************
 * LDI
 ***************************************************************/
#define LDI {													\
	WM( _DE, RM(_HL) ); 										\
	_HL++; _DE++; _BC--;										\
	_F &= SF | ZF | YF | XF | CF;								\
	if (_BC) _F |= VF;											\
}

/***************************************************************
 * CPI
 ***************************************************************/
#define CPI {													\
	UINT8 val = RM(_HL);										\
	UINT8 res = _A - val;										\
	_HL++; _BC--;												\
	_F = (_F & CF) | SZ[res] | ((_A ^ val ^ res) & HF) | NF;	\
	if (_BC) _F |= VF;											\
}

/***************************************************************
 * INI
 ***************************************************************/
#define INI {													\
	WM( _HL, IN(_BC) ); 										\
	_HL++; _B--;												\
	_F = (_B) ? NF : NF | ZF;									\
}

/***************************************************************
 * OUTI
 ***************************************************************/
#define OUTI {													\
	OUT( _BC, RM(_HL) );										\
	_HL++; _B--;												\
	_F = (_B) ? NF : NF | ZF;									\
}

/***************************************************************
 * LDD
 ***************************************************************/
#define LDD {													\
	WM( _DE, RM(_HL) ); 										\
	_HL--; _DE--; _BC--;										\
	_F &= SF | ZF | YF | XF | CF;								\
	if (_BC) _F |= VF;											\
}

/***************************************************************
 * CPD
 ***************************************************************/
#define CPD {													\
	UINT8 val = RM(_HL);										\
	UINT8 res = _A - val;										\
	_HL--; _BC--;												\
	_F = (_F & CF) | SZ[res] | ((_A ^ val ^ res) & HF) | NF;	\
	if (_BC) _F |= VF;											\
}

/***************************************************************
 * IND
 ***************************************************************/
#define IND {													\
	WM( _HL, IN(_BC) ); 										\
	_HL--; _B--;												\
	_F = (_B) ? NF : NF | ZF;									\
}

/***************************************************************
 * OUTD
 ***************************************************************/
#define OUTD {													\
	OUT( _BC, RM(_HL) );										\
	_HL--; _B--;												\
	_F = (_B) ? NF : NF | ZF;									\
}

/***************************************************************
 * LDIR
 ***************************************************************/
#define LDIR	LDI; if (_BC) { _PC -= 2; CY(5); }

/***************************************************************
 * CPIR
 ***************************************************************/
#define CPIR	CPI; if (_BC && !(_F & ZF)) { _PC -= 2; CY(5); }

/***************************************************************
 * INIR
 ***************************************************************/
#define INIR	INI; if (_B) { _PC -= 2; CY( 5); }

/***************************************************************
 * OTIR
 ***************************************************************/
#define OTIR	OUTI; if (_B) { _PC -= 2; CY( 5); }

/***************************************************************
 * LDDR
 ***************************************************************/
#define LDDR	LDD; if (_BC) { _PC -= 2; CY(5); }

/***************************************************************
 * CPDR
 ***************************************************************/
#define CPDR	CPD; if (_BC && !(_F & ZF)) { _PC -= 2; CY( 5); }

/***************************************************************
 * INDR
 ***************************************************************/
#define INDR	IND; if (_B) { _PC -= 2; CY( 5); }

/***************************************************************
 * OTDR
 ***************************************************************/
#define OTDR	OUTD; if (_B) { _PC -= 2; CY( 5); }

/***************************************************************
 * EI
 ***************************************************************/
#if NEW_INTERRUPT_SYSTEM
#define EI {													\
    /* If interrupts were disabled, execute one more            \
     * instruction and check the IRQ line.                      \
     * If not, simply set interrupt flip-flop 2                 \
     */                                                         \
    if (!_IFF1) {                                               \
        _IFF1 = _IFF2 = 1;                                      \
        R_INC;                                                  \
		previouspc = _PCD;										\
		CALL_MAME_DEBUG;										\
		EXEC(op,ROP()); 										\
		if (Z80.irq_state != CLEAR_LINE) {						\
            Z80.pending_irq |= INT_IRQ;                         \
		}														\
    } else _IFF2 = 1;                                           \
}
#else
#define EI {                                                    \
    /* If interrupts were disabled, execute one more            \
     * instruction and check the IRQ line.                      \
     * If not, simply set interrupt flip-flop 2                 \
     */                                                         \
    if (!_IFF1) {                                               \
        _IFF1 = _IFF2 = 1;                                      \
        R_INC;                                                  \
		previouspc = _PCD;										\
		CALL_MAME_DEBUG;										\
		EXEC(op,ROP()); 										\
    } else _IFF2 = 1;                                           \
}
#endif

/**********************************************************
 * opcodes with CB prefix
 * rotate, shift and bit operations
 **********************************************************/
OP(cb,00) { _B = RLC(_B);											} /* RLC  B 		  */
OP(cb,01) { _C = RLC(_C);											} /* RLC  C 		  */
OP(cb,02) { _D = RLC(_D);											} /* RLC  D 		  */
OP(cb,03) { _E = RLC(_E);											} /* RLC  E 		  */
OP(cb,04) { _H = RLC(_H);											} /* RLC  H 		  */
OP(cb,05) { _L = RLC(_L);											} /* RLC  L 		  */
OP(cb,06) { WM( _HL, RLC(RM(_HL)) );								} /* RLC  (HL)		  */
OP(cb,07) { _A = RLC(_A);											} /* RLC  A 		  */

OP(cb,08) { _B = RRC(_B);											} /* RRC  B 		  */
OP(cb,09) { _C = RRC(_C);											} /* RRC  C 		  */
OP(cb,0a) { _D = RRC(_D);											} /* RRC  D 		  */
OP(cb,0b) { _E = RRC(_E);											} /* RRC  E 		  */
OP(cb,0c) { _H = RRC(_H);											} /* RRC  H 		  */
OP(cb,0d) { _L = RRC(_L);											} /* RRC  L 		  */
OP(cb,0e) { WM( _HL, RRC(RM(_HL)) );								} /* RRC  (HL)		  */
OP(cb,0f) { _A = RRC(_A);											} /* RRC  A 		  */

OP(cb,10) { _B = RL(_B);											} /* RL   B 		  */
OP(cb,11) { _C = RL(_C);											} /* RL   C 		  */
OP(cb,12) { _D = RL(_D);											} /* RL   D 		  */
OP(cb,13) { _E = RL(_E);											} /* RL   E 		  */
OP(cb,14) { _H = RL(_H);											} /* RL   H 		  */
OP(cb,15) { _L = RL(_L);											} /* RL   L 		  */
OP(cb,16) { WM( _HL, RL(RM(_HL)) ); 								} /* RL   (HL)		  */
OP(cb,17) { _A = RL(_A);											} /* RL   A 		  */

OP(cb,18) { _B = RR(_B);											} /* RR   B 		  */
OP(cb,19) { _C = RR(_C);											} /* RR   C 		  */
OP(cb,1a) { _D = RR(_D);											} /* RR   D 		  */
OP(cb,1b) { _E = RR(_E);											} /* RR   E 		  */
OP(cb,1c) { _H = RR(_H);											} /* RR   H 		  */
OP(cb,1d) { _L = RR(_L);											} /* RR   L 		  */
OP(cb,1e) { WM( _HL, RR(RM(_HL)) ); 								} /* RR   (HL)		  */
OP(cb,1f) { _A = RR(_A);											} /* RR   A 		  */

OP(cb,20) { _B = SLA(_B);											} /* SLA  B 		  */
OP(cb,21) { _C = SLA(_C);											} /* SLA  C 		  */
OP(cb,22) { _D = SLA(_D);											} /* SLA  D 		  */
OP(cb,23) { _E = SLA(_E);											} /* SLA  E 		  */
OP(cb,24) { _H = SLA(_H);											} /* SLA  H 		  */
OP(cb,25) { _L = SLA(_L);											} /* SLA  L 		  */
OP(cb,26) { WM( _HL, SLA(RM(_HL)) );								} /* SLA  (HL)		  */
OP(cb,27) { _A = SLA(_A);											} /* SLA  A 		  */

OP(cb,28) { _B = SRA(_B);											} /* SRA  B 		  */
OP(cb,29) { _C = SRA(_C);											} /* SRA  C 		  */
OP(cb,2a) { _D = SRA(_D);											} /* SRA  D 		  */
OP(cb,2b) { _E = SRA(_E);											} /* SRA  E 		  */
OP(cb,2c) { _H = SRA(_H);											} /* SRA  H 		  */
OP(cb,2d) { _L = SRA(_L);											} /* SRA  L 		  */
OP(cb,2e) { WM( _HL, SRA(RM(_HL)) );								} /* SRA  (HL)		  */
OP(cb,2f) { _A = SRA(_A);											} /* SRA  A 		  */

OP(cb,30) { _B = SLL(_B);											} /* SLL  B 		  */
OP(cb,31) { _C = SLL(_C);											} /* SLL  C 		  */
OP(cb,32) { _D = SLL(_D);											} /* SLL  D 		  */
OP(cb,33) { _E = SLL(_E);											} /* SLL  E 		  */
OP(cb,34) { _H = SLL(_H);											} /* SLL  H 		  */
OP(cb,35) { _L = SLL(_L);											} /* SLL  L 		  */
OP(cb,36) { WM( _HL, SLL(RM(_HL)) );								} /* SLL  (HL)		  */
OP(cb,37) { _A = SLL(_A);											} /* SLL  A 		  */

OP(cb,38) { _B = SRL(_B);											} /* SRL  B 		  */
OP(cb,39) { _C = SRL(_C);											} /* SRL  C 		  */
OP(cb,3a) { _D = SRL(_D);											} /* SRL  D 		  */
OP(cb,3b) { _E = SRL(_E);											} /* SRL  E 		  */
OP(cb,3c) { _H = SRL(_H);											} /* SRL  H 		  */
OP(cb,3d) { _L = SRL(_L);											} /* SRL  L 		  */
OP(cb,3e) { WM( _HL, SRL(RM(_HL)) );								} /* SRL  (HL)		  */
OP(cb,3f) { _A = SRL(_A);											} /* SRL  A 		  */

OP(cb,40) { BIT(0,_B);												} /* BIT  0,B		  */
OP(cb,41) { BIT(0,_C);												} /* BIT  0,C		  */
OP(cb,42) { BIT(0,_D);												} /* BIT  0,D		  */
OP(cb,43) { BIT(0,_E);												} /* BIT  0,E		  */
OP(cb,44) { BIT(0,_H);												} /* BIT  0,H		  */
OP(cb,45) { BIT(0,_L);												} /* BIT  0,L		  */
OP(cb,46) { BIT(0,RM(_HL)); 										} /* BIT  0,(HL)	  */
OP(cb,47) { BIT(0,_A);												} /* BIT  0,A		  */

OP(cb,48) { BIT(1,_B);												} /* BIT  1,B		  */
OP(cb,49) { BIT(1,_C);												} /* BIT  1,C		  */
OP(cb,4a) { BIT(1,_D);												} /* BIT  1,D		  */
OP(cb,4b) { BIT(1,_E);												} /* BIT  1,E		  */
OP(cb,4c) { BIT(1,_H);												} /* BIT  1,H		  */
OP(cb,4d) { BIT(1,_L);												} /* BIT  1,L		  */
OP(cb,4e) { BIT(1,RM(_HL)); 										} /* BIT  1,(HL)	  */
OP(cb,4f) { BIT(1,_A);												} /* BIT  1,A		  */

OP(cb,50) { BIT(2,_B);												} /* BIT  2,B		  */
OP(cb,51) { BIT(2,_C);												} /* BIT  2,C		  */
OP(cb,52) { BIT(2,_D);												} /* BIT  2,D		  */
OP(cb,53) { BIT(2,_E);												} /* BIT  2,E		  */
OP(cb,54) { BIT(2,_H);												} /* BIT  2,H		  */
OP(cb,55) { BIT(2,_L);												} /* BIT  2,L		  */
OP(cb,56) { BIT(2,RM(_HL)); 										} /* BIT  2,(HL)	  */
OP(cb,57) { BIT(2,_A);												} /* BIT  2,A		  */

OP(cb,58) { BIT(3,_B);												} /* BIT  3,B		  */
OP(cb,59) { BIT(3,_C);												} /* BIT  3,C		  */
OP(cb,5a) { BIT(3,_D);												} /* BIT  3,D		  */
OP(cb,5b) { BIT(3,_E);												} /* BIT  3,E		  */
OP(cb,5c) { BIT(3,_H);												} /* BIT  3,H		  */
OP(cb,5d) { BIT(3,_L);												} /* BIT  3,L		  */
OP(cb,5e) { BIT(3,RM(_HL)); 										} /* BIT  3,(HL)	  */
OP(cb,5f) { BIT(3,_A);												} /* BIT  3,A		  */

OP(cb,60) { BIT(4,_B);												} /* BIT  4,B		  */
OP(cb,61) { BIT(4,_C);												} /* BIT  4,C		  */
OP(cb,62) { BIT(4,_D);												} /* BIT  4,D		  */
OP(cb,63) { BIT(4,_E);												} /* BIT  4,E		  */
OP(cb,64) { BIT(4,_H);												} /* BIT  4,H		  */
OP(cb,65) { BIT(4,_L);												} /* BIT  4,L		  */
OP(cb,66) { BIT(4,RM(_HL)); 										} /* BIT  4,(HL)	  */
OP(cb,67) { BIT(4,_A);												} /* BIT  4,A		  */

OP(cb,68) { BIT(5,_B);												} /* BIT  5,B		  */
OP(cb,69) { BIT(5,_C);												} /* BIT  5,C		  */
OP(cb,6a) { BIT(5,_D);												} /* BIT  5,D		  */
OP(cb,6b) { BIT(5,_E);												} /* BIT  5,E		  */
OP(cb,6c) { BIT(5,_H);												} /* BIT  5,H		  */
OP(cb,6d) { BIT(5,_L);												} /* BIT  5,L		  */
OP(cb,6e) { BIT(5,RM(_HL)); 										} /* BIT  5,(HL)	  */
OP(cb,6f) { BIT(5,_A);												} /* BIT  5,A		  */

OP(cb,70) { BIT(6,_B);												} /* BIT  6,B		  */
OP(cb,71) { BIT(6,_C);												} /* BIT  6,C		  */
OP(cb,72) { BIT(6,_D);												} /* BIT  6,D		  */
OP(cb,73) { BIT(6,_E);												} /* BIT  6,E		  */
OP(cb,74) { BIT(6,_H);												} /* BIT  6,H		  */
OP(cb,75) { BIT(6,_L);												} /* BIT  6,L		  */
OP(cb,76) { BIT(6,RM(_HL)); 										} /* BIT  6,(HL)	  */
OP(cb,77) { BIT(6,_A);												} /* BIT  6,A		  */

OP(cb,78) { BIT(7,_B);												} /* BIT  7,B		  */
OP(cb,79) { BIT(7,_C);												} /* BIT  7,C		  */
OP(cb,7a) { BIT(7,_D);												} /* BIT  7,D		  */
OP(cb,7b) { BIT(7,_E);												} /* BIT  7,E		  */
OP(cb,7c) { BIT(7,_H);												} /* BIT  7,H		  */
OP(cb,7d) { BIT(7,_L);												} /* BIT  7,L		  */
OP(cb,7e) { BIT(7,RM(_HL)); 										} /* BIT  7,(HL)	  */
OP(cb,7f) { BIT(7,_A);												} /* BIT  7,A		  */

OP(cb,80) { _B = RES(0,_B); 										} /* RES  0,B		  */
OP(cb,81) { _C = RES(0,_C); 										} /* RES  0,C		  */
OP(cb,82) { _D = RES(0,_D); 										} /* RES  0,D		  */
OP(cb,83) { _E = RES(0,_E); 										} /* RES  0,E		  */
OP(cb,84) { _H = RES(0,_H); 										} /* RES  0,H		  */
OP(cb,85) { _L = RES(0,_L); 										} /* RES  0,L		  */
OP(cb,86) { WM( _HL, RES(0,RM(_HL)) );								} /* RES  0,(HL)	  */
OP(cb,87) { _A = RES(0,_A); 										} /* RES  0,A		  */

OP(cb,88) { _B = RES(1,_B); 										} /* RES  1,B		  */
OP(cb,89) { _C = RES(1,_C); 										} /* RES  1,C		  */
OP(cb,8a) { _D = RES(1,_D); 										} /* RES  1,D		  */
OP(cb,8b) { _E = RES(1,_E); 										} /* RES  1,E		  */
OP(cb,8c) { _H = RES(1,_H); 										} /* RES  1,H		  */
OP(cb,8d) { _L = RES(1,_L); 										} /* RES  1,L		  */
OP(cb,8e) { WM( _HL, RES(1,RM(_HL)) );								} /* RES  1,(HL)	  */
OP(cb,8f) { _A = RES(1,_A); 										} /* RES  1,A		  */

OP(cb,90) { _B = RES(2,_B); 										} /* RES  2,B		  */
OP(cb,91) { _C = RES(2,_C); 										} /* RES  2,C		  */
OP(cb,92) { _D = RES(2,_D); 										} /* RES  2,D		  */
OP(cb,93) { _E = RES(2,_E); 										} /* RES  2,E		  */
OP(cb,94) { _H = RES(2,_H); 										} /* RES  2,H		  */
OP(cb,95) { _L = RES(2,_L); 										} /* RES  2,L		  */
OP(cb,96) { WM( _HL, RES(2,RM(_HL)) );								} /* RES  2,(HL)	  */
OP(cb,97) { _A = RES(2,_A); 										} /* RES  2,A		  */

OP(cb,98) { _B = RES(3,_B); 										} /* RES  3,B		  */
OP(cb,99) { _C = RES(3,_C); 										} /* RES  3,C		  */
OP(cb,9a) { _D = RES(3,_D); 										} /* RES  3,D		  */
OP(cb,9b) { _E = RES(3,_E); 										} /* RES  3,E		  */
OP(cb,9c) { _H = RES(3,_H); 										} /* RES  3,H		  */
OP(cb,9d) { _L = RES(3,_L); 										} /* RES  3,L		  */
OP(cb,9e) { WM( _HL, RES(3,RM(_HL)) );								} /* RES  3,(HL)	  */
OP(cb,9f) { _A = RES(3,_A); 										} /* RES  3,A		  */

OP(cb,a0) { _B = RES(4,_B); 										} /* RES  4,B		  */
OP(cb,a1) { _C = RES(4,_C); 										} /* RES  4,C		  */
OP(cb,a2) { _D = RES(4,_D); 										} /* RES  4,D		  */
OP(cb,a3) { _E = RES(4,_E); 										} /* RES  4,E		  */
OP(cb,a4) { _H = RES(4,_H); 										} /* RES  4,H		  */
OP(cb,a5) { _L = RES(4,_L); 										} /* RES  4,L		  */
OP(cb,a6) { WM( _HL, RES(4,RM(_HL)) );								} /* RES  4,(HL)	  */
OP(cb,a7) { _A = RES(4,_A); 										} /* RES  4,A		  */

OP(cb,a8) { _B = RES(5,_B); 										} /* RES  5,B		  */
OP(cb,a9) { _C = RES(5,_C); 										} /* RES  5,C		  */
OP(cb,aa) { _D = RES(5,_D); 										} /* RES  5,D		  */
OP(cb,ab) { _E = RES(5,_E); 										} /* RES  5,E		  */
OP(cb,ac) { _H = RES(5,_H); 										} /* RES  5,H		  */
OP(cb,ad) { _L = RES(5,_L); 										} /* RES  5,L		  */
OP(cb,ae) { WM( _HL, RES(5,RM(_HL)) );								} /* RES  5,(HL)	  */
OP(cb,af) { _A = RES(5,_A); 										} /* RES  5,A		  */

OP(cb,b0) { _B = RES(6,_B); 										} /* RES  6,B		  */
OP(cb,b1) { _C = RES(6,_C); 										} /* RES  6,C		  */
OP(cb,b2) { _D = RES(6,_D); 										} /* RES  6,D		  */
OP(cb,b3) { _E = RES(6,_E); 										} /* RES  6,E		  */
OP(cb,b4) { _H = RES(6,_H); 										} /* RES  6,H		  */
OP(cb,b5) { _L = RES(6,_L); 										} /* RES  6,L		  */
OP(cb,b6) { WM( _HL, RES(6,RM(_HL)) );								} /* RES  6,(HL)	  */
OP(cb,b7) { _A = RES(6,_A); 										} /* RES  6,A		  */

OP(cb,b8) { _B = RES(7,_B); 										} /* RES  7,B		  */
OP(cb,b9) { _C = RES(7,_C); 										} /* RES  7,C		  */
OP(cb,ba) { _D = RES(7,_D); 										} /* RES  7,D		  */
OP(cb,bb) { _E = RES(7,_E); 										} /* RES  7,E		  */
OP(cb,bc) { _H = RES(7,_H); 										} /* RES  7,H		  */
OP(cb,bd) { _L = RES(7,_L); 										} /* RES  7,L		  */
OP(cb,be) { WM( _HL, RES(7,RM(_HL)) );								} /* RES  7,(HL)	  */
OP(cb,bf) { _A = RES(7,_A); 										} /* RES  7,A		  */

OP(cb,c0) { _B = SET(0,_B); 										} /* SET  0,B		  */
OP(cb,c1) { _C = SET(0,_C); 										} /* SET  0,C		  */
OP(cb,c2) { _D = SET(0,_D); 										} /* SET  0,D		  */
OP(cb,c3) { _E = SET(0,_E); 										} /* SET  0,E		  */
OP(cb,c4) { _H = SET(0,_H); 										} /* SET  0,H		  */
OP(cb,c5) { _L = SET(0,_L); 										} /* SET  0,L		  */
OP(cb,c6) { WM( _HL, SET(0,RM(_HL)) );								} /* SET  0,(HL)	  */
OP(cb,c7) { _A = SET(0,_A); 										} /* SET  0,A		  */

OP(cb,c8) { _B = SET(1,_B); 										} /* SET  1,B		  */
OP(cb,c9) { _C = SET(1,_C); 										} /* SET  1,C		  */
OP(cb,ca) { _D = SET(1,_D); 										} /* SET  1,D		  */
OP(cb,cb) { _E = SET(1,_E); 										} /* SET  1,E		  */
OP(cb,cc) { _H = SET(1,_H); 										} /* SET  1,H		  */
OP(cb,cd) { _L = SET(1,_L); 										} /* SET  1,L		  */
OP(cb,ce) { WM( _HL, SET(1,RM(_HL)) );								} /* SET  1,(HL)	  */
OP(cb,cf) { _A = SET(1,_A); 										} /* SET  1,A		  */

OP(cb,d0) { _B = SET(2,_B); 										} /* SET  2,B		  */
OP(cb,d1) { _C = SET(2,_C); 										} /* SET  2,C		  */
OP(cb,d2) { _D = SET(2,_D); 										} /* SET  2,D		  */
OP(cb,d3) { _E = SET(2,_E); 										} /* SET  2,E		  */
OP(cb,d4) { _H = SET(2,_H); 										} /* SET  2,H		  */
OP(cb,d5) { _L = SET(2,_L); 										} /* SET  2,L		  */
OP(cb,d6) { WM( _HL, SET(2,RM(_HL)) );								}/* SET  2,(HL) 	 */
OP(cb,d7) { _A = SET(2,_A); 										} /* SET  2,A		  */

OP(cb,d8) { _B = SET(3,_B); 										} /* SET  3,B		  */
OP(cb,d9) { _C = SET(3,_C); 										} /* SET  3,C		  */
OP(cb,da) { _D = SET(3,_D); 										} /* SET  3,D		  */
OP(cb,db) { _E = SET(3,_E); 										} /* SET  3,E		  */
OP(cb,dc) { _H = SET(3,_H); 										} /* SET  3,H		  */
OP(cb,dd) { _L = SET(3,_L); 										} /* SET  3,L		  */
OP(cb,de) { WM( _HL, SET(3,RM(_HL)) );								} /* SET  3,(HL)	  */
OP(cb,df) { _A = SET(3,_A); 										} /* SET  3,A		  */

OP(cb,e0) { _B = SET(4,_B); 										} /* SET  4,B		  */
OP(cb,e1) { _C = SET(4,_C); 										} /* SET  4,C		  */
OP(cb,e2) { _D = SET(4,_D); 										} /* SET  4,D		  */
OP(cb,e3) { _E = SET(4,_E); 										} /* SET  4,E		  */
OP(cb,e4) { _H = SET(4,_H); 										} /* SET  4,H		  */
OP(cb,e5) { _L = SET(4,_L); 										} /* SET  4,L		  */
OP(cb,e6) { WM( _HL, SET(4,RM(_HL)) );								} /* SET  4,(HL)	  */
OP(cb,e7) { _A = SET(4,_A); 										} /* SET  4,A		  */

OP(cb,e8) { _B = SET(5,_B); 										} /* SET  5,B		  */
OP(cb,e9) { _C = SET(5,_C); 										} /* SET  5,C		  */
OP(cb,ea) { _D = SET(5,_D); 										} /* SET  5,D		  */
OP(cb,eb) { _E = SET(5,_E); 										} /* SET  5,E		  */
OP(cb,ec) { _H = SET(5,_H); 										} /* SET  5,H		  */
OP(cb,ed) { _L = SET(5,_L); 										} /* SET  5,L		  */
OP(cb,ee) { WM( _HL, SET(5,RM(_HL)) );								} /* SET  5,(HL)	  */
OP(cb,ef) { _A = SET(5,_A); 										} /* SET  5,A		  */

OP(cb,f0) { _B = SET(6,_B); 										} /* SET  6,B		  */
OP(cb,f1) { _C = SET(6,_C); 										} /* SET  6,C		  */
OP(cb,f2) { _D = SET(6,_D); 										} /* SET  6,D		  */
OP(cb,f3) { _E = SET(6,_E); 										} /* SET  6,E		  */
OP(cb,f4) { _H = SET(6,_H); 										} /* SET  6,H		  */
OP(cb,f5) { _L = SET(6,_L); 										} /* SET  6,L		  */
OP(cb,f6) { WM( _HL, SET(6,RM(_HL)) );								} /* SET  6,(HL)	  */
OP(cb,f7) { _A = SET(6,_A); 										} /* SET  6,A		  */

OP(cb,f8) { _B = SET(7,_B); 										} /* SET  7,B		  */
OP(cb,f9) { _C = SET(7,_C); 										} /* SET  7,C		  */
OP(cb,fa) { _D = SET(7,_D); 										} /* SET  7,D		  */
OP(cb,fb) { _E = SET(7,_E); 										} /* SET  7,E		  */
OP(cb,fc) { _H = SET(7,_H); 										} /* SET  7,H		  */
OP(cb,fd) { _L = SET(7,_L); 										} /* SET  7,L		  */
OP(cb,fe) { WM( _HL, SET(7,RM(_HL)) );								} /* SET  7,(HL)	  */
OP(cb,ff) { _A = SET(7,_A); 										} /* SET  7,A		  */


/**********************************************************
* opcodes with DD/FD CB prefix
* rotate, shift and bit operations with (IX+o)
**********************************************************/
OP(xxcb,00) { _B = RLC( RM(EA) ); WM( EA,_B );						} /* RLC  B=(XY+o)	  */
OP(xxcb,01) { _C = RLC( RM(EA) ); WM( EA,_C );						} /* RLC  C=(XY+o)	  */
OP(xxcb,02) { _D = RLC( RM(EA) ); WM( EA,_D );						} /* RLC  D=(XY+o)	  */
OP(xxcb,03) { _E = RLC( RM(EA) ); WM( EA,_E );						} /* RLC  E=(XY+o)	  */
OP(xxcb,04) { _H = RLC( RM(EA) ); WM( EA,_H );						} /* RLC  H=(XY+o)	  */
OP(xxcb,05) { _L = RLC( RM(EA) ); WM( EA,_L );						} /* RLC  L=(XY+o)	  */
OP(xxcb,06) { WM( EA, RLC( RM(EA) ) );								} /* RLC  (XY+o)	  */
OP(xxcb,07) { _A = RLC( RM(EA) ); WM( EA,_A );						} /* RLC  A=(XY+o)	  */

OP(xxcb,08) { _B = RRC( RM(EA) ); WM( EA,_B );						} /* RRC  B=(XY+o)	  */
OP(xxcb,09) { _C = RRC( RM(EA) ); WM( EA,_C );						} /* RRC  C=(XY+o)	  */
OP(xxcb,0a) { _D = RRC( RM(EA) ); WM( EA,_D );						} /* RRC  D=(XY+o)	  */
OP(xxcb,0b) { _E = RRC( RM(EA) ); WM( EA,_E );						} /* RRC  E=(XY+o)	  */
OP(xxcb,0c) { _H = RRC( RM(EA) ); WM( EA,_H );						} /* RRC  H=(XY+o)	  */
OP(xxcb,0d) { _L = RRC( RM(EA) ); WM( EA,_L );						} /* RRC  L=(XY+o)	  */
OP(xxcb,0e) { WM( EA,RRC( RM(EA) ) );								} /* RRC  (XY+o)	  */
OP(xxcb,0f) { _A = RRC( RM(EA) ); WM( EA,_A );						} /* RRC  A=(XY+o)	  */

OP(xxcb,10) { _B = RL( RM(EA) ); WM( EA,_B );						} /* RL   B=(XY+o)	  */
OP(xxcb,11) { _C = RL( RM(EA) ); WM( EA,_C );						} /* RL   C=(XY+o)	  */
OP(xxcb,12) { _D = RL( RM(EA) ); WM( EA,_D );						} /* RL   D=(XY+o)	  */
OP(xxcb,13) { _E = RL( RM(EA) ); WM( EA,_E );						} /* RL   E=(XY+o)	  */
OP(xxcb,14) { _H = RL( RM(EA) ); WM( EA,_H );						} /* RL   H=(XY+o)	  */
OP(xxcb,15) { _L = RL( RM(EA) ); WM( EA,_L );						} /* RL   L=(XY+o)	  */
OP(xxcb,16) { WM( EA,RL( RM(EA) ) );								} /* RL   (XY+o)	  */
OP(xxcb,17) { _A = RL( RM(EA) ); WM( EA,_A );						} /* RL   A=(XY+o)	  */

OP(xxcb,18) { _B = RR( RM(EA) ); WM( EA,_B );						} /* RR   B=(XY+o)	  */
OP(xxcb,19) { _C = RR( RM(EA) ); WM( EA,_C );						} /* RR   C=(XY+o)	  */
OP(xxcb,1a) { _D = RR( RM(EA) ); WM( EA,_D );						} /* RR   D=(XY+o)	  */
OP(xxcb,1b) { _E = RR( RM(EA) ); WM( EA,_E );						} /* RR   E=(XY+o)	  */
OP(xxcb,1c) { _H = RR( RM(EA) ); WM( EA,_H );						} /* RR   H=(XY+o)	  */
OP(xxcb,1d) { _L = RR( RM(EA) ); WM( EA,_L );						} /* RR   L=(XY+o)	  */
OP(xxcb,1e) { WM( EA,RR( RM(EA) ) );								} /* RR   (XY+o)	  */
OP(xxcb,1f) { _A = RR( RM(EA) ); WM( EA,_A );						} /* RR   A=(XY+o)	  */

OP(xxcb,20) { _B = SLA( RM(EA) ); WM( EA,_B );						} /* SLA  B=(XY+o)	  */
OP(xxcb,21) { _C = SLA( RM(EA) ); WM( EA,_C );						} /* SLA  C=(XY+o)	  */
OP(xxcb,22) { _D = SLA( RM(EA) ); WM( EA,_D );						} /* SLA  D=(XY+o)	  */
OP(xxcb,23) { _E = SLA( RM(EA) ); WM( EA,_E );						} /* SLA  E=(XY+o)	  */
OP(xxcb,24) { _H = SLA( RM(EA) ); WM( EA,_H );						} /* SLA  H=(XY+o)	  */
OP(xxcb,25) { _L = SLA( RM(EA) ); WM( EA,_L );						} /* SLA  L=(XY+o)	  */
OP(xxcb,26) { WM( EA,SLA( RM(EA) ) );								} /* SLA  (XY+o)	  */
OP(xxcb,27) { _A = SLA( RM(EA) ); WM( EA,_A );						} /* SLA  A=(XY+o)	  */

OP(xxcb,28) { _B = SRA( RM(EA) ); WM( EA,_B );						} /* SRA  B=(XY+o)	  */
OP(xxcb,29) { _C = SRA( RM(EA) ); WM( EA,_C );						} /* SRA  C=(XY+o)	  */
OP(xxcb,2a) { _D = SRA( RM(EA) ); WM( EA,_D );						} /* SRA  D=(XY+o)	  */
OP(xxcb,2b) { _E = SRA( RM(EA) ); WM( EA,_E );						} /* SRA  E=(XY+o)	  */
OP(xxcb,2c) { _H = SRA( RM(EA) ); WM( EA,_H );						} /* SRA  H=(XY+o)	  */
OP(xxcb,2d) { _L = SRA( RM(EA) ); WM( EA,_L );						} /* SRA  L=(XY+o)	  */
OP(xxcb,2e) { WM( EA,SRA( RM(EA) ) );								} /* SRA  (XY+o)	  */
OP(xxcb,2f) { _A = SRA( RM(EA) ); WM( EA,_A );						} /* SRA  A=(XY+o)	  */

OP(xxcb,30) { _B = SLL( RM(EA) ); WM( EA,_B );						} /* SLL  B=(XY+o)	  */
OP(xxcb,31) { _C = SLL( RM(EA) ); WM( EA,_C );						} /* SLL  C=(XY+o)	  */
OP(xxcb,32) { _D = SLL( RM(EA) ); WM( EA,_D );						} /* SLL  D=(XY+o)	  */
OP(xxcb,33) { _E = SLL( RM(EA) ); WM( EA,_E );						} /* SLL  E=(XY+o)	  */
OP(xxcb,34) { _H = SLL( RM(EA) ); WM( EA,_H );						} /* SLL  H=(XY+o)	  */
OP(xxcb,35) { _L = SLL( RM(EA) ); WM( EA,_L );						} /* SLL  L=(XY+o)	  */
OP(xxcb,36) { WM( EA,SLL( RM(EA) ) );								} /* SLL  (XY+o)	  */
OP(xxcb,37) { _A = SLL( RM(EA) ); WM( EA,_A );						} /* SLL  A=(XY+o)	  */

OP(xxcb,38) { _B = SRL( RM(EA) ); WM( EA,_B );						} /* SRL  B=(XY+o)	  */
OP(xxcb,39) { _C = SRL( RM(EA) ); WM( EA,_C );						} /* SRL  C=(XY+o)	  */
OP(xxcb,3a) { _D = SRL( RM(EA) ); WM( EA,_D );						} /* SRL  D=(XY+o)	  */
OP(xxcb,3b) { _E = SRL( RM(EA) ); WM( EA,_E );						} /* SRL  E=(XY+o)	  */
OP(xxcb,3c) { _H = SRL( RM(EA) ); WM( EA,_H );						} /* SRL  H=(XY+o)	  */
OP(xxcb,3d) { _L = SRL( RM(EA) ); WM( EA,_L );						} /* SRL  L=(XY+o)	  */
OP(xxcb,3e) { WM( EA,SRL( RM(EA) ) );								} /* SRL  (XY+o)	  */
OP(xxcb,3f) { _A = SRL( RM(EA) ); WM( EA,_A );						} /* SRL  A=(XY+o)	  */

OP(xxcb,40) { _B = RM(EA); BIT(0,_B);								} /* BIT  0,B=(XY+o)  */
OP(xxcb,41) { _C = RM(EA); BIT(0,_C);								} /* BIT  0,C=(XY+o)  */
OP(xxcb,42) { _D = RM(EA); BIT(0,_D);								} /* BIT  0,D=(XY+o)  */
OP(xxcb,43) { _E = RM(EA); BIT(0,_E);								} /* BIT  0,E=(XY+o)  */
OP(xxcb,44) { _H = RM(EA); BIT(0,_H);								} /* BIT  0,H=(XY+o)  */
OP(xxcb,45) { _L = RM(EA); BIT(0,_L);								} /* BIT  0,L=(XY+o)  */
OP(xxcb,46) { BIT(0,RM(EA));										} /* BIT  0,(XY+o)	  */
OP(xxcb,47) { _A = RM(EA); BIT(0,_A);								} /* BIT  0,A=(XY+o)  */

OP(xxcb,48) { _B = RM(EA); BIT(1,_B);								} /* BIT  1,B=(XY+o)  */
OP(xxcb,49) { _C = RM(EA); BIT(1,_C);								} /* BIT  1,C=(XY+o)  */
OP(xxcb,4a) { _D = RM(EA); BIT(1,_D);								} /* BIT  1,D=(XY+o)  */
OP(xxcb,4b) { _E = RM(EA); BIT(1,_E);								} /* BIT  1,E=(XY+o)  */
OP(xxcb,4c) { _H = RM(EA); BIT(1,_H);								} /* BIT  1,H=(XY+o)  */
OP(xxcb,4d) { _L = RM(EA); BIT(1,_L);								} /* BIT  1,L=(XY+o)  */
OP(xxcb,4e) { BIT(1,RM(EA));										} /* BIT  1,(XY+o)	  */
OP(xxcb,4f) { _A = RM(EA); BIT(1,_A);								} /* BIT  1,A=(XY+o)  */

OP(xxcb,50) { _B = RM(EA); BIT(2,_B);								} /* BIT  2,B=(XY+o)  */
OP(xxcb,51) { _C = RM(EA); BIT(2,_C);								} /* BIT  2,C=(XY+o)  */
OP(xxcb,52) { _D = RM(EA); BIT(2,_D);								} /* BIT  2,D=(XY+o)  */
OP(xxcb,53) { _E = RM(EA); BIT(2,_E);								} /* BIT  2,E=(XY+o)  */
OP(xxcb,54) { _H = RM(EA); BIT(2,_H);								} /* BIT  2,H=(XY+o)  */
OP(xxcb,55) { _L = RM(EA); BIT(2,_L);								} /* BIT  2,L=(XY+o)  */
OP(xxcb,56) { BIT(2,RM(EA));										} /* BIT  2,(XY+o)	  */
OP(xxcb,57) { _A = RM(EA); BIT(2,_A);								} /* BIT  2,A=(XY+o)  */

OP(xxcb,58) { _B = RM(EA); BIT(3,_B);								} /* BIT  3,B=(XY+o)  */
OP(xxcb,59) { _C = RM(EA); BIT(3,_C);								} /* BIT  3,C=(XY+o)  */
OP(xxcb,5a) { _D = RM(EA); BIT(3,_D);								} /* BIT  3,D=(XY+o)  */
OP(xxcb,5b) { _E = RM(EA); BIT(3,_E);								} /* BIT  3,E=(XY+o)  */
OP(xxcb,5c) { _H = RM(EA); BIT(3,_H);								} /* BIT  3,H=(XY+o)  */
OP(xxcb,5d) { _L = RM(EA); BIT(3,_L);								} /* BIT  3,L=(XY+o)  */
OP(xxcb,5e) { BIT(3,RM(EA));										} /* BIT  3,(XY+o)	  */
OP(xxcb,5f) { _A = RM(EA); BIT(3,_A);								} /* BIT  3,A=(XY+o)  */

OP(xxcb,60) { _B = RM(EA); BIT(4,_B);								} /* BIT  4,B=(XY+o)  */
OP(xxcb,61) { _C = RM(EA); BIT(4,_C);								} /* BIT  4,C=(XY+o)  */
OP(xxcb,62) { _D = RM(EA); BIT(4,_D);								} /* BIT  4,D=(XY+o)  */
OP(xxcb,63) { _E = RM(EA); BIT(4,_E);								} /* BIT  4,E=(XY+o)  */
OP(xxcb,64) { _H = RM(EA); BIT(4,_H);								} /* BIT  4,H=(XY+o)  */
OP(xxcb,65) { _L = RM(EA); BIT(4,_L);								} /* BIT  4,L=(XY+o)  */
OP(xxcb,66) { BIT(4,RM(EA));										} /* BIT  4,(XY+o)	  */
OP(xxcb,67) { _A = RM(EA); BIT(4,_A);								} /* BIT  4,A=(XY+o)  */

OP(xxcb,68) { _B = RM(EA); BIT(5,_B);								} /* BIT  5,B=(XY+o)  */
OP(xxcb,69) { _C = RM(EA); BIT(5,_C);								} /* BIT  5,C=(XY+o)  */
OP(xxcb,6a) { _D = RM(EA); BIT(5,_D);								} /* BIT  5,D=(XY+o)  */
OP(xxcb,6b) { _E = RM(EA); BIT(5,_E);								} /* BIT  5,E=(XY+o)  */
OP(xxcb,6c) { _H = RM(EA); BIT(5,_H);								} /* BIT  5,H=(XY+o)  */
OP(xxcb,6d) { _L = RM(EA); BIT(5,_L);								} /* BIT  5,L=(XY+o)  */
OP(xxcb,6e) { BIT(5,RM(EA));										} /* BIT  5,(XY+o)	  */
OP(xxcb,6f) { _A = RM(EA); BIT(5,_A);								} /* BIT  5,A=(XY+o)  */

OP(xxcb,70) { _B = RM(EA); BIT(6,_B);								} /* BIT  6,B=(XY+o)  */
OP(xxcb,71) { _C = RM(EA); BIT(6,_C);								} /* BIT  6,C=(XY+o)  */
OP(xxcb,72) { _D = RM(EA); BIT(6,_D);								} /* BIT  6,D=(XY+o)  */
OP(xxcb,73) { _E = RM(EA); BIT(6,_E);								} /* BIT  6,E=(XY+o)  */
OP(xxcb,74) { _H = RM(EA); BIT(6,_H);								} /* BIT  6,H=(XY+o)  */
OP(xxcb,75) { _L = RM(EA); BIT(6,_L);								} /* BIT  6,L=(XY+o)  */
OP(xxcb,76) { BIT(6,RM(EA));										} /* BIT  6,(XY+o)	  */
OP(xxcb,77) { _A = RM(EA); BIT(6,_A);								} /* BIT  6,A=(XY+o)  */

OP(xxcb,78) { _B = RM(EA); BIT(7,_B);								} /* BIT  7,B=(XY+o)  */
OP(xxcb,79) { _C = RM(EA); BIT(7,_C);								} /* BIT  7,C=(XY+o)  */
OP(xxcb,7a) { _D = RM(EA); BIT(7,_D);								} /* BIT  7,D=(XY+o)  */
OP(xxcb,7b) { _E = RM(EA); BIT(7,_E);								} /* BIT  7,E=(XY+o)  */
OP(xxcb,7c) { _H = RM(EA); BIT(7,_H);								} /* BIT  7,H=(XY+o)  */
OP(xxcb,7d) { _L = RM(EA); BIT(7,_L);								} /* BIT  7,L=(XY+o)  */
OP(xxcb,7e) { BIT(7,RM(EA));										} /* BIT  7,(XY+o)	  */
OP(xxcb,7f) { _A = RM(EA); BIT(7,_A);								} /* BIT  7,A=(XY+o)  */

OP(xxcb,80) { _B = RES(0, RM(EA) ); WM( EA,_B );					} /* RES  0,B=(XY+o)  */
OP(xxcb,81) { _C = RES(0, RM(EA) ); WM( EA,_C );					} /* RES  0,C=(XY+o)  */
OP(xxcb,82) { _D = RES(0, RM(EA) ); WM( EA,_D );					} /* RES  0,D=(XY+o)  */
OP(xxcb,83) { _E = RES(0, RM(EA) ); WM( EA,_E );					} /* RES  0,E=(XY+o)  */
OP(xxcb,84) { _H = RES(0, RM(EA) ); WM( EA,_H );					} /* RES  0,H=(XY+o)  */
OP(xxcb,85) { _L = RES(0, RM(EA) ); WM( EA,_L );					} /* RES  0,L=(XY+o)  */
OP(xxcb,86) { WM( EA, RES(0,RM(EA)) );								} /* RES  0,(XY+o)	  */
OP(xxcb,87) { _A = RES(0, RM(EA) ); WM( EA,_A );					} /* RES  0,A=(XY+o)  */

OP(xxcb,88) { _B = RES(1, RM(EA) ); WM( EA,_B );					} /* RES  1,B=(XY+o)  */
OP(xxcb,89) { _C = RES(1, RM(EA) ); WM( EA,_C );					} /* RES  1,C=(XY+o)  */
OP(xxcb,8a) { _D = RES(1, RM(EA) ); WM( EA,_D );					} /* RES  1,D=(XY+o)  */
OP(xxcb,8b) { _E = RES(1, RM(EA) ); WM( EA,_E );					} /* RES  1,E=(XY+o)  */
OP(xxcb,8c) { _H = RES(1, RM(EA) ); WM( EA,_H );					} /* RES  1,H=(XY+o)  */
OP(xxcb,8d) { _L = RES(1, RM(EA) ); WM( EA,_L );					} /* RES  1,L=(XY+o)  */
OP(xxcb,8e) { WM( EA, RES(1,RM(EA)) );								} /* RES  1,(XY+o)	  */
OP(xxcb,8f) { _A = RES(1, RM(EA) ); WM( EA,_A );					} /* RES  1,A=(XY+o)  */

OP(xxcb,90) { _B = RES(2, RM(EA) ); WM( EA,_B );					} /* RES  2,B=(XY+o)  */
OP(xxcb,91) { _C = RES(2, RM(EA) ); WM( EA,_C );					} /* RES  2,C=(XY+o)  */
OP(xxcb,92) { _D = RES(2, RM(EA) ); WM( EA,_D );					} /* RES  2,D=(XY+o)  */
OP(xxcb,93) { _E = RES(2, RM(EA) ); WM( EA,_E );					} /* RES  2,E=(XY+o)  */
OP(xxcb,94) { _H = RES(2, RM(EA) ); WM( EA,_H );					} /* RES  2,H=(XY+o)  */
OP(xxcb,95) { _L = RES(2, RM(EA) ); WM( EA,_L );					} /* RES  2,L=(XY+o)  */
OP(xxcb,96) { WM( EA, RES(2,RM(EA)) );								} /* RES  2,(XY+o)	  */
OP(xxcb,97) { _A = RES(2, RM(EA) ); WM( EA,_A );					} /* RES  2,A=(XY+o)  */

OP(xxcb,98) { _B = RES(3, RM(EA) ); WM( EA,_B );					} /* RES  3,B=(XY+o)  */
OP(xxcb,99) { _C = RES(3, RM(EA) ); WM( EA,_C );					} /* RES  3,C=(XY+o)  */
OP(xxcb,9a) { _D = RES(3, RM(EA) ); WM( EA,_D );					} /* RES  3,D=(XY+o)  */
OP(xxcb,9b) { _E = RES(3, RM(EA) ); WM( EA,_E );					} /* RES  3,E=(XY+o)  */
OP(xxcb,9c) { _H = RES(3, RM(EA) ); WM( EA,_H );					} /* RES  3,H=(XY+o)  */
OP(xxcb,9d) { _L = RES(3, RM(EA) ); WM( EA,_L );					} /* RES  3,L=(XY+o)  */
OP(xxcb,9e) { WM( EA, RES(3,RM(EA)) );								} /* RES  3,(XY+o)	  */
OP(xxcb,9f) { _A = RES(3, RM(EA) ); WM( EA,_A );					} /* RES  3,A=(XY+o)  */

OP(xxcb,a0) { _B = RES(4, RM(EA) ); WM( EA,_B );					} /* RES  4,B=(XY+o)  */
OP(xxcb,a1) { _C = RES(4, RM(EA) ); WM( EA,_C );					} /* RES  4,C=(XY+o)  */
OP(xxcb,a2) { _D = RES(4, RM(EA) ); WM( EA,_D );					} /* RES  4,D=(XY+o)  */
OP(xxcb,a3) { _E = RES(4, RM(EA) ); WM( EA,_E );					} /* RES  4,E=(XY+o)  */
OP(xxcb,a4) { _H = RES(4, RM(EA) ); WM( EA,_H );					} /* RES  4,H=(XY+o)  */
OP(xxcb,a5) { _L = RES(4, RM(EA) ); WM( EA,_L );					} /* RES  4,L=(XY+o)  */
OP(xxcb,a6) { WM( EA, RES(4,RM(EA)) );								} /* RES  4,(XY+o)	  */
OP(xxcb,a7) { _A = RES(4, RM(EA) ); WM( EA,_A );					} /* RES  4,A=(XY+o)  */

OP(xxcb,a8) { _B = RES(5, RM(EA) ); WM( EA,_B );					} /* RES  5,B=(XY+o)  */
OP(xxcb,a9) { _C = RES(5, RM(EA) ); WM( EA,_C );					} /* RES  5,C=(XY+o)  */
OP(xxcb,aa) { _D = RES(5, RM(EA) ); WM( EA,_D );					} /* RES  5,D=(XY+o)  */
OP(xxcb,ab) { _E = RES(5, RM(EA) ); WM( EA,_E );					} /* RES  5,E=(XY+o)  */
OP(xxcb,ac) { _H = RES(5, RM(EA) ); WM( EA,_H );					} /* RES  5,H=(XY+o)  */
OP(xxcb,ad) { _L = RES(5, RM(EA) ); WM( EA,_L );					} /* RES  5,L=(XY+o)  */
OP(xxcb,ae) { WM( EA, RES(5,RM(EA)) );								} /* RES  5,(XY+o)	  */
OP(xxcb,af) { _A = RES(5, RM(EA) ); WM( EA,_A );					} /* RES  5,A=(XY+o)  */

OP(xxcb,b0) { _B = RES(6, RM(EA) ); WM( EA,_B );					} /* RES  6,B=(XY+o)  */
OP(xxcb,b1) { _C = RES(6, RM(EA) ); WM( EA,_C );					} /* RES  6,C=(XY+o)  */
OP(xxcb,b2) { _D = RES(6, RM(EA) ); WM( EA,_D );					} /* RES  6,D=(XY+o)  */
OP(xxcb,b3) { _E = RES(6, RM(EA) ); WM( EA,_E );					} /* RES  6,E=(XY+o)  */
OP(xxcb,b4) { _H = RES(6, RM(EA) ); WM( EA,_H );					} /* RES  6,H=(XY+o)  */
OP(xxcb,b5) { _L = RES(6, RM(EA) ); WM( EA,_L );					} /* RES  6,L=(XY+o)  */
OP(xxcb,b6) { WM( EA, RES(6,RM(EA)) );								} /* RES  6,(XY+o)	  */
OP(xxcb,b7) { _A = RES(6, RM(EA) ); WM( EA,_A );					} /* RES  6,A=(XY+o)  */

OP(xxcb,b8) { _B = RES(7, RM(EA) ); WM( EA,_B );					} /* RES  7,B=(XY+o)  */
OP(xxcb,b9) { _C = RES(7, RM(EA) ); WM( EA,_C );					} /* RES  7,C=(XY+o)  */
OP(xxcb,ba) { _D = RES(7, RM(EA) ); WM( EA,_D );					} /* RES  7,D=(XY+o)  */
OP(xxcb,bb) { _E = RES(7, RM(EA) ); WM( EA,_E );					} /* RES  7,E=(XY+o)  */
OP(xxcb,bc) { _H = RES(7, RM(EA) ); WM( EA,_H );					} /* RES  7,H=(XY+o)  */
OP(xxcb,bd) { _L = RES(7, RM(EA) ); WM( EA,_L );					} /* RES  7,L=(XY+o)  */
OP(xxcb,be) { WM( EA, RES(7,RM(EA)) );								} /* RES  7,(XY+o)	  */
OP(xxcb,bf) { _A = RES(7, RM(EA) ); WM( EA,_A );					} /* RES  7,A=(XY+o)  */

OP(xxcb,c0) { _B = SET(0, RM(EA) ); WM( EA,_B );					} /* SET  0,B=(XY+o)  */
OP(xxcb,c1) { _C = SET(0, RM(EA) ); WM( EA,_C );					} /* SET  0,C=(XY+o)  */
OP(xxcb,c2) { _D = SET(0, RM(EA) ); WM( EA,_D );					} /* SET  0,D=(XY+o)  */
OP(xxcb,c3) { _E = SET(0, RM(EA) ); WM( EA,_E );					} /* SET  0,E=(XY+o)  */
OP(xxcb,c4) { _H = SET(0, RM(EA) ); WM( EA,_H );					} /* SET  0,H=(XY+o)  */
OP(xxcb,c5) { _L = SET(0, RM(EA) ); WM( EA,_L );					} /* SET  0,L=(XY+o)  */
OP(xxcb,c6) { WM( EA, SET(0,RM(EA)) );								} /* SET  0,(XY+o)	  */
OP(xxcb,c7) { _A = SET(0, RM(EA) ); WM( EA,_A );					} /* SET  0,A=(XY+o)  */

OP(xxcb,c8) { _B = SET(1, RM(EA) ); WM( EA,_B );					} /* SET  1,B=(XY+o)  */
OP(xxcb,c9) { _C = SET(1, RM(EA) ); WM( EA,_C );					} /* SET  1,C=(XY+o)  */
OP(xxcb,ca) { _D = SET(1, RM(EA) ); WM( EA,_D );					} /* SET  1,D=(XY+o)  */
OP(xxcb,cb) { _E = SET(1, RM(EA) ); WM( EA,_E );					} /* SET  1,E=(XY+o)  */
OP(xxcb,cc) { _H = SET(1, RM(EA) ); WM( EA,_H );					} /* SET  1,H=(XY+o)  */
OP(xxcb,cd) { _L = SET(1, RM(EA) ); WM( EA,_L );					} /* SET  1,L=(XY+o)  */
OP(xxcb,ce) { WM( EA, SET(1,RM(EA)) );								} /* SET  1,(XY+o)	  */
OP(xxcb,cf) { _A = SET(1, RM(EA) ); WM( EA,_A );					} /* SET  1,A=(XY+o)  */

OP(xxcb,d0) { _B = SET(2, RM(EA) ); WM( EA,_B );					} /* SET  2,B=(XY+o)  */
OP(xxcb,d1) { _C = SET(2, RM(EA) ); WM( EA,_C );					} /* SET  2,C=(XY+o)  */
OP(xxcb,d2) { _D = SET(2, RM(EA) ); WM( EA,_D );					} /* SET  2,D=(XY+o)  */
OP(xxcb,d3) { _E = SET(2, RM(EA) ); WM( EA,_E );					} /* SET  2,E=(XY+o)  */
OP(xxcb,d4) { _H = SET(2, RM(EA) ); WM( EA,_H );					} /* SET  2,H=(XY+o)  */
OP(xxcb,d5) { _L = SET(2, RM(EA) ); WM( EA,_L );					} /* SET  2,L=(XY+o)  */
OP(xxcb,d6) { WM( EA, SET(2,RM(EA)) );								} /* SET  2,(XY+o)	  */
OP(xxcb,d7) { _A = SET(2, RM(EA) ); WM( EA,_A );					} /* SET  2,A=(XY+o)  */

OP(xxcb,d8) { _B = SET(3, RM(EA) ); WM( EA,_B );					} /* SET  3,B=(XY+o)  */
OP(xxcb,d9) { _C = SET(3, RM(EA) ); WM( EA,_C );					} /* SET  3,C=(XY+o)  */
OP(xxcb,da) { _D = SET(3, RM(EA) ); WM( EA,_D );					} /* SET  3,D=(XY+o)  */
OP(xxcb,db) { _E = SET(3, RM(EA) ); WM( EA,_E );					} /* SET  3,E=(XY+o)  */
OP(xxcb,dc) { _H = SET(3, RM(EA) ); WM( EA,_H );					} /* SET  3,H=(XY+o)  */
OP(xxcb,dd) { _L = SET(3, RM(EA) ); WM( EA,_L );					} /* SET  3,L=(XY+o)  */
OP(xxcb,de) { WM( EA, SET(3,RM(EA)) );								} /* SET  3,(XY+o)	  */
OP(xxcb,df) { _A = SET(3, RM(EA) ); WM( EA,_A );					} /* SET  3,A=(XY+o)  */

OP(xxcb,e0) { _B = SET(4, RM(EA) ); WM( EA,_B );					} /* SET  4,B=(XY+o)  */
OP(xxcb,e1) { _C = SET(4, RM(EA) ); WM( EA,_C );					} /* SET  4,C=(XY+o)  */
OP(xxcb,e2) { _D = SET(4, RM(EA) ); WM( EA,_D );					} /* SET  4,D=(XY+o)  */
OP(xxcb,e3) { _E = SET(4, RM(EA) ); WM( EA,_E );					} /* SET  4,E=(XY+o)  */
OP(xxcb,e4) { _H = SET(4, RM(EA) ); WM( EA,_H );					} /* SET  4,H=(XY+o)  */
OP(xxcb,e5) { _L = SET(4, RM(EA) ); WM( EA,_L );					} /* SET  4,L=(XY+o)  */
OP(xxcb,e6) { WM( EA, SET(4,RM(EA)) );								} /* SET  4,(XY+o)	  */
OP(xxcb,e7) { _A = SET(4, RM(EA) ); WM( EA,_A );					} /* SET  4,A=(XY+o)  */

OP(xxcb,e8) { _B = SET(5, RM(EA) ); WM( EA,_B );					} /* SET  5,B=(XY+o)  */
OP(xxcb,e9) { _C = SET(5, RM(EA) ); WM( EA,_C );					} /* SET  5,C=(XY+o)  */
OP(xxcb,ea) { _D = SET(5, RM(EA) ); WM( EA,_D );					} /* SET  5,D=(XY+o)  */
OP(xxcb,eb) { _E = SET(5, RM(EA) ); WM( EA,_E );					} /* SET  5,E=(XY+o)  */
OP(xxcb,ec) { _H = SET(5, RM(EA) ); WM( EA,_H );					} /* SET  5,H=(XY+o)  */
OP(xxcb,ed) { _L = SET(5, RM(EA) ); WM( EA,_L );					} /* SET  5,L=(XY+o)  */
OP(xxcb,ee) { WM( EA, SET(5,RM(EA)) );								} /* SET  5,(XY+o)	  */
OP(xxcb,ef) { _A = SET(5, RM(EA) ); WM( EA,_A );					} /* SET  5,A=(XY+o)  */

OP(xxcb,f0) { _B = SET(6, RM(EA) ); WM( EA,_B );					} /* SET  6,B=(XY+o)  */
OP(xxcb,f1) { _C = SET(6, RM(EA) ); WM( EA,_C );					} /* SET  6,C=(XY+o)  */
OP(xxcb,f2) { _D = SET(6, RM(EA) ); WM( EA,_D );					} /* SET  6,D=(XY+o)  */
OP(xxcb,f3) { _E = SET(6, RM(EA) ); WM( EA,_E );					} /* SET  6,E=(XY+o)  */
OP(xxcb,f4) { _H = SET(6, RM(EA) ); WM( EA,_H );					} /* SET  6,H=(XY+o)  */
OP(xxcb,f5) { _L = SET(6, RM(EA) ); WM( EA,_L );					} /* SET  6,L=(XY+o)  */
OP(xxcb,f6) { WM( EA, SET(6,RM(EA)) );								} /* SET  6,(XY+o)	  */
OP(xxcb,f7) { _A = SET(6, RM(EA) ); WM( EA,_A );					} /* SET  6,A=(XY+o)  */

OP(xxcb,f8) { _B = SET(7, RM(EA) ); WM( EA,_B );					} /* SET  7,B=(XY+o)  */
OP(xxcb,f9) { _C = SET(7, RM(EA) ); WM( EA,_C );					} /* SET  7,C=(XY+o)  */
OP(xxcb,fa) { _D = SET(7, RM(EA) ); WM( EA,_D );					} /* SET  7,D=(XY+o)  */
OP(xxcb,fb) { _E = SET(7, RM(EA) ); WM( EA,_E );					} /* SET  7,E=(XY+o)  */
OP(xxcb,fc) { _H = SET(7, RM(EA) ); WM( EA,_H );					} /* SET  7,H=(XY+o)  */
OP(xxcb,fd) { _L = SET(7, RM(EA) ); WM( EA,_L );					} /* SET  7,L=(XY+o)  */
OP(xxcb,fe) { WM( EA, SET(7,RM(EA)) );								} /* SET  7,(XY+o)	  */
OP(xxcb,ff) { _A = SET(7, RM(EA) ); WM( EA,_A );					} /* SET  7,A=(XY+o)  */

OP(illegal,1) {
	_PC--;
} 

/**********************************************************
 * IX register related opcodes (DD prefix)
 **********************************************************/
#define dd_00 illegal_1 											  /* DB   DD		  */
#define dd_01 illegal_1 											  /* DB   DD		  */
#define dd_02 illegal_1 											  /* DB   DD		  */
#define dd_03 illegal_1 											  /* DB   DD		  */
#define dd_04 illegal_1 											  /* DB   DD		  */
#define dd_05 illegal_1 											  /* DB   DD		  */
#define dd_06 illegal_1 											  /* DB   DD		  */
#define dd_07 illegal_1 											  /* DB   DD		  */

#define dd_08 illegal_1 											  /* DB   DD		  */
OP(dd,09) { ADD16(IX,BC);											} /* ADD  IX,BC 	  */
#define dd_0a illegal_1 											  /* DB   DD		  */
#define dd_0b illegal_1 											  /* DB   DD		  */
#define dd_0c illegal_1 											  /* DB   DD		  */
#define dd_0d illegal_1 											  /* DB   DD		  */
#define dd_0e illegal_1 											  /* DB   DD		  */
#define dd_0f illegal_1 											  /* DB   DD		  */

#define dd_10 illegal_1 											  /* DB   DD		  */
#define dd_11 illegal_1 											  /* DB   DD		  */
#define dd_12 illegal_1 											  /* DB   DD		  */
#define dd_13 illegal_1 											  /* DB   DD		  */
#define dd_14 illegal_1 											  /* DB   DD		  */
#define dd_15 illegal_1 											  /* DB   DD		  */
#define dd_16 illegal_1 											  /* DB   DD		  */
#define dd_17 illegal_1 											  /* DB   DD		  */

#define dd_18 illegal_1 											  /* DB   DD		  */
OP(dd,19) { ADD16(IX,DE);											} /* ADD  IX,DE 	  */
#define dd_1a illegal_1 											  /* DB   DD		  */
#define dd_1b illegal_1 											  /* DB   DD		  */
#define dd_1c illegal_1 											  /* DB   DD		  */
#define dd_1d illegal_1 											  /* DB   DD		  */
#define dd_1e illegal_1 											  /* DB   DD		  */
#define dd_1f illegal_1 											  /* DB   DD		  */

#define dd_20 illegal_1 											  /* DB   DD		  */
OP(dd,21) { _IX = ARG16();											} /* LD   IX,w		  */
OP(dd,22) { WM16( ARG16(), &Z80.IX );								} /* LD   (w),IX	  */
OP(dd,23) { _IX++;													} /* INC  IX		  */
OP(dd,24) { _HX = INC(_HX); 										} /* INC  HX		  */
OP(dd,25) { _HX = DEC(_HX); 										} /* DEC  HX		  */
OP(dd,26) { _HX = ARG();											} /* LD   HX,n		  */
#define dd_27 illegal_1 											  /* DB   DD		  */

#define dd_28 illegal_1 											  /* DB   DD		  */
OP(dd,29) { ADD16(IX,IX);											} /* ADD  IX,IX 	  */
OP(dd,2a) { _IX = RM16( ARG16() );									} /* LD   IX,(w)	  */
OP(dd,2b) { _IX--;													} /* DEC  IX		  */
OP(dd,2c) { _LX = INC(_LX); 										} /* INC  LX		  */
OP(dd,2d) { _LX = DEC(_LX); 										} /* DEC  LX		  */
OP(dd,2e) { _LX = ARG();											} /* LD   LX,n		  */
#define dd_2f illegal_1 											  /* DB   DD		  */

#define dd_30 illegal_1 											  /* DB   DD		  */
#define dd_31 illegal_1 											  /* DB   DD		  */
#define dd_32 illegal_1 											  /* DB   DD		  */
#define dd_33 illegal_1 											  /* DB   DD		  */
OP(dd,34) { EAX; WM( EA, INC(RM(EA)) ); 							} /* INC  (IX+o)	  */
OP(dd,35) { EAX; WM( EA, DEC(RM(EA)) ); 							} /* DEC  (IX+o)	  */
OP(dd,36) { EAX; WM( EA, ARG() );									} /* LD   (IX+o),n	  */
#define dd_37 illegal_1 											  /* DB   DD		  */

#define dd_38 illegal_1 											  /* DB   DD		  */
OP(dd,39) { ADD16(IX,SP);											} /* ADD  IX,SP 	  */
#define dd_3a illegal_1 											  /* DB   DD		  */
#define dd_3b illegal_1 											  /* DB   DD		  */
#define dd_3c illegal_1 											  /* DB   DD		  */
#define dd_3d illegal_1 											  /* DB   DD		  */
#define dd_3e illegal_1 											  /* DB   DD		  */
#define dd_3f illegal_1 											  /* DB   DD		  */

#define dd_40 illegal_1 											  /* DB   DD		  */
#define dd_41 illegal_1 											  /* DB   DD		  */
#define dd_42 illegal_1 											  /* DB   DD		  */
#define dd_43 illegal_1 											  /* DB   DD		  */
OP(dd,44) { _B = _HX;												} /* LD   B,HX		  */
OP(dd,45) { _B = _LX;												} /* LD   B,LX		  */
OP(dd,46) { EAX; _B = RM(EA);										} /* LD   B,(IX+o)	  */
#define dd_47 illegal_1 											  /* DB   DD		  */

#define dd_48 illegal_1 											  /* DB   DD		  */
#define dd_49 illegal_1 											  /* DB   DD		  */
#define dd_4a illegal_1 											  /* DB   DD		  */
#define dd_4b illegal_1 											  /* DB   DD		  */
OP(dd,4c) { _C = _HX;												} /* LD   C,HX		  */
OP(dd,4d) { _C = _LX;												} /* LD   C,LX		  */
OP(dd,4e) { EAX; _C = RM(EA);										} /* LD   C,(IX+o)	  */
#define dd_4f illegal_1 											  /* DB   DD		  */

#define dd_50 illegal_1 											  /* DB   DD		  */
#define dd_51 illegal_1 											  /* DB   DD		  */
#define dd_52 illegal_1 											  /* DB   DD		  */
#define dd_53 illegal_1 											  /* DB   DD		  */
OP(dd,54) { _D = _HX;												} /* LD   D,HX		  */
OP(dd,55) { _D = _LX;												} /* LD   D,LX		  */
OP(dd,56) { EAX; _D = RM(EA);										} /* LD   D,(IX+o)	  */
#define dd_57 illegal_1 											  /* DB   DD		  */

#define dd_58 illegal_1 											  /* DB   DD		  */
#define dd_59 illegal_1 											  /* DB   DD		  */
#define dd_5a illegal_1 											  /* DB   DD		  */
#define dd_5b illegal_1 											  /* DB   DD		  */
OP(dd,5c) { _E = _HX;												} /* LD   E,HX		  */
OP(dd,5d) { _E = _LX;												} /* LD   E,LX		  */
OP(dd,5e) { EAX; _E = RM(EA);										} /* LD   E,(IX+o)	  */
#define dd_5f illegal_1 											  /* DB   DD		  */

OP(dd,60) { _HX = _B;												} /* LD   HX,B		  */
OP(dd,61) { _HX = _C;												} /* LD   HX,C		  */
OP(dd,62) { _HX = _D;												} /* LD   HX,D		  */
OP(dd,63) { _HX = _E;												} /* LD   HX,E		  */
OP(dd,64) { 														} /* LD   HX,HX 	  */
OP(dd,65) { _HX = _LX;												} /* LD   HX,LX 	  */
OP(dd,66) { EAX; _H = RM(EA);										} /* LD   H,(IX+o)	  */
OP(dd,67) { _HX = _A;												} /* LD   HX,A		  */

OP(dd,68) { _LX = _B;												} /* LD   LX,B		  */
OP(dd,69) { _LX = _C;												} /* LD   LX,C		  */
OP(dd,6a) { _LX = _D;												} /* LD   LX,D		  */
OP(dd,6b) { _LX = _E;												} /* LD   LX,E		  */
OP(dd,6c) { _LX = _HX;												} /* LD   LX,HX 	  */
OP(dd,6d) { 														} /* LD   LX,LX 	  */
OP(dd,6e) { EAX; _L = RM(EA);										} /* LD   L,(IX+o)	  */
OP(dd,6f) { _LX = _A;												} /* LD   LX,A		  */

OP(dd,70) { EAX; WM( EA, _B );										} /* LD   (IX+o),B	  */
OP(dd,71) { EAX; WM( EA, _C );										} /* LD   (IX+o),C	  */
OP(dd,72) { EAX; WM( EA, _D );										} /* LD   (IX+o),D	  */
OP(dd,73) { EAX; WM( EA, _E );										} /* LD   (IX+o),E	  */
OP(dd,74) { EAX; WM( EA, _H );										} /* LD   (IX+o),H	  */
OP(dd,75) { EAX; WM( EA, _L );										} /* LD   (IX+o),L	  */
#define dd_76 illegal_1 													  /* DB   DD		  */
OP(dd,77) { EAX; WM( EA, _A );										} /* LD   (IX+o),A	  */

#define dd_78 illegal_1 											  /* DB   DD		  */
#define dd_79 illegal_1 											  /* DB   DD		  */
#define dd_7a illegal_1 											  /* DB   DD		  */
#define dd_7b illegal_1 											  /* DB   DD		  */
OP(dd,7c) { _A = _HX;												} /* LD   A,HX		  */
OP(dd,7d) { _A = _LX;												} /* LD   A,LX		  */
OP(dd,7e) { EAX; _A = RM(EA);										} /* LD   A,(IX+o)	  */
#define dd_7f illegal_1 											  /* DB   DD		  */

#define dd_80 illegal_1 											  /* DB   DD		  */
#define dd_81 illegal_1 											  /* DB   DD		  */
#define dd_82 illegal_1 											  /* DB   DD		  */
#define dd_83 illegal_1 											  /* DB   DD		  */
OP(dd,84) { ADD(_HX);												} /* ADD  A,HX		  */
OP(dd,85) { ADD(_LX);												} /* ADD  A,LX		  */
OP(dd,86) { EAX; ADD(RM(EA));										} /* ADD  A,(IX+o)	  */
#define dd_87 illegal_1 											  /* DB   DD		  */

#define dd_88 illegal_1 											  /* DB   DD		  */
#define dd_89 illegal_1 											  /* DB   DD		  */
#define dd_8a illegal_1 											  /* DB   DD		  */
#define dd_8b illegal_1 											  /* DB   DD		  */
OP(dd,8c) { ADC(_HX);												} /* ADC  A,HX		  */
OP(dd,8d) { ADC(_LX);												} /* ADC  A,LX		  */
OP(dd,8e) { EAX; ADC(RM(EA));										} /* ADC  A,(IX+o)	  */
#define dd_8f illegal_1 											  /* DB   DD		  */

#define dd_90 illegal_1 											  /* DB   DD		  */
#define dd_91 illegal_1 											  /* DB   DD		  */
#define dd_92 illegal_1 											  /* DB   DD		  */
#define dd_93 illegal_1 											  /* DB   DD		  */
OP(dd,94) { SUB(_HX);												} /* SUB  HX		  */
OP(dd,95) { SUB(_LX);												} /* SUB  LX		  */
OP(dd,96) { EAX; SUB(RM(EA));										} /* SUB  (IX+o)	  */
#define dd_97 illegal_1 											  /* DB   DD		  */

#define dd_98 illegal_1 											  /* DB   DD		  */
#define dd_99 illegal_1 											  /* DB   DD		  */
#define dd_9a illegal_1 											  /* DB   DD		  */
#define dd_9b illegal_1 											  /* DB   DD		  */
OP(dd,9c) { SBC(_HX);												} /* SBC  A,HX		  */
OP(dd,9d) { SBC(_LX);												} /* SBC  A,LX		  */
OP(dd,9e) { EAX; SBC(RM(EA));										} /* SBC  A,(IX+o)	  */
#define dd_9f illegal_1 											  /* DB   DD		  */

#define dd_a0 illegal_1 											  /* DB   DD		  */
#define dd_a1 illegal_1 											  /* DB   DD		  */
#define dd_a2 illegal_1 											  /* DB   DD		  */
#define dd_a3 illegal_1 											  /* DB   DD		  */
OP(dd,a4) { AND(_HX);												} /* AND  HX		  */
OP(dd,a5) { AND(_LX);												} /* AND  LX		  */
OP(dd,a6) { EAX; AND(RM(EA));										} /* AND  (IX+o)	  */
#define dd_a7 illegal_1 											  /* DB   DD		  */

#define dd_a8 illegal_1 											  /* DB   DD		  */
#define dd_a9 illegal_1 											  /* DB   DD		  */
#define dd_aa illegal_1 											  /* DB   DD		  */
#define dd_ab illegal_1 											  /* DB   DD		  */
OP(dd,ac) { XOR(_HX);												} /* XOR  HX		  */
OP(dd,ad) { XOR(_LX);												} /* XOR  LX		  */
OP(dd,ae) { EAX; XOR(RM(EA));										} /* XOR  (IX+o)	  */
#define dd_af illegal_1 											  /* DB   DD		  */

#define dd_b0 illegal_1 											  /* DB   DD		  */
#define dd_b1 illegal_1 											  /* DB   DD		  */
#define dd_b2 illegal_1 											  /* DB   DD		  */
#define dd_b3 illegal_1 											  /* DB   DD		  */
OP(dd,b4) { OR(_HX);												} /* OR   HX		  */
OP(dd,b5) { OR(_LX);												} /* OR   LX		  */
OP(dd,b6) { EAX; OR(RM(EA));										} /* OR   (IX+o)	  */
#define dd_b7 illegal_1 											  /* DB   DD		  */

#define dd_b8 illegal_1 											  /* DB   DD		  */
#define dd_b9 illegal_1 											  /* DB   DD		  */
#define dd_ba illegal_1 											  /* DB   DD		  */
#define dd_bb illegal_1 											  /* DB   DD		  */
OP(dd,bc) { CP(_HX);												} /* CP   HX		  */
OP(dd,bd) { CP(_LX);												} /* CP   LX		  */
OP(dd,be) { EAX; CP(RM(EA));										} /* CP   (IX+o)	  */
#define dd_bf illegal_1 											  /* DB   DD		  */

#define dd_c0 illegal_1 											  /* DB   DD		  */
#define dd_c1 illegal_1 											  /* DB   DD		  */
#define dd_c2 illegal_1 											  /* DB   DD		  */
#define dd_c3 illegal_1 											  /* DB   DD		  */
#define dd_c4 illegal_1 											  /* DB   DD		  */
#define dd_c5 illegal_1 											  /* DB   DD		  */
#define dd_c6 illegal_1 											  /* DB   DD		  */
#define dd_c7 illegal_1 													  /* DB   DD		  */

#define dd_c8 illegal_1 											  /* DB   DD		  */
#define dd_c9 illegal_1 											  /* DB   DD		  */
#define dd_ca illegal_1 											  /* DB   DD		  */
OP(dd,cb) { EAX; EXEC(xxcb,ARG());									} /* **   DD CB xx	  */
#define dd_cc illegal_1 											  /* DB   DD		  */
#define dd_cd illegal_1 											  /* DB   DD		  */
#define dd_ce illegal_1 											  /* DB   DD		  */
#define dd_cf illegal_1 											  /* DB   DD		  */

#define dd_d0 illegal_1 											  /* DB   DD		  */
#define dd_d1 illegal_1 											  /* DB   DD		  */
#define dd_d2 illegal_1 											  /* DB   DD		  */
#define dd_d3 illegal_1 											  /* DB   DD		  */
#define dd_d4 illegal_1 											  /* DB   DD		  */
#define dd_d5 illegal_1 											  /* DB   DD		  */
#define dd_d6 illegal_1 											  /* DB   DD		  */
#define dd_d7 illegal_1 											  /* DB   DD		  */

#define dd_d8 illegal_1 											  /* DB   DD		  */
#define dd_d9 illegal_1 											  /* DB   DD		  */
#define dd_da illegal_1 											  /* DB   DD		  */
#define dd_db illegal_1 											  /* DB   DD		  */
#define dd_dc illegal_1 											  /* DB   DD		  */
#define dd_dd illegal_1 											  /* DB   DD		  */
#define dd_de illegal_1 											  /* DB   DD		  */
#define dd_df illegal_1 											  /* DB   DD		  */

#define dd_e0 illegal_1 											  /* DB   DD		  */
OP(dd,e1) { POP(IX);												} /* POP  IX		  */
#define dd_e2 illegal_1 											  /* DB   DD		  */
OP(dd,e3) { EXSP(IX);												} /* EX   (SP),IX	  */
#define dd_e4 illegal_1 											  /* DB   DD		  */
OP(dd,e5) { PUSH( IX ); 											} /* PUSH IX		  */
#define dd_e6 illegal_1 											  /* DB   DD		  */
#define dd_e7 illegal_1 											  /* DB   DD		  */

#define dd_e8 illegal_1 											  /* DB   DD		  */
OP(dd,e9) { _PC = _IX; change_pc16(_PCD);							} /* JP   (IX)		  */
#define dd_ea illegal_1 											  /* DB   DD		  */
#define dd_eb illegal_1 											  /* DB   DD		  */
#define dd_ec illegal_1 											  /* DB   DD		  */
#define dd_ed illegal_1 											  /* DB   DD		  */
#define dd_ee illegal_1 											  /* DB   DD		  */
#define dd_ef illegal_1 											  /* DB   DD		  */

#define dd_f0 illegal_1 											  /* DB   DD		  */
#define dd_f1 illegal_1 											  /* DB   DD		  */
#define dd_f2 illegal_1 											  /* DB   DD		  */
#define dd_f3 illegal_1 											  /* DB   DD		  */
#define dd_f4 illegal_1 											  /* DB   DD		  */
#define dd_f5 illegal_1 											  /* DB   DD		  */
#define dd_f6 illegal_1 											  /* DB   DD		  */
#define dd_f7 illegal_1 											  /* DB   DD		  */

#define dd_f8 illegal_1 											  /* DB   DD		  */
OP(dd,f9) { _SP = _IX;												} /* LD   SP,IX 	  */
#define dd_fa illegal_1 											  /* DB   DD		  */
#define dd_fb illegal_1 											  /* DB   DD		  */
#define dd_fc illegal_1 											  /* DB   DD		  */
#define dd_fd illegal_1 											  /* DB   DD		  */
#define dd_fe illegal_1 											  /* DB   DD		  */
#define dd_ff illegal_1 											  /* DB   DD		  */

/**********************************************************
 * IY register related opcodes (FD prefix)
 **********************************************************/
#define fd_00 illegal_1 											  /* DB   FD		  */
#define fd_01 illegal_1 											  /* DB   FD		  */
#define fd_02 illegal_1 											  /* DB   FD		  */
#define fd_03 illegal_1 											  /* DB   FD		  */
#define fd_04 illegal_1 											  /* DB   FD		  */
#define fd_05 illegal_1 											  /* DB   FD		  */
#define fd_06 illegal_1 											  /* DB   FD		  */
#define fd_07 illegal_1 											  /* DB   FD		  */

#define fd_08 illegal_1 											  /* DB   FD		  */
OP(fd,09) { ADD16(IY,BC);											} /* ADD  IY,BC 	  */
#define fd_0a illegal_1 											  /* DB   FD		  */
#define fd_0b illegal_1 											  /* DB   FD		  */
#define fd_0c illegal_1 											  /* DB   FD		  */
#define fd_0d illegal_1 											  /* DB   FD		  */
#define fd_0e illegal_1 											  /* DB   FD		  */
#define fd_0f illegal_1 											  /* DB   FD		  */

#define fd_10 illegal_1 											  /* DB   FD		  */
#define fd_11 illegal_1 											  /* DB   FD		  */
#define fd_12 illegal_1 											  /* DB   FD		  */
#define fd_13 illegal_1 											  /* DB   FD		  */
#define fd_14 illegal_1 											  /* DB   FD		  */
#define fd_15 illegal_1 											  /* DB   FD		  */
#define fd_16 illegal_1 											  /* DB   FD		  */
#define fd_17 illegal_1 											  /* DB   FD		  */

#define fd_18 illegal_1 											  /* DB   FD		  */
OP(fd,19) { ADD16(IY,DE);											} /* ADD  IY,DE 	  */
#define fd_1a illegal_1 											  /* DB   FD		  */
#define fd_1b illegal_1 											  /* DB   FD		  */
#define fd_1c illegal_1 											  /* DB   FD		  */
#define fd_1d illegal_1 											  /* DB   FD		  */
#define fd_1e illegal_1 											  /* DB   FD		  */
#define fd_1f illegal_1 											  /* DB   FD		  */

#define fd_20 illegal_1 											  /* DB   FD		  */
OP(fd,21) { _IY = ARG16();											} /* LD   IY,w		  */
OP(fd,22) { WM16( ARG16(), &Z80.IY );								} /* LD   (w),IY	  */
OP(fd,23) { _IY++;													} /* INC  IY		  */
OP(fd,24) { _HY = INC(_HY); 										} /* INC  HY		  */
OP(fd,25) { _HY = DEC(_HY); 										} /* DEC  HY		  */
OP(fd,26) { _HY = ARG();											} /* LD   HY,n		  */
#define fd_27 illegal_1 											  /* DB   FD		  */

#define fd_28 illegal_1 											  /* DB   FD		  */
OP(fd,29) { ADD16(IY,IY);											} /* ADD  IY,IY 	  */
OP(fd,2a) { _IY = RM16( ARG16() );									} /* LD   IY,(w)	  */
OP(fd,2b) { _IY--;													} /* DEC  IY		  */
OP(fd,2c) { _LY = INC(_LY); 										} /* INC  LY		  */
OP(fd,2d) { _LY = DEC(_LY); 										} /* DEC  LY		  */
OP(fd,2e) { _LY = ARG();											} /* LD   LY,n		  */
#define fd_2f illegal_1 											  /* DB   FD		  */

#define fd_30 illegal_1 											  /* DB   FD		  */
#define fd_31 illegal_1 											  /* DB   FD		  */
#define fd_32 illegal_1 											  /* DB   FD		  */
#define fd_33 illegal_1 											  /* DB   FD		  */
OP(fd,34) { EAY; WM( EA, INC(RM(EA)) ); 							} /* INC  (IY+o)	  */
OP(fd,35) { EAY; WM( EA, DEC(RM(EA)) ); 							} /* DEC  (IY+o)	  */
OP(fd,36) { EAY; WM( EA, ARG() );									} /* LD   (IY+o),n	  */
#define fd_37 illegal_1 											  /* DB   FD		  */

#define fd_38 illegal_1 											  /* DB   FD		  */
OP(fd,39) { ADD16(IY,SP);											} /* ADD  IY,SP 	  */
#define fd_3a illegal_1 											  /* DB   FD		  */
#define fd_3b illegal_1 											  /* DB   FD		  */
#define fd_3c illegal_1 											  /* DB   FD		  */
#define fd_3d illegal_1 											  /* DB   FD		  */
#define fd_3e illegal_1 											  /* DB   FD		  */
#define fd_3f illegal_1 											  /* DB   FD		  */

#define fd_40 illegal_1 											  /* DB   FD		  */
#define fd_41 illegal_1 											  /* DB   FD		  */
#define fd_42 illegal_1 											  /* DB   FD		  */
#define fd_43 illegal_1 											  /* DB   FD		  */
OP(fd,44) { _B = _HY;												} /* LD   B,HY		  */
OP(fd,45) { _B = _LY;												} /* LD   B,LY		  */
OP(fd,46) { EAY; _B = RM(EA);										} /* LD   B,(IY+o)	  */
#define fd_47 illegal_1 											  /* DB   FD		  */

#define fd_48 illegal_1 											  /* DB   FD		  */
#define fd_49 illegal_1 											  /* DB   FD		  */
#define fd_4a illegal_1 											  /* DB   FD		  */
#define fd_4b illegal_1 											  /* DB   FD		  */
OP(fd,4c) { _C = _HY;												} /* LD   C,HY		  */
OP(fd,4d) { _C = _LY;												} /* LD   C,LY		  */
OP(fd,4e) { EAY; _C = RM(EA);										} /* LD   C,(IY+o)	  */
#define fd_4f illegal_1 											  /* DB   FD		  */

#define fd_50 illegal_1 											  /* DB   FD		  */
#define fd_51 illegal_1 											  /* DB   FD		  */
#define fd_52 illegal_1 											  /* DB   FD		  */
#define fd_53 illegal_1 											  /* DB   FD		  */
OP(fd,54) { _D = _HY;												} /* LD   D,HY		  */
OP(fd,55) { _D = _LY;												} /* LD   D,LY		  */
OP(fd,56) { EAY; _D = RM(EA);										} /* LD   D,(IY+o)	  */
#define fd_57 illegal_1 											  /* DB   FD		  */

#define fd_58 illegal_1 											  /* DB   FD		  */
#define fd_59 illegal_1 											  /* DB   FD		  */
#define fd_5a illegal_1 											  /* DB   FD		  */
#define fd_5b illegal_1 											  /* DB   FD		  */
OP(fd,5c) { _E = _HY;												} /* LD   E,HY		  */
OP(fd,5d) { _E = _LY;												} /* LD   E,LY		  */
OP(fd,5e) { EAY; _E = RM(EA);										} /* LD   E,(IY+o)	  */
#define fd_5f illegal_1 											  /* DB   FD		  */

OP(fd,60) { _HY = _B;												} /* LD   HY,B		  */
OP(fd,61) { _HY = _C;												} /* LD   HY,C		  */
OP(fd,62) { _HY = _D;												} /* LD   HY,D		  */
OP(fd,63) { _HY = _E;												} /* LD   HY,E		  */
OP(fd,64) { 														} /* LD   HY,HY 	  */
OP(fd,65) { _HY = _LY;												} /* LD   HY,LY 	  */
OP(fd,66) { EAY; _H = RM(EA);										} /* LD   H,(IY+o)	  */
OP(fd,67) { _HY = _A;												} /* LD   HY,A		  */

OP(fd,68) { _LY = _B;												} /* LD   LY,B		  */
OP(fd,69) { _LY = _C;												} /* LD   LY,C		  */
OP(fd,6a) { _LY = _D;												} /* LD   LY,D		  */
OP(fd,6b) { _LY = _E;												} /* LD   LY,E		  */
OP(fd,6c) { _LY = _HY;												} /* LD   LY,HY 	  */
OP(fd,6d) { 														} /* LD   LY,LY 	  */
OP(fd,6e) { EAY; _L = RM(EA);										} /* LD   L,(IY+o)	  */
OP(fd,6f) { _LY = _A;												} /* LD   LY,A		  */

OP(fd,70) { EAY; WM( EA, _B );										} /* LD   (IY+o),B	  */
OP(fd,71) { EAY; WM( EA, _C );										} /* LD   (IY+o),C	  */
OP(fd,72) { EAY; WM( EA, _D );										} /* LD   (IY+o),D	  */
OP(fd,73) { EAY; WM( EA, _E );										} /* LD   (IY+o),E	  */
OP(fd,74) { EAY; WM( EA, _H );										} /* LD   (IY+o),H	  */
OP(fd,75) { EAY; WM( EA, _L );										} /* LD   (IY+o),L	  */
#define fd_76 illegal_1 													  /* DB   FD		  */
OP(fd,77) { EAY; WM( EA, _A );										} /* LD   (IY+o),A	  */

#define fd_78 illegal_1 											  /* DB   FD		  */
#define fd_79 illegal_1 											  /* DB   FD		  */
#define fd_7a illegal_1 											  /* DB   FD		  */
#define fd_7b illegal_1 											  /* DB   FD		  */
OP(fd,7c) { _A = _HY;												} /* LD   A,HY		  */
OP(fd,7d) { _A = _LY;												} /* LD   A,LY		  */
OP(fd,7e) { EAY; _A = RM(EA);										} /* LD   A,(IY+o)	  */
#define fd_7f illegal_1 											  /* DB   FD		  */

#define fd_80 illegal_1 											  /* DB   FD		  */
#define fd_81 illegal_1 											  /* DB   FD		  */
#define fd_82 illegal_1 											  /* DB   FD		  */
#define fd_83 illegal_1 											  /* DB   FD		  */
OP(fd,84) { ADD(_HY);												} /* ADD  A,HY		  */
OP(fd,85) { ADD(_LY);												} /* ADD  A,LY		  */
OP(fd,86) { EAY; ADD(RM(EA));										} /* ADD  A,(IY+o)	  */
#define fd_87 illegal_1 											  /* DB   FD		  */

#define fd_88 illegal_1 											  /* DB   FD		  */
#define fd_89 illegal_1 											  /* DB   FD		  */
#define fd_8a illegal_1 											  /* DB   FD		  */
#define fd_8b illegal_1 											  /* DB   FD		  */
OP(fd,8c) { ADC(_HY);												} /* ADC  A,HY		  */
OP(fd,8d) { ADC(_LY);												} /* ADC  A,LY		  */
OP(fd,8e) { EAY; ADC(RM(EA));										} /* ADC  A,(IY+o)	  */
#define fd_8f illegal_1 											  /* DB   FD		  */

#define fd_90 illegal_1 											  /* DB   FD		  */
#define fd_91 illegal_1 											  /* DB   FD		  */
#define fd_92 illegal_1 											  /* DB   FD		  */
#define fd_93 illegal_1 											  /* DB   FD		  */
OP(fd,94) { SUB(_HY);												} /* SUB  HY		  */
OP(fd,95) { SUB(_LY);												} /* SUB  LY		  */
OP(fd,96) { EAY; SUB(RM(EA));										} /* SUB  (IY+o)	  */
#define fd_97 illegal_1 											  /* DB   FD		  */

#define fd_98 illegal_1 											  /* DB   FD		  */
#define fd_99 illegal_1 											  /* DB   FD		  */
#define fd_9a illegal_1 											  /* DB   FD		  */
#define fd_9b illegal_1 											  /* DB   FD		  */
OP(fd,9c) { SBC(_HY);												} /* SBC  A,HY		  */
OP(fd,9d) { SBC(_LY);												} /* SBC  A,LY		  */
OP(fd,9e) { EAY; SBC(RM(EA));										} /* SBC  A,(IY+o)	  */
#define fd_9f illegal_1 											  /* DB   FD		  */

#define fd_a0 illegal_1 											  /* DB   FD		  */
#define fd_a1 illegal_1 											  /* DB   FD		  */
#define fd_a2 illegal_1 											  /* DB   FD		  */
#define fd_a3 illegal_1 											  /* DB   FD		  */
OP(fd,a4) { AND(_HY);												} /* AND  HY		  */
OP(fd,a5) { AND(_LY);												} /* AND  LY		  */
OP(fd,a6) { EAY; AND(RM(EA));										} /* AND  (IY+o)	  */
#define fd_a7 illegal_1 											  /* DB   FD		  */

#define fd_a8 illegal_1 											  /* DB   FD		  */
#define fd_a9 illegal_1 											  /* DB   FD		  */
#define fd_aa illegal_1 											  /* DB   FD		  */
#define fd_ab illegal_1 											  /* DB   FD		  */
OP(fd,ac) { XOR(_HY);												} /* XOR  HY		  */
OP(fd,ad) { XOR(_LY);												} /* XOR  LY		  */
OP(fd,ae) { EAY; XOR(RM(EA));										} /* XOR  (IY+o)	  */
#define fd_af illegal_1 											  /* DB   FD		  */

#define fd_b0 illegal_1 											  /* DB   FD		  */
#define fd_b1 illegal_1 											  /* DB   FD		  */
#define fd_b2 illegal_1 											  /* DB   FD		  */
#define fd_b3 illegal_1 											  /* DB   FD		  */
OP(fd,b4) { OR(_HY);												} /* OR   HY		  */
OP(fd,b5) { OR(_LY);												} /* OR   LY		  */
OP(fd,b6) { EAY; OR(RM(EA));										} /* OR   (IY+o)	  */
#define fd_b7 illegal_1 											  /* DB   FD		  */

#define fd_b8 illegal_1 											  /* DB   FD		  */
#define fd_b9 illegal_1 											  /* DB   FD		  */
#define fd_ba illegal_1 											  /* DB   FD		  */
#define fd_bb illegal_1 											  /* DB   FD		  */
OP(fd,bc) { CP(_HY);												} /* CP   HY		  */
OP(fd,bd) { CP(_LY);												} /* CP   LY		  */
OP(fd,be) { EAY; CP(RM(EA));										} /* CP   (IY+o)	  */
#define fd_bf illegal_1 											  /* DB   FD		  */

#define fd_c0 illegal_1 											  /* DB   FD		  */
#define fd_c1 illegal_1 											  /* DB   FD		  */
#define fd_c2 illegal_1 											  /* DB   FD		  */
#define fd_c3 illegal_1 											  /* DB   FD		  */
#define fd_c4 illegal_1 											  /* DB   FD		  */
#define fd_c5 illegal_1 											  /* DB   FD		  */
#define fd_c6 illegal_1 											  /* DB   FD		  */
#define fd_c7 illegal_1 											  /* DB   FD		  */

#define fd_c8 illegal_1 											  /* DB   FD		  */
#define fd_c9 illegal_1 											  /* DB   FD		  */
#define fd_ca illegal_1 											  /* DB   FD		  */
OP(fd,cb) { EAY; EXEC(xxcb,ARG());									} /* **   FD CB xx	  */
#define fd_cc illegal_1 											  /* DB   FD		  */
#define fd_cd illegal_1 											  /* DB   FD		  */
#define fd_ce illegal_1 											  /* DB   FD		  */
#define fd_cf illegal_1 											  /* DB   FD		  */

#define fd_d0 illegal_1 											  /* DB   FD		  */
#define fd_d1 illegal_1 											  /* DB   FD		  */
#define fd_d2 illegal_1 											  /* DB   FD		  */
#define fd_d3 illegal_1 											  /* DB   FD		  */
#define fd_d4 illegal_1 											  /* DB   FD		  */
#define fd_d5 illegal_1 											  /* DB   FD		  */
#define fd_d6 illegal_1 											  /* DB   FD		  */
#define fd_d7 illegal_1 											  /* DB   FD		  */

#define fd_d8 illegal_1 											  /* DB   FD		  */
#define fd_d9 illegal_1 											  /* DB   FD		  */
#define fd_da illegal_1 											  /* DB   FD		  */
#define fd_db illegal_1 											  /* DB   FD		  */
#define fd_dc illegal_1 											  /* DB   FD		  */
#define fd_dd illegal_1 											  /* DB   FD		  */
#define fd_de illegal_1 											  /* DB   FD		  */
#define fd_df illegal_1 											  /* DB   FD		  */

#define fd_e0 illegal_1 											  /* DB   FD		  */
OP(fd,e1) { POP(IY);												} /* POP  IY		  */
#define fd_e2 illegal_1 											  /* DB   FD		  */
OP(fd,e3) { EXSP(IY);												} /* EX   (SP),IY	  */
#define fd_e4 illegal_1 											  /* DB   FD		  */
OP(fd,e5) { PUSH( IY ); 											} /* PUSH IY		  */
#define fd_e6 illegal_1 											  /* DB   FD		  */
#define fd_e7 illegal_1 											  /* DB   FD		  */

#define fd_e8 illegal_1 											  /* DB   FD		  */
OP(fd,e9) { _PC = _IY; change_pc16(_PCD);							} /* JP   (IY)		  */
#define fd_ea illegal_1 											  /* DB   FD		  */
#define fd_eb illegal_1 											  /* DB   FD		  */
#define fd_ec illegal_1 											  /* DB   FD		  */
#define fd_ed illegal_1 											  /* DB   FD		  */
#define fd_ee illegal_1 											  /* DB   FD		  */
#define fd_ef illegal_1 											  /* DB   FD		  */

#define fd_f0 illegal_1 											  /* DB   FD		  */
#define fd_f1 illegal_1 											  /* DB   FD		  */
#define fd_f2 illegal_1 											  /* DB   FD		  */
#define fd_f3 illegal_1 											  /* DB   FD		  */
#define fd_f4 illegal_1 											  /* DB   FD		  */
#define fd_f5 illegal_1 											  /* DB   FD		  */
#define fd_f6 illegal_1 											  /* DB   FD		  */
#define fd_f7 illegal_1 											  /* DB   FD		  */

#define fd_f8 illegal_1 											  /* DB   FD		  */
OP(fd,f9) { _SP = _IY;												} /* LD   SP,IY 	  */
#define fd_fa illegal_1 											  /* DB   FD		  */
#define fd_fb illegal_1 											  /* DB   FD		  */
#define fd_fc illegal_1 											  /* DB   FD		  */
#define fd_fd illegal_1 											  /* DB   FD		  */
#define fd_fe illegal_1 											  /* DB   FD		  */
#define fd_ff illegal_1 											  /* DB   FD		  */

OP(illegal,2)
{
}

/**********************************************************
 * special opcodes (ED prefix)
 **********************************************************/
#define ed_00 illegal_2 											  /* DB   ED		  */
#define ed_01 illegal_2 											  /* DB   ED		  */
#define ed_02 illegal_2 											  /* DB   ED		  */
#define ed_03 illegal_2 											  /* DB   ED		  */
#define ed_04 illegal_2 											  /* DB   ED		  */
#define ed_05 illegal_2 											  /* DB   ED		  */
#define ed_06 illegal_2 											  /* DB   ED		  */
#define ed_07 illegal_2 											  /* DB   ED		  */

#define ed_08 illegal_2 											  /* DB   ED		  */
#define ed_09 illegal_2 											  /* DB   ED		  */
#define ed_0a illegal_2 											  /* DB   ED		  */
#define ed_0b illegal_2 											  /* DB   ED		  */
#define ed_0c illegal_2 											  /* DB   ED		  */
#define ed_0d illegal_2 											  /* DB   ED		  */
#define ed_0e illegal_2 											  /* DB   ED		  */
#define ed_0f illegal_2 											  /* DB   ED		  */

#define ed_10 illegal_2 											  /* DB   ED		  */
#define ed_11 illegal_2 											  /* DB   ED		  */
#define ed_12 illegal_2 											  /* DB   ED		  */
#define ed_13 illegal_2 											  /* DB   ED		  */
#define ed_14 illegal_2 											  /* DB   ED		  */
#define ed_15 illegal_2 											  /* DB   ED		  */
#define ed_16 illegal_2 											  /* DB   ED		  */
#define ed_17 illegal_2 											  /* DB   ED		  */

#define ed_18 illegal_2 											  /* DB   ED		  */
#define ed_19 illegal_2 											  /* DB   ED		  */
#define ed_1a illegal_2 											  /* DB   ED		  */
#define ed_1b illegal_2 											  /* DB   ED		  */
#define ed_1c illegal_2 											  /* DB   ED		  */
#define ed_1d illegal_2 											  /* DB   ED		  */
#define ed_1e illegal_2 											  /* DB   ED		  */
#define ed_1f illegal_2 											  /* DB   ED		  */

#define ed_20 illegal_2 											  /* DB   ED		  */
#define ed_21 illegal_2 											  /* DB   ED		  */
#define ed_22 illegal_2 											  /* DB   ED		  */
#define ed_23 illegal_2 											  /* DB   ED		  */
#define ed_24 illegal_2 											  /* DB   ED		  */
#define ed_25 illegal_2 											  /* DB   ED		  */
#define ed_26 illegal_2 											  /* DB   ED		  */
#define ed_27 illegal_2 											  /* DB   ED		  */

#define ed_28 illegal_2 											  /* DB   ED		  */
#define ed_29 illegal_2 											  /* DB   ED		  */
#define ed_2a illegal_2 											  /* DB   ED		  */
#define ed_2b illegal_2 											  /* DB   ED		  */
#define ed_2c illegal_2 											  /* DB   ED		  */
#define ed_2d illegal_2 											  /* DB   ED		  */
#define ed_2e illegal_2 											  /* DB   ED		  */
#define ed_2f illegal_2 											  /* DB   ED		  */

#define ed_30 illegal_2 											  /* DB   ED		  */
#define ed_31 illegal_2 											  /* DB   ED		  */
#define ed_32 illegal_2 											  /* DB   ED		  */
#define ed_33 illegal_2 											  /* DB   ED		  */
#define ed_34 illegal_2 											  /* DB   ED		  */
#define ed_35 illegal_2 											  /* DB   ED		  */
#define ed_36 illegal_2 											  /* DB   ED		  */
#define ed_37 illegal_2 											  /* DB   ED		  */

#define ed_38 illegal_2 											  /* DB   ED		  */
#define ed_39 illegal_2 											  /* DB   ED		  */
#define ed_3a illegal_2 											  /* DB   ED		  */
#define ed_3b illegal_2 											  /* DB   ED		  */
#define ed_3c illegal_2 											  /* DB   ED		  */
#define ed_3d illegal_2 											  /* DB   ED		  */
#define ed_3e illegal_2 											  /* DB   ED		  */
#define ed_3f illegal_2 											  /* DB   ED		  */

OP(ed,40) { _B = IN(_BC); _F = (_F & CF) | SZP[_B]; 				} /* IN   B,(C) 	  */
OP(ed,41) { OUT(_BC,_B);											} /* OUT  (C),B 	  */
OP(ed,42) { SBC16(_HL,_BC); 										} /* SBC  HL,BC 	  */
OP(ed,43) { WM16( ARG16(), &Z80.BC );								} /* LD   (w),BC	  */
OP(ed,44) { NEG;													} /* NEG			  */
OP(ed,45) { RETN;													} /* RETN;			  */
OP(ed,46) { _IM = 0;												} /* IM   0 		  */
OP(ed,47) { LD_I_A; 												} /* LD   I,A		  */

OP(ed,48) { _C = IN(_BC); _F = (_F & CF) | SZP[_C]; 				} /* IN   C,(C) 	  */
OP(ed,49) { OUT(_BC,_C);											} /* OUT  (C),C 	  */
OP(ed,4a) { ADC16(_HL,_BC); 										} /* ADC  HL,BC 	  */
OP(ed,4b) { _BC = RM16( ARG16() );									} /* LD   BC,(w)	  */
OP(ed,4c) { NEG;													} /* NEG			  */
OP(ed,4d) { RETI;													} /* RETI			  */
OP(ed,4e) { _IM = 0;												} /* IM   0 		  */
OP(ed,4f) { LD_R_A; 												} /* LD   R,A		  */

OP(ed,50) { _D = IN(_BC); _F = (_F & CF) | SZP[_D]; 				} /* IN   D,(C) 	  */
OP(ed,51) { OUT(_BC,_D);											} /* OUT  (C),D 	  */
OP(ed,52) { SBC16(_HL,_DE); 										} /* SBC  HL,DE 	  */
OP(ed,53) { WM16( ARG16(), &Z80.DE );								} /* LD   (w),DE	  */
OP(ed,54) { NEG;													} /* NEG			  */
OP(ed,55) { RETN;													} /* RETN;			  */
OP(ed,56) { _IM = 1;												} /* IM   1 		  */
OP(ed,57) { LD_A_I; 												} /* LD   A,I		  */

OP(ed,58) { _E = IN(_BC); _F = (_F & CF) | SZP[_E]; 				} /* IN   E,(C) 	  */
OP(ed,59) { OUT(_BC,_E);											} /* OUT  (C),E 	  */
OP(ed,5a) { ADC16(_HL,_DE); 										} /* ADC  HL,DE 	  */
OP(ed,5b) { _DE = RM16( ARG16() );									} /* LD   DE,(w)	  */
OP(ed,5c) { NEG;													} /* NEG			  */
OP(ed,5d) { RETI;													} /* RETI			  */
OP(ed,5e) { _IM = 2;												} /* IM   2 		  */
OP(ed,5f) { LD_A_R; 												} /* LD   A,R		  */

OP(ed,60) { _H = IN(_BC); _F = (_F & CF) | SZP[_H]; 				} /* IN   H,(C) 	  */
OP(ed,61) { OUT(_BC,_H);											} /* OUT  (C),H 	  */
OP(ed,62) { SBC16(_HL,_HL); 										} /* SBC  HL,HL 	  */
OP(ed,63) { WM16( ARG16(), &Z80.HL );								} /* LD   (w),HL	  */
OP(ed,64) { NEG;													} /* NEG			  */
OP(ed,65) { RETN;													} /* RETN;			  */
OP(ed,66) { _IM = 0;												} /* IM   0 		  */
OP(ed,67) { RRD;													} /* RRD  (HL)		  */

OP(ed,68) { _L = IN(_BC); _F = (_F & CF) | SZP[_L]; 				} /* IN   L,(C) 	  */
OP(ed,69) { OUT(_BC,_L);											} /* OUT  (C),L 	  */
OP(ed,6a) { ADC16(_HL,_HL); 										} /* ADC  HL,HL 	  */
OP(ed,6b) { _HL = RM16( ARG16() );									} /* LD   HL,(w)	  */
OP(ed,6c) { NEG;													} /* NEG			  */
OP(ed,6d) { RETI;													} /* RETI			  */
OP(ed,6e) { _IM = 0;												} /* IM   0 		  */
OP(ed,6f) { RLD;													} /* RLD  (HL)		  */

OP(ed,70) { UINT8 res = IN(_BC); _F = (_F & CF) | SZP[res]; 		} /* IN   0,(C) 	  */
OP(ed,71) { OUT(_BC,0); 											} /* OUT  (C),0 	  */
OP(ed,72) { SBC16(_HL,_SP); 										} /* SBC  HL,SP 	  */
OP(ed,73) { WM16( ARG16(), &Z80.SP );								} /* LD   (w),SP	  */
OP(ed,74) { NEG;													} /* NEG			  */
OP(ed,75) { RETN;													} /* RETN;			  */
OP(ed,76) { _IM = 1;												} /* IM   1 		  */
#define ed_77 illegal_2 											  /* DB   ED,77 	  */

OP(ed,78) { _A = IN(_BC); _F = (_F & CF) | SZP[_A]; 				} /* IN   E,(C) 	  */
OP(ed,79) { OUT(_BC,_A);											} /* OUT  (C),E 	  */
OP(ed,7a) { ADC16(_HL,_SP); 										} /* ADC  HL,SP 	  */
OP(ed,7b) { _SP = RM16( ARG16() );									} /* LD   SP,(w)	  */
OP(ed,7c) { NEG;													} /* NEG			  */
OP(ed,7d) { RETI;													} /* RETI			  */
OP(ed,7e) { _IM = 2;												} /* IM   2 		  */
#define ed_7f illegal_2 											  /* DB   ED,7F 	  */

#define ed_80 illegal_2 											  /* DB   ED		  */
#define ed_81 illegal_2 											  /* DB   ED		  */
#define ed_82 illegal_2 											  /* DB   ED		  */
#define ed_83 illegal_2 											  /* DB   ED		  */
#define ed_84 illegal_2 											  /* DB   ED		  */
#define ed_85 illegal_2 											  /* DB   ED		  */
#define ed_86 illegal_2 											  /* DB   ED		  */
#define ed_87 illegal_2 											  /* DB   ED		  */

#define ed_88 illegal_2 											  /* DB   ED		  */
#define ed_89 illegal_2 											  /* DB   ED		  */
#define ed_8a illegal_2 											  /* DB   ED		  */
#define ed_8b illegal_2 											  /* DB   ED		  */
#define ed_8c illegal_2 											  /* DB   ED		  */
#define ed_8d illegal_2 											  /* DB   ED		  */
#define ed_8e illegal_2 											  /* DB   ED		  */
#define ed_8f illegal_2 											  /* DB   ED		  */

#define ed_90 illegal_2 											  /* DB   ED		  */
#define ed_91 illegal_2 											  /* DB   ED		  */
#define ed_92 illegal_2 											  /* DB   ED		  */
#define ed_93 illegal_2 											  /* DB   ED		  */
#define ed_94 illegal_2 											  /* DB   ED		  */
#define ed_95 illegal_2 											  /* DB   ED		  */
#define ed_96 illegal_2 											  /* DB   ED		  */
#define ed_97 illegal_2 											  /* DB   ED		  */

#define ed_98 illegal_2 											  /* DB   ED		  */
#define ed_99 illegal_2 											  /* DB   ED		  */
#define ed_9a illegal_2 											  /* DB   ED		  */
#define ed_9b illegal_2 											  /* DB   ED		  */
#define ed_9c illegal_2 											  /* DB   ED		  */
#define ed_9d illegal_2 											  /* DB   ED		  */
#define ed_9e illegal_2 											  /* DB   ED		  */
#define ed_9f illegal_2 											  /* DB   ED		  */

OP(ed,a0) { LDI;													} /* LDI			  */
OP(ed,a1) { CPI;													} /* CPI			  */
OP(ed,a2) { INI;													} /* INI			  */
OP(ed,a3) { OUTI;													} /* OUTI			  */
#define ed_a4 illegal_2 											  /* DB   ED		  */
#define ed_a5 illegal_2 											  /* DB   ED		  */
#define ed_a6 illegal_2 											  /* DB   ED		  */
#define ed_a7 illegal_2 											  /* DB   ED		  */

OP(ed,a8) { LDD;													} /* LDD			  */
OP(ed,a9) { CPD;													} /* CPD			  */
OP(ed,aa) { IND;													} /* IND			  */
OP(ed,ab) { OUTD;													} /* OUTD			  */
#define ed_ac illegal_2 											  /* DB   ED		  */
#define ed_ad illegal_2 											  /* DB   ED		  */
#define ed_ae illegal_2 											  /* DB   ED		  */
#define ed_af illegal_2 											  /* DB   ED		  */

OP(ed,b0) { LDIR;													} /* LDIR			  */
OP(ed,b1) { CPIR;													} /* CPIR			  */
OP(ed,b2) { INIR;													} /* INIR			  */
OP(ed,b3) { OTIR;													} /* OTIR			  */
#define ed_b4 illegal_2 											  /* DB   ED		  */
#define ed_b5 illegal_2 											  /* DB   ED		  */
#define ed_b6 illegal_2 											  /* DB   ED		  */
#define ed_b7 illegal_2 											  /* DB   ED		  */

OP(ed,b8) { LDDR;													} /* LDDR			  */
OP(ed,b9) { CPDR;													} /* CPDR			  */
OP(ed,ba) { INDR;													} /* INDR			  */
OP(ed,bb) { OTDR;													} /* OTDR			  */
#define ed_bc illegal_2 											  /* DB   ED		  */
#define ed_bd illegal_2 											  /* DB   ED		  */
#define ed_be illegal_2 											  /* DB   ED		  */
#define ed_bf illegal_2 											  /* DB   ED		  */

#define ed_c0 illegal_2 											  /* DB   ED		  */
#define ed_c1 illegal_2 											  /* DB   ED		  */
#define ed_c2 illegal_2 											  /* DB   ED		  */
#define ed_c3 illegal_2 											  /* DB   ED		  */
#define ed_c4 illegal_2 											  /* DB   ED		  */
#define ed_c5 illegal_2 											  /* DB   ED		  */
#define ed_c6 illegal_2 											  /* DB   ED		  */
#define ed_c7 illegal_2 											  /* DB   ED		  */

#define ed_c8 illegal_2 											  /* DB   ED		  */
#define ed_c9 illegal_2 											  /* DB   ED		  */
#define ed_ca illegal_2 											  /* DB   ED		  */
#define ed_cb illegal_2 											  /* DB   ED		  */
#define ed_cc illegal_2 											  /* DB   ED		  */
#define ed_cd illegal_2 											  /* DB   ED		  */
#define ed_ce illegal_2 											  /* DB   ED		  */
#define ed_cf illegal_2 											  /* DB   ED		  */

#define ed_d0 illegal_2 											  /* DB   ED		  */
#define ed_d1 illegal_2 											  /* DB   ED		  */
#define ed_d2 illegal_2 											  /* DB   ED		  */
#define ed_d3 illegal_2 											  /* DB   ED		  */
#define ed_d4 illegal_2 											  /* DB   ED		  */
#define ed_d5 illegal_2 											  /* DB   ED		  */
#define ed_d6 illegal_2 											  /* DB   ED		  */
#define ed_d7 illegal_2 											  /* DB   ED		  */

#define ed_d8 illegal_2 											  /* DB   ED		  */
#define ed_d9 illegal_2 											  /* DB   ED		  */
#define ed_da illegal_2 											  /* DB   ED		  */
#define ed_db illegal_2 											  /* DB   ED		  */
#define ed_dc illegal_2 											  /* DB   ED		  */
#define ed_dd illegal_2 											  /* DB   ED		  */
#define ed_de illegal_2 											  /* DB   ED		  */
#define ed_df illegal_2 											  /* DB   ED		  */

#define ed_e0 illegal_2 											  /* DB   ED		  */
#define ed_e1 illegal_2 											  /* DB   ED		  */
#define ed_e2 illegal_2 											  /* DB   ED		  */
#define ed_e3 illegal_2 											  /* DB   ED		  */
#define ed_e4 illegal_2 											  /* DB   ED		  */
#define ed_e5 illegal_2 											  /* DB   ED		  */
#define ed_e6 illegal_2 											  /* DB   ED		  */
#define ed_e7 illegal_2 											  /* DB   ED		  */

#define ed_e8 illegal_2 											  /* DB   ED		  */
#define ed_e9 illegal_2 											  /* DB   ED		  */
#define ed_ea illegal_2 											  /* DB   ED		  */
#define ed_eb illegal_2 											  /* DB   ED		  */
#define ed_ec illegal_2 											  /* DB   ED		  */
#define ed_ed illegal_2 											  /* DB   ED		  */
#define ed_ee illegal_2 											  /* DB   ED		  */
#define ed_ef illegal_2 											  /* DB   ED		  */

#define ed_f0 illegal_2 											  /* DB   ED		  */
#define ed_f1 illegal_2 											  /* DB   ED		  */
#define ed_f2 illegal_2 											  /* DB   ED		  */
#define ed_f3 illegal_2 											  /* DB   ED		  */
#define ed_f4 illegal_2 											  /* DB   ED		  */
#define ed_f5 illegal_2 											  /* DB   ED		  */
#define ed_f6 illegal_2 											  /* DB   ED		  */
#define ed_f7 illegal_2 											  /* DB   ED		  */

#define ed_f8 illegal_2 											  /* DB   ED		  */
#define ed_f9 illegal_2 											  /* DB   ED		  */
#define ed_fa illegal_2 											  /* DB   ED		  */
#define ed_fb illegal_2 											  /* DB   ED		  */
#define ed_fc illegal_2 											  /* DB   ED		  */
#define ed_fd illegal_2 											  /* DB   ED		  */
#define ed_fe illegal_2 											  /* DB   ED		  */
#define ed_ff illegal_2 											  /* DB   ED		  */

/**********************************************************
 * main opcodes
 **********************************************************/
OP(op,00) { 														} /* NOP			  */
OP(op,01) { _BC = ARG16();											} /* LD   BC,w		  */
OP(op,02) { WM( _BC, _A );											} /* LD   (BC),A	  */
OP(op,03) { _BC++;													} /* INC  BC		  */
OP(op,04) { _B = INC(_B);											} /* INC  B 		  */
OP(op,05) { _B = DEC(_B);											} /* DEC  B 		  */
OP(op,06) { _B = ARG(); 											} /* LD   B,n		  */
OP(op,07) { RLCA;													} /* RLCA			  */

OP(op,08) { EX_AF;													} /* EX   AF,AF'      */
OP(op,09) { ADD16(HL,BC);											} /* ADD  HL,BC 	  */
OP(op,0a) { _A = RM(_BC);											} /* LD   A,(BC)	  */
OP(op,0b) { _BC--;													} /* DEC  BC		  */
OP(op,0c) { _C = INC(_C);											} /* INC  C 		  */
OP(op,0d) { _C = DEC(_C);											} /* DEC  C 		  */
OP(op,0e) { _C = ARG(); 											} /* LD   C,n		  */
OP(op,0f) { RRCA;													} /* RRCA			  */

OP(op,10) { _B--; JR(_B);											} /* DJNZ o 		  */
OP(op,11) { _DE = ARG16();											} /* LD   DE,w		  */
OP(op,12) { WM( _DE, _A );											} /* LD   (DE),A	  */
OP(op,13) { _DE++;													} /* INC  DE		  */
OP(op,14) { _D = INC(_D);											} /* INC  D 		  */
OP(op,15) { _D = DEC(_D);											} /* DEC  D 		  */
OP(op,16) { _D = ARG(); 											} /* LD   D,n		  */
OP(op,17) { RLA;													} /* RLA			  */

OP(op,18) { JR(1);													} /* JR   o 		  */
OP(op,19) { ADD16(HL,DE);											} /* ADD  HL,DE 	  */
OP(op,1a) { _A = RM(_DE);											} /* LD   A,(DE)	  */
OP(op,1b) { _DE--;													} /* DEC  DE		  */
OP(op,1c) { _E = INC(_E);											} /* INC  E 		  */
OP(op,1d) { _E = DEC(_E);											} /* DEC  E 		  */
OP(op,1e) { _E = ARG(); 											} /* LD   E,n		  */
OP(op,1f) { RRA;													} /* RRA			  */

OP(op,20) { JR(!(_F & ZF)); 										} /* JR   NZ,o		  */
OP(op,21) { _HL = ARG16();											} /* LD   HL,w		  */
OP(op,22) { WM16( ARG16(), &Z80.HL );								} /* LD   (w),HL	  */
OP(op,23) { _HL++;													} /* INC  HL		  */
OP(op,24) { _H = INC(_H);											} /* INC  H 		  */
OP(op,25) { _H = DEC(_H);											} /* DEC  H 		  */
OP(op,26) { _H = ARG(); 											} /* LD   H,n		  */
OP(op,27) { DAA;													} /* DAA			  */

OP(op,28) { JR(_F & ZF);											} /* JR   Z,o		  */
OP(op,29) { ADD16(HL,HL);											} /* ADD  HL,HL 	  */
OP(op,2a) { _HL = RM16( ARG16() );									} /* LD   HL,(w)	  */
OP(op,2b) { _HL--;													} /* DEC  HL		  */
OP(op,2c) { _L = INC(_L);											} /* INC  L 		  */
OP(op,2d) { _L = DEC(_L);											} /* DEC  L 		  */
OP(op,2e) { _L = ARG(); 											} /* LD   L,n		  */
OP(op,2f) { _A ^= 0xff; _F |= HF | NF;								} /* CPL			  */

OP(op,30) { JR(!(_F & CF)); 										} /* JR   NC,o		  */
OP(op,31) { _SP = ARG16();											} /* LD   SP,w		  */
OP(op,32) { WM( ARG16(), _A );										} /* LD   (w),A 	  */
OP(op,33) { _SP++;													} /* INC  SP		  */
OP(op,34) { WM( _HL, INC(RM(_HL)) );								} /* INC  (HL)		  */
OP(op,35) { WM( _HL, DEC(RM(_HL)) );								} /* DEC  (HL)		  */
OP(op,36) { WM( _HL, ARG() );										} /* LD   (HL),n	  */
OP(op,37) { _F = (_F & ~(HF|NF)) | CF;								} /* SCF			  */

OP(op,38) { JR(_F & CF);											} /* JR   C,o		  */
OP(op,39) { ADD16(HL,SP);											} /* ADD  HL,SP 	  */
OP(op,3a) { _A = RM( ARG16() ); 									} /* LD   A,(w) 	  */
OP(op,3b) { _SP--;													} /* DEC  SP		  */
OP(op,3c) { _A = INC(_A);											} /* INC  A 		  */
OP(op,3d) { _A = DEC(_A);											} /* DEC  A 		  */
OP(op,3e) { _A = ARG(); 											} /* LD   A,n		  */
OP(op,3f) { _F = ((_F & ~(HF|NF)) | ((_F & CF)<<4)) ^ CF;			} /* CCF			  */

OP(op,40) { 														} /* LD   B,B		  */
OP(op,41) { _B = _C;												} /* LD   B,C		  */
OP(op,42) { _B = _D;												} /* LD   B,D		  */
OP(op,43) { _B = _E;												} /* LD   B,E		  */
OP(op,44) { _B = _H;												} /* LD   B,H		  */
OP(op,45) { _B = _L;												} /* LD   B,L		  */
OP(op,46) { _B = RM(_HL);											} /* LD   B,(HL)	  */
OP(op,47) { _B = _A;												} /* LD   B,A		  */

OP(op,48) { _C = _B;												} /* LD   C,B		  */
OP(op,49) { 														} /* LD   C,C		  */
OP(op,4a) { _C = _D;												} /* LD   C,D		  */
OP(op,4b) { _C = _E;												} /* LD   C,E		  */
OP(op,4c) { _C = _H;												} /* LD   C,H		  */
OP(op,4d) { _C = _L;												} /* LD   C,L		  */
OP(op,4e) { _C = RM(_HL);											} /* LD   C,(HL)	  */
OP(op,4f) { _C = _A;												} /* LD   C,A		  */

OP(op,50) { _D = _B;												} /* LD   D,B		  */
OP(op,51) { _D = _C;												} /* LD   D,C		  */
OP(op,52) { 														} /* LD   D,D		  */
OP(op,53) { _D = _E;												} /* LD   D,E		  */
OP(op,54) { _D = _H;												} /* LD   D,H		  */
OP(op,55) { _D = _L;												} /* LD   D,L		  */
OP(op,56) { _D = RM(_HL);											} /* LD   D,(HL)	  */
OP(op,57) { _D = _A;												} /* LD   D,A		  */

OP(op,58) { _E = _B;												} /* LD   E,B		  */
OP(op,59) { _E = _C;												} /* LD   E,C		  */
OP(op,5a) { _E = _D;												} /* LD   E,D		  */
OP(op,5b) { 														} /* LD   E,E		  */
OP(op,5c) { _E = _H;												} /* LD   E,H		  */
OP(op,5d) { _E = _L;												} /* LD   E,L		  */
OP(op,5e) { _E = RM(_HL);											} /* LD   E,(HL)	  */
OP(op,5f) { _E = _A;												} /* LD   E,A		  */

OP(op,60) { _H = _B;												} /* LD   H,B		  */
OP(op,61) { _H = _C;												} /* LD   H,C		  */
OP(op,62) { _H = _D;												} /* LD   H,D		  */
OP(op,63) { _H = _E;												} /* LD   H,E		  */
OP(op,64) { 														} /* LD   H,H		  */
OP(op,65) { _H = _L;												} /* LD   H,L		  */
OP(op,66) { _H = RM(_HL);											} /* LD   H,(HL)	  */
OP(op,67) { _H = _A;												} /* LD   H,A		  */

OP(op,68) { _L = _B;												} /* LD   L,B		  */
OP(op,69) { _L = _C;												} /* LD   L,C		  */
OP(op,6a) { _L = _D;												} /* LD   L,D		  */
OP(op,6b) { _L = _E;												} /* LD   L,E		  */
OP(op,6c) { _L = _H;												} /* LD   L,H		  */
OP(op,6d) { 														} /* LD   L,L		  */
OP(op,6e) { _L = RM(_HL);											} /* LD   L,(HL)	  */
OP(op,6f) { _L = _A;												} /* LD   L,A		  */

OP(op,70) { WM( _HL, _B );											} /* LD   (HL),B	  */
OP(op,71) { WM( _HL, _C );											} /* LD   (HL),C	  */
OP(op,72) { WM( _HL, _D );											} /* LD   (HL),D	  */
OP(op,73) { WM( _HL, _E );											} /* LD   (HL),E	  */
OP(op,74) { WM( _HL, _H );											} /* LD   (HL),H	  */
OP(op,75) { WM( _HL, _L );											} /* LD   (HL),L	  */
OP(op,76) { _PC--; _HALT = 1; if( Z80_ICount > 0 ) Z80_ICount=0;	} /* HALT			  */
OP(op,77) { WM( _HL, _A );											} /* LD   (HL),A	  */

OP(op,78) { _A = _B;												} /* LD   A,B		  */
OP(op,79) { _A = _C;												} /* LD   A,C		  */
OP(op,7a) { _A = _D;												} /* LD   A,D		  */
OP(op,7b) { _A = _E;												} /* LD   A,E		  */
OP(op,7c) { _A = _H;												} /* LD   A,H		  */
OP(op,7d) { _A = _L;												} /* LD   A,L		  */
OP(op,7e) { _A = RM(_HL);											} /* LD   A,(HL)	  */
OP(op,7f) { 														} /* LD   A,A		  */

OP(op,80) { ADD(_B);												} /* ADD  A,B		  */
OP(op,81) { ADD(_C);												} /* ADD  A,C		  */
OP(op,82) { ADD(_D);												} /* ADD  A,D		  */
OP(op,83) { ADD(_E);												} /* ADD  A,E		  */
OP(op,84) { ADD(_H);												} /* ADD  A,H		  */
OP(op,85) { ADD(_L);												} /* ADD  A,L		  */
OP(op,86) { ADD(RM(_HL));											} /* ADD  A,(HL)	  */
OP(op,87) { ADD(_A);												} /* ADD  A,A		  */

OP(op,88) { ADC(_B);												} /* ADC  A,B		  */
OP(op,89) { ADC(_C);												} /* ADC  A,C		  */
OP(op,8a) { ADC(_D);												} /* ADC  A,D		  */
OP(op,8b) { ADC(_E);												} /* ADC  A,E		  */
OP(op,8c) { ADC(_H);												} /* ADC  A,H		  */
OP(op,8d) { ADC(_L);												} /* ADC  A,L		  */
OP(op,8e) { ADC(RM(_HL));											} /* ADC  A,(HL)	  */
OP(op,8f) { ADC(_A);												} /* ADC  A,A		  */

OP(op,90) { SUB(_B);												} /* SUB  B 		  */
OP(op,91) { SUB(_C);												} /* SUB  C 		  */
OP(op,92) { SUB(_D);												} /* SUB  D 		  */
OP(op,93) { SUB(_E);												} /* SUB  E 		  */
OP(op,94) { SUB(_H);												} /* SUB  H 		  */
OP(op,95) { SUB(_L);												} /* SUB  L 		  */
OP(op,96) { SUB(RM(_HL));											} /* SUB  (HL)		  */
OP(op,97) { SUB(_A);												} /* SUB  A 		  */

OP(op,98) { SBC(_B);												} /* SBC  A,B		  */
OP(op,99) { SBC(_C);												} /* SBC  A,C		  */
OP(op,9a) { SBC(_D);												} /* SBC  A,D		  */
OP(op,9b) { SBC(_E);												} /* SBC  A,E		  */
OP(op,9c) { SBC(_H);												} /* SBC  A,H		  */
OP(op,9d) { SBC(_L);												} /* SBC  A,L		  */
OP(op,9e) { SBC(RM(_HL));											} /* SBC  A,(HL)	  */
OP(op,9f) { SBC(_A);												} /* SBC  A,A		  */

OP(op,a0) { AND(_B);												} /* AND  B 		  */
OP(op,a1) { AND(_C);												} /* AND  C 		  */
OP(op,a2) { AND(_D);												} /* AND  D 		  */
OP(op,a3) { AND(_E);												} /* AND  E 		  */
OP(op,a4) { AND(_H);												} /* AND  H 		  */
OP(op,a5) { AND(_L);												} /* AND  L 		  */
OP(op,a6) { AND(RM(_HL));											} /* AND  (HL)		  */
OP(op,a7) { AND(_A);												} /* AND  A 		  */

OP(op,a8) { XOR(_B);												} /* XOR  B 		  */
OP(op,a9) { XOR(_C);												} /* XOR  C 		  */
OP(op,aa) { XOR(_D);												} /* XOR  D 		  */
OP(op,ab) { XOR(_E);												} /* XOR  E 		  */
OP(op,ac) { XOR(_H);												} /* XOR  H 		  */
OP(op,ad) { XOR(_L);												} /* XOR  L 		  */
OP(op,ae) { XOR(RM(_HL));											} /* XOR  (HL)		  */
OP(op,af) { XOR(_A);												} /* XOR  A 		  */

OP(op,b0) { OR(_B); 												} /* OR   B 		  */
OP(op,b1) { OR(_C); 												} /* OR   C 		  */
OP(op,b2) { OR(_D); 												} /* OR   D 		  */
OP(op,b3) { OR(_E); 												} /* OR   E 		  */
OP(op,b4) { OR(_H); 												} /* OR   H 		  */
OP(op,b5) { OR(_L); 												} /* OR   L 		  */
OP(op,b6) { OR(RM(_HL));											} /* OR   (HL)		  */
OP(op,b7) { OR(_A); 												} /* OR   A 		  */

OP(op,b8) { CP(_B); 												} /* CP   B 		  */
OP(op,b9) { CP(_C); 												} /* CP   C 		  */
OP(op,ba) { CP(_D); 												} /* CP   D 		  */
OP(op,bb) { CP(_E); 												} /* CP   E 		  */
OP(op,bc) { CP(_H); 												} /* CP   H 		  */
OP(op,bd) { CP(_L); 												} /* CP   L 		  */
OP(op,be) { CP(RM(_HL));											} /* CP   (HL)		  */
OP(op,bf) { CP(_A); 												} /* CP   A 		  */

OP(op,c0) { RET(!(_F & ZF));										} /* RET  NZ		  */
OP(op,c1) { POP(BC);												} /* POP  BC		  */
OP(op,c2) { JP_COND(!(_F & ZF));									} /* JP   NZ,a		  */
OP(op,c3) { JP; 													} /* JP   a 		  */
OP(op,c4) { CALL(!(_F & ZF));										} /* CALL NZ,a		  */
OP(op,c5) { PUSH( BC ); 											} /* PUSH BC		  */
OP(op,c6) { ADD(ARG()); 											} /* ADD  A,n		  */
OP(op,c7) { RST(0x00);												} /* RST  0 		  */

OP(op,c8) { RET(_F & ZF);											} /* RET  Z 		  */
OP(op,c9) { RET(1); 												} /* RET			  */
OP(op,ca) { JP_COND(_F & ZF);										} /* JP   Z,a		  */
OP(op,cb) { R_INC; EXEC(cb,ROP());									} /* **** CB xx 	  */
OP(op,cc) { CALL(_F & ZF);											} /* CALL Z,a		  */
OP(op,cd) { CALL(1);												} /* CALL a 		  */
OP(op,ce) { ADC(ARG()); 											} /* ADC  A,n		  */
OP(op,cf) { RST(0x08);												} /* RST  1 		  */

OP(op,d0) { RET(!(_F & CF));										} /* RET  NC		  */
OP(op,d1) { POP(DE);												} /* POP  DE		  */
OP(op,d2) { JP_COND(!(_F & CF));									} /* JP   NC,a		  */
OP(op,d3) { unsigned n = ARG() | (_A << 8); OUT( n, _A );			} /* OUT  (n),A 	  */
OP(op,d4) { CALL(!(_F & CF));										} /* CALL NC,a		  */
OP(op,d5) { PUSH( DE ); 											} /* PUSH DE		  */
OP(op,d6) { SUB(ARG()); 											} /* SUB  n 		  */
OP(op,d7) { RST(0x10);												} /* RST  2 		  */

OP(op,d8) { RET(_F & CF);											} /* RET  C 		  */
OP(op,d9) { EXX;													} /* EXX			  */
OP(op,da) { JP_COND(_F & CF);										} /* JP   C,a		  */
OP(op,db) { unsigned n = ARG() | (_A << 8); _A = IN( n );			} /* IN   A,(n) 	  */
OP(op,dc) { CALL(_F & CF);											} /* CALL C,a		  */
OP(op,dd) { R_INC; EXEC(dd,ROP());									} /* **** DD xx 	  */
OP(op,de) { SBC(ARG()); 											} /* SBC  A,n		  */
OP(op,df) { RST(0x18);												} /* RST  3 		  */

OP(op,e0) { RET(!(_F & PF));										} /* RET  PE		  */
OP(op,e1) { POP(HL);												} /* POP  HL		  */
OP(op,e2) { JP_COND(!(_F & PF));									} /* JP   PE,a		  */
OP(op,e3) { EXSP(HL);												} /* EX   HL,(SP)	  */
OP(op,e4) { CALL(!(_F & PF));										} /* CALL PE,a		  */
OP(op,e5) { PUSH( HL ); 											} /* PUSH HL		  */
OP(op,e6) { AND(ARG()); 											} /* AND  n 		  */
OP(op,e7) { RST(0x20);												} /* RST  4 		  */

OP(op,e8) { RET(_F & PF);											} /* RET  PO		  */
OP(op,e9) { _PC = _HL; change_pc16(_PCD);							} /* JP   (HL)		  */
OP(op,ea) { JP_COND(_F & PF);										} /* JP   PO,a		  */
OP(op,eb) { EX_DE_HL;												} /* EX   DE,HL 	  */
OP(op,ec) { CALL(_F & PF);											} /* CALL PO,a		  */
OP(op,ed) { R_INC; EXEC(ed,ROP());									} /* **** ED xx 	  */
OP(op,ee) { XOR(ARG()); 											} /* XOR  n 		  */
OP(op,ef) { RST(0x28);												} /* RST  5 		  */

OP(op,f0) { RET(!(_F & SF));										} /* RET  P 		  */
OP(op,f1) { POP(AF);												} /* POP  AF		  */
OP(op,f2) { JP_COND(!(_F & SF));									} /* JP   P,a		  */
OP(op,f3) { _IFF1 = _IFF2 = 0;										} /* DI 			  */
OP(op,f4) { CALL(!(_F & SF));										} /* CALL P,a		  */
OP(op,f5) { PUSH( AF ); 											} /* PUSH AF		  */
OP(op,f6) { OR(ARG());												} /* OR   n 		  */
OP(op,f7) { RST(0x30);												} /* RST  6 		  */

OP(op,f8) { RET(_F & SF);											} /* RET  M 		  */
OP(op,f9) { _SP = _HL;												} /* LD   SP,HL 	  */
OP(op,fa) { JP_COND(_F & SF);										} /* JP   M,a		  */
OP(op,fb) { EI; 													} /* EI 			  */
OP(op,fc) { CALL(_F & SF);											} /* CALL M,a		  */
OP(op,fd) { R_INC; EXEC(fd,ROP());									} /* **** FD xx 	  */
OP(op,fe) { CP(ARG());												} /* CP   n 		  */
OP(op,ff) { RST(0x38);												} /* RST  7 		  */


#define MKTABLE(tablename,prefix) \
static void (*tablename[0x100])(void) = {   \
    prefix##_00,prefix##_01,prefix##_02,prefix##_03,prefix##_04,prefix##_05,prefix##_06,prefix##_07, \
    prefix##_08,prefix##_09,prefix##_0a,prefix##_0b,prefix##_0c,prefix##_0d,prefix##_0e,prefix##_0f, \
    prefix##_10,prefix##_11,prefix##_12,prefix##_13,prefix##_14,prefix##_15,prefix##_16,prefix##_17, \
    prefix##_18,prefix##_19,prefix##_1a,prefix##_1b,prefix##_1c,prefix##_1d,prefix##_1e,prefix##_1f, \
    prefix##_20,prefix##_21,prefix##_22,prefix##_23,prefix##_24,prefix##_25,prefix##_26,prefix##_27, \
    prefix##_28,prefix##_29,prefix##_2a,prefix##_2b,prefix##_2c,prefix##_2d,prefix##_2e,prefix##_2f, \
    prefix##_30,prefix##_31,prefix##_32,prefix##_33,prefix##_34,prefix##_35,prefix##_36,prefix##_37, \
    prefix##_38,prefix##_39,prefix##_3a,prefix##_3b,prefix##_3c,prefix##_3d,prefix##_3e,prefix##_3f, \
    prefix##_40,prefix##_41,prefix##_42,prefix##_43,prefix##_44,prefix##_45,prefix##_46,prefix##_47, \
    prefix##_48,prefix##_49,prefix##_4a,prefix##_4b,prefix##_4c,prefix##_4d,prefix##_4e,prefix##_4f, \
    prefix##_50,prefix##_51,prefix##_52,prefix##_53,prefix##_54,prefix##_55,prefix##_56,prefix##_57, \
    prefix##_58,prefix##_59,prefix##_5a,prefix##_5b,prefix##_5c,prefix##_5d,prefix##_5e,prefix##_5f, \
    prefix##_60,prefix##_61,prefix##_62,prefix##_63,prefix##_64,prefix##_65,prefix##_66,prefix##_67, \
    prefix##_68,prefix##_69,prefix##_6a,prefix##_6b,prefix##_6c,prefix##_6d,prefix##_6e,prefix##_6f, \
    prefix##_70,prefix##_71,prefix##_72,prefix##_73,prefix##_74,prefix##_75,prefix##_76,prefix##_77, \
    prefix##_78,prefix##_79,prefix##_7a,prefix##_7b,prefix##_7c,prefix##_7d,prefix##_7e,prefix##_7f, \
    prefix##_80,prefix##_81,prefix##_82,prefix##_83,prefix##_84,prefix##_85,prefix##_86,prefix##_87, \
    prefix##_88,prefix##_89,prefix##_8a,prefix##_8b,prefix##_8c,prefix##_8d,prefix##_8e,prefix##_8f, \
    prefix##_90,prefix##_91,prefix##_92,prefix##_93,prefix##_94,prefix##_95,prefix##_96,prefix##_97, \
    prefix##_98,prefix##_99,prefix##_9a,prefix##_9b,prefix##_9c,prefix##_9d,prefix##_9e,prefix##_9f, \
    prefix##_a0,prefix##_a1,prefix##_a2,prefix##_a3,prefix##_a4,prefix##_a5,prefix##_a6,prefix##_a7, \
    prefix##_a8,prefix##_a9,prefix##_aa,prefix##_ab,prefix##_ac,prefix##_ad,prefix##_ae,prefix##_af, \
    prefix##_b0,prefix##_b1,prefix##_b2,prefix##_b3,prefix##_b4,prefix##_b5,prefix##_b6,prefix##_b7, \
    prefix##_b8,prefix##_b9,prefix##_ba,prefix##_bb,prefix##_bc,prefix##_bd,prefix##_be,prefix##_bf, \
    prefix##_c0,prefix##_c1,prefix##_c2,prefix##_c3,prefix##_c4,prefix##_c5,prefix##_c6,prefix##_c7, \
    prefix##_c8,prefix##_c9,prefix##_ca,prefix##_cb,prefix##_cc,prefix##_cd,prefix##_ce,prefix##_cf, \
    prefix##_d0,prefix##_d1,prefix##_d2,prefix##_d3,prefix##_d4,prefix##_d5,prefix##_d6,prefix##_d7, \
    prefix##_d8,prefix##_d9,prefix##_da,prefix##_db,prefix##_dc,prefix##_dd,prefix##_de,prefix##_df, \
    prefix##_e0,prefix##_e1,prefix##_e2,prefix##_e3,prefix##_e4,prefix##_e5,prefix##_e6,prefix##_e7, \
    prefix##_e8,prefix##_e9,prefix##_ea,prefix##_eb,prefix##_ec,prefix##_ed,prefix##_ee,prefix##_ef, \
    prefix##_f0,prefix##_f1,prefix##_f2,prefix##_f3,prefix##_f4,prefix##_f5,prefix##_f6,prefix##_f7, \
    prefix##_f8,prefix##_f9,prefix##_fa,prefix##_fb,prefix##_fc,prefix##_fd,prefix##_fe,prefix##_ff}

MKTABLE(Z80op,op);
MKTABLE(Z80cb,cb);
MKTABLE(Z80dd,dd);
MKTABLE(Z80ed,ed);
MKTABLE(Z80fd,fd);
MKTABLE(Z80xxcb,xxcb);

/****************************************************************************
 * Reset registers to their initial values
 ****************************************************************************/
void Z80_Reset(Z80_DaisyChain *daisy_chain)
{
    UINT8 _s;
	int i;
    for (i = 0; i < 256; i++) {
		SZ[i] = 0;
		if (i==0) SZ[i] |= ZF;
		if (i&128) SZ[i] |= SF;
		SZP[i] = SZ[i];
		if ((((i>>7)^(i>>6)^(i>>5)^(i>>4)^(i>>3)^(i>>2)^(i>>1)^i)&1) == 0)
			SZP[i] |= PF;
		SZHV_inc[i] = SZ[i];
		if (i == 0x80) SZHV_inc[i] |= VF;
		if ((i & 0x0f) == 0x00) SZHV_inc[i] |= HF;
        SZHV_dec[i] = SZ[i] | NF;
		if (i == 0x7f) SZHV_dec[i] |= VF;
		if ((i & 0x0f) == 0x0f) SZHV_dec[i] |= HF;
    }
#if BIG_FLAGS_ARRAY
	if (!SZHVC_add || !SZHVC_sub) {
		int oldval, newval;
		UINT8 *padd, *padc, *psub, *psbc;
        /* allocate big flag arrays once */
		SZHVC_add = (UINT8 *)gm_malloc(2*256*256);
		SZHVC_sub = (UINT8 *)gm_malloc(2*256*256);
		if (!SZHVC_add || !SZHVC_sub) {
			raise(SIGABRT);
		}
		padd = &SZHVC_add[0];
		padc = &SZHVC_add[256*256];
		psub = &SZHVC_sub[0];
		psbc = &SZHVC_sub[256*256];
		for (oldval = 0; oldval < 256; oldval++) {
			for (newval = 0; newval < 256; newval++) {
				/* add or adc w/o carry set */
				*padd = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
				if ((newval & 0x0f) < (oldval & 0x0f)) *padd |= HF;
				if (newval < oldval) *padd |= CF;
				else if ((newval & 0x80) != (oldval & 0x80)) *padd |= VF;
				padd++;

				/* adc with carry set */
				*padc = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
                if (((newval-1) & 0x0f) < (oldval & 0x0f)) *padc |= HF;
				if (((newval-1) & 0xff) < oldval) *padc |= CF;
				else if (((newval-1) & 0x80) != (oldval & 0x80)) *padc |= VF;
				padc++;

				/* cp, sub or sbc w/o carry set */
				*psub = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
                if ((newval & 0x0f) > (oldval & 0x0f)) *psub |= HF;
				if (newval > oldval) *psub |= CF;
				else if ((newval & 0x80) != (oldval & 0x80)) *psub |= VF;
				psub++;

				/* sbc with carry set */
				*psbc = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
                if (((newval+1) & 0x0f) > (oldval & 0x0f)) *psbc |= HF;
				if (((newval+1) & 0xff) > oldval) *psbc |= CF;
				else if (((newval+1) & 0x80) != (oldval & 0x80)) *psbc |= VF;
				psbc++;
			}
		}
	}
#endif
    gm_memset (&Z80, 0, sizeof(Z80_Regs));
	Z80.request_irq = Z80.service_irq = -1;
    change_pc(_PCD);
    _SPD = 0xf000;
#if NEW_INTERRUPT_SYSTEM
	Z80.nmi_state = CLEAR_LINE;
	Z80.irq_state = CLEAR_LINE;
	Z80.pending_irq = 0;
#else
	Z80_Clear_Pending_Interrupts();
#endif
    if (daisy_chain) {
        while( daisy_chain->irq_param != -1 && Z80.irq_max < Z80_MAXDAISY )
        {
            /* set callbackhandler after reti */
			Z80.irq[Z80.irq_max] = *daisy_chain;
            /* device reset */
			if( Z80.irq[Z80.irq_max].reset )
				Z80.irq[Z80.irq_max].reset(Z80.irq[Z80.irq_max].irq_param);
			Z80.irq_max++;
            daisy_chain++;
        }
    }
}

/* search highest interrupt request device (next interrupt device) */
/*    and highest interrupt service device (next reti      device) */
static void check_daisy_chain(void)
{
	int device;
	Z80.request_irq = Z80.service_irq = -1;

	/* search higher IRQ or IEO */
	for( device = 0 ; device < Z80.irq_max ; device ++ ) {
        /* IEO = disable ? */
		if( Z80.int_state[device] & Z80_INT_IEO ) {
			/* if IEO is disable , masking lower IRQ */
			Z80.request_irq = -1;
			/* set highest interrupt service device */
			Z80.service_irq = device;
		}
		/* IRQ = ON ? */
		if( Z80.int_state[device] & Z80_INT_REQ )
			Z80.request_irq = device;
    }
	/* set interrupt pending flag */
	if( Z80.request_irq >= 0 )
		Z80.pending_irq |=	INT_IRQ;
	else
		Z80.pending_irq &= ~INT_IRQ;
}

static void Interrupt(void)
{
	if ( (Z80.pending_irq & NMI_IRQ) || _IFF1 ) {

        int irq_vector = Z80_IGNORE_INT;

		/* there isn't a valid previouspc */
		previouspc = -1;

        /* Check if processor was halted */
		if (_HALT) {
			++_PC;
			_HALT = 0;
		}

        if (Z80.pending_irq & NMI_IRQ) {
			/* Save interrupt flip-flop 1 to 2 */
			_IFF2 = _IFF1;
			/* Clear interrupt flip-flop 1 */
			_IFF1 = 0;
			PUSH( PC );
			_PCD = 0x0066;
			/* reset NMI interrupt request */
			Z80.pending_irq &= ~NMI_IRQ;
		} else {
			/* Clear interrupt flip-flop 1 */
            _IFF1 = 0;
#if NEW_INTERRUPT_SYSTEM
			if( !Z80.irq_max ) {  /* not daisy chain mode */
				/* call back the cpu interface to retrieve the vector */
				Z80.vector = (*Z80.irq_callback)(0);
			}
#else
			/* reset INT interrupt request */
			Z80.pending_irq &= ~INT_IRQ;
#endif
			if ( Z80.irq_max ) {
                if( Z80.request_irq >= 0 ) {
                    irq_vector = Z80.irq[Z80.request_irq].interrupt_entry(Z80.irq[Z80.request_irq].irq_param);
                    Z80.request_irq = -1;
                }
            } else {
                irq_vector = Z80.vector;
            }
            /* Interrupt mode 2. Call [Z80.I:databyte] */
			if( _IM == 2 ) {
				irq_vector = (irq_vector & 0xff) | (_I << 8);
				PUSH( PC );
				_PCD = RM16( irq_vector );
			} else
			/* Interrupt mode 1. RST 38h */
			if( _IM == 1 ) {
				/* R_INC; wasn't there... */
				EXEC(op,0xff);
			} else {
				/* Interrupt mode 0. We check for CALL and JP instructions, */
				/* if neither of these were found we assume a 1 byte opcode */
				/* was placed on the databus								*/
				switch (irq_vector & 0xff0000) {
					case 0xcd0000:	/* call */
						PUSH( PC );
					case 0xc30000:	/* jump */
						_PCD = irq_vector & 0xffff;
						break;
					default:
						irq_vector &= 0xff;
						/* R_INC; wasn't there... */
						EXEC(op,irq_vector);
						break;
				}
			}
		}
    }
    change_pc(_PCD);
}

#if NEW_INTERRUPT_SYSTEM

void Z80_set_nmi_line(int state)
{
    if (Z80.nmi_state == state) return;
    Z80.nmi_state = state;
	if (state != CLEAR_LINE) {
        Z80.pending_irq |= NMI_IRQ;
	}
}

void Z80_set_irq_line(int irqline, int state)
{
    Z80.irq_state = state;
	if (state == CLEAR_LINE) {
		if( !Z80.irq_max ) {
			if(!_IFF1 ) 						/* if interrupts are disabled */
				Z80.pending_irq &= ~INT_IRQ;	/* clear the pending IRQ */
		}
	}  else {
		if( Z80.irq_max ) {
			int daisychain, dc_device, dc_state;
			daisychain = (*Z80.irq_callback)(irqline);
			dc_device = daisychain >> 8;
			dc_state = daisychain & 0xff;
			if( Z80.int_state[dc_device] != dc_state ) {
				/* set new interrupt status */
				Z80.int_state[dc_device] = dc_state;
				/* check interrupt status */
				check_daisy_chain();
			}
		} else {
			if( _IFF1 ) 					/* if interrupts are enabled */
				Z80.pending_irq |= INT_IRQ; /* set the pending IRQ */
		}
	}
}

void Z80_set_irq_callback(int (*callback)(int))
{
    Z80.irq_callback = callback;
}

#else

void Z80_Cause_Interrupt(int type)
{
/* type value :                                                          */
/*  Z80_NMI_INT                      -> NMI request                      */
/*  Z80_IGNORE_INT                   -> no request                       */
/*  vector(0x00-0xff)                -> SINGLE interrupt request         */
/*  Z80_VECTOR(device,status)        -> DaisyChain change interrupt status */
/*      device : device number of daisy-chain link                       */
/*      status : Z80_INT_REQ  -> interrupt request                       */
/*               Z80_INT_IEO  -> interrupt disable output                */

    if (type == Z80_NMI_INT) {
        Z80.pending_irq |= NMI_IRQ;
	} else if (type != Z80_IGNORE_INT) {
        if( Z80.irq_max ) {   /* daisy chain mode */
            int device = type >> 8;
            int state  = type & 0xff;

            if( Z80.int_state[device] != state ) {
                /* set new interrupt status */
                Z80.int_state[device] = state;
                /* check interrupt status */
                check_daisy_chain();
            }
        } else {
            /* single int mode */
			Z80.vector = type & 0xff;
            Z80.pending_irq |= INT_IRQ;
        }
    }
}

void Z80_Clear_Pending_Interrupts(void)
{
    int i;

    /* clear irq for all device */
    for( i = 0 ; i < Z80_MAXDAISY ; i++ )
    {
        /* !!!!! shoud be clear interrupt status for daisy-chain devices !!!!! */
        Z80.int_state[i]  = 0;
    }
    Z80.pending_irq = 0;
	Z80.service_irq = -1;
}

#endif

/****************************************************************************
 * Set all registers to given values
 ****************************************************************************/
void Z80_SetRegs (Z80_Regs *Regs)
{
	Z80 = *Regs;
	change_pc(_PCD);
}

/****************************************************************************
 * Get all registers in given buffer
 ****************************************************************************/
void Z80_GetRegs (Z80_Regs *Regs)
{
	*Regs = Z80;
}

/****************************************************************************
 * Return program counter
 ****************************************************************************/
unsigned Z80_GetPC (void)
{
	return _PCD;
}

/****************************************************************************
 * Execute IPeriod T-states. Return number of T-states really executed
 ****************************************************************************/
int Z80_Execute(int cycles)
{
	Z80_ICount = cycles;
	do {
		if (Z80.pending_irq)
			Interrupt();
		CALL_MAME_DEBUG;
		R_INC;
        previouspc = _PCD;
		EXEC_INLINE(op,ROP());
	} while (Z80_ICount > 0);

	return cycles - Z80_ICount;
}

