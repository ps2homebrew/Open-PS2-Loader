/*
  padhook.h Open PS2 Loader In Game Reset
 
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  SPU definitions taken from PS2SDK freesd.
  Copyright (c) 2004 TyRaNiD <tiraniddo@hotmail.com>
  Copyright (c) 2004,2007 Lukasz Bruun <mail@lukasz.dk>
*/

#include <tamtypes.h>


// SPU2 Registers
#define U16_REGISTER(x)    ((volatile u16 *) (0xBF900000 | (x)))
#define U32_REGISTER(x)	   ((volatile u32 *) (0xBF800000 | (x)))
// SD_CORE_ATTR Macros
#define SD_SPU2_ON					(1 << 15)
// SPU DMA Channels 0,1 - 1088 bytes apart
#define SD_DMA_CHCR(ch)     ((volatile u32*)(0xBF8010C8+(ch*1088)))
#define SD_DMA_START		    (1 << 24)
// SPDIF OUT
#define SD_C_SPDIF_OUT			((volatile u16*)0xBF9007C0)
// Base of SPU2 regs is 0x0xBF900000
#define SD_BASE_REG(reg)    ((volatile u16 *)(0xBF900000 + reg))
#define SD_A_REG(core, reg) SD_BASE_REG(0x1A0 + ((core) << 10) + (reg)) 
#define SD_A_KOFF_HI(core)  SD_A_REG((core), 0x04)
#define SD_A_KOFF_LO(core)  SD_A_REG((core), 0x06) 
#define SD_P_REG(core, reg) SD_BASE_REG(0x760 + ((core) * 40) + (reg))
#define SD_P_MVOLL(core)    SD_P_REG((core), 0x00)
#define SD_P_MVOLR(core)    SD_P_REG((core), 0x02)
#define SD_S_REG(core, reg) SD_BASE_REG(0x180 + ((core) << 10) + (reg)) 
#define SD_S_PMON_HI(core)  SD_S_REG((core), 0x00)
#define SD_S_PMON_LO(core)  SD_S_REG((core), 0x02)
#define SD_S_NON_HI(core)   SD_S_REG((core), 0x04)
#define SD_S_NON_LO(core)   SD_S_REG((core), 0x06)
#define SD_CORE_ATTR(core)  SD_S_REG((core), 0x1A)


// DEV9 Registers
#define DEV9_R_1460 ((volatile u16*)0xBF801460)
#define DEV9_R_1464 ((volatile u16*)0xBF801464)
#define DEV9_R_1466 ((volatile u16*)0xBF801466)
#define DEV9_R_146C ((volatile u16*)0xBF80146C)
#define DEV9_R_146E ((volatile u16*)0xBF80146E)
#define DEV9_R_1474 ((volatile u16*)0xBF801474)


// CDVD Registers
#define CDVD_R_NDIN ((volatile u8*)0xBF402005)
#define CDVD_R_POFF ((volatile u8*)0xBF402008)
#define CDVD_R_SCMD ((volatile u8*)0xBF402016)
#define CDVD_R_SDIN ((volatile u8*)0xBF402017)


typedef struct {
	u32 option;
	int port;
	int slot;
	int number;
	u8  name[16];
} pad2socketparam_t;

typedef struct {
 int libpad;
 u8 *pad_buf;
 int vb_count;
 int pos_combo1;
 int pos_combo2;
 int pos_state;
 int pos_frame;
 u8  combo_type;
 u8  prev_frame;
} paddata_t;

typedef struct {
 int press;
 int vb_count;
} powerbuttondata_t;

typedef struct {
	u32 *pattern;
	u32 *mask;
	int  size;
	int  version;	
} pattern_t;

#define IGR_LIBPAD_V1 1
#define IGR_LIBPAD_V2 2

#define IGR_PAD_STABLE_V1 0x06
#define IGR_PAD_STABLE_V2 0x01

#define IGR_COMBO_R1_L1_R2_L2  0xF0
#define IGR_COMBO_START_SELECT 0xF6
#define IGR_COMBO_R3_L3        0xF9

#define NB_PADOPEN_PATTERN 7

/*******************************************************
 * For libpad support
 *
 * libpad          2.1.1.0
 * libpad          2.1.3.0
 * libpad          2.2.0.0
 * libpad          2.3.0.0
 * libpad          2.4.1.0
 * libpad          2.5.0.0
 * libpad          2.6.0.0
 * libpad          2.7.0.0
 * libpad          2.7.1.0
 * libpad          2.8.0.0
 * libpad          3.0.0.0
 * libpad          3.0.1.0
 * libpad          3.0.2.0
 * libpad          3.1.0.0
 */
static u32 padPortOpenpattern0[] = {
	0x27bdff50,		//	addiu		sp, sp, $ff50
	0xffb40050,		//	sd		 	s4, $0050(sp)
	0xffb30040,		//	sd		  s3, $0040(sp)
	0x00c0a02d,		//	daddu		s4, a2, zero
	0xffb20030,		//	sd			s2, $0030(sp)
	0x0080982d,		//	daddu		s3, a0, zero
	0xffbf00a0,		//	sd			ra, $00a0(sp)
	0x00a0902d,		//	daddu		s2, a1, zero
	0xffbe0090,		//	sd			fp, $0090(sp)
	0x3282003f,		//	andi		v0, s4, $003f
	0xffb70080,		//	sd			s7, $0080(sp)
	0xffb60070,		//	sd			s6, $0070(sp)
	0xffb50060,		//	sd			s5, $0060(sp)
	0xffb10020,		//	sd			s1, $0020(sp)
	0x10400000,		//	beq			v0, zero, $XXXXXXXX
	0xffb00010,		//	sd			s0, $0010(sp)
	0x3c020000,		//	lui			v0, $XXXX
	0x8c430000,		//	lw			v1, $XXXX(v0)
	0x10600000,		//	beq			v1, zero, $XXXXXXXX
	0x3c040000,		//	lui			a0, $XXXX
	0x0280282d,		//	daddu		a1, s4, zero
	0x0c000000		//	jal			scePrintf
};
static u32 padPortOpenpattern0_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffff0000,
	0xffff0000,
	0xffff0000,
	0xffffffff,
	0xfc000000
};

/*******************************************************
 * For libpad support
 *
 * libpad          2.1.0.0
 */
static u32 padPortOpenpattern1[] = {
	0x27bdff50,		//	addiu		sp, sp, $ff50
	0xffb20030,		//	sd			s2, $0030(sp)
	0xffb40050,		//	sd			s4, $0050(sp)
	0x00c0902d,		//	daddu		s2, a2, zero
	0xffb30040,		//	sd			s3, $0040(sp)
	0x00a0a02d,		//	daddu		s4, a1, zero
	0xffbf00a0,		//	sd			ra, $00a0(sp)
	0x0080982d,		//	daddu		s3, a0, zero
	0xffbe0090,		//	sd			fp, $0090(sp)
	0x3242003f,		//	andi		v0, s2, $003f
	0xffb70080,		//	sd			s7, $0080(sp)
	0xffb60070,		//	sd			s6, $0070(sp)
	0xffb50060,		//	sd			s5, $0060(sp)
	0xffb10020,		//	sd			s1, $0020(sp)
	0x10400000,		//	beq			v0, zero, $XXXXXXXX
	0xffb00010,		//	sd			s0, $0010(sp)
	0x3c040000,		//	lui			a0, $XXXX
	0x0240282d,		//	daddu		a1, s2, zero
	0x0c000000		//	jal			kprintf
};
static u32 padPortOpenpattern1_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xfc000000
};

/*******************************************************
 * For libpad support
 *
 * libpad          1.6.3.0
 * libpad          2.0.0.0
 */
static u32 padPortOpenpattern2[] = {
	0x27bdff60,		//	addiu		sp, sp, $ff60               
	0xffb20030,		//	sd			s2, $0030(sp)               
	0xffb70080,		//	sd			s7, $0080(sp)               
	0x00c0902d,		//	daddu		s2, a2, zero                
	0xffb60070,		//	sd			s6, $0070(sp)               
	0x0080b82d,		//	daddu		s7, a0, zero                
	0xffbf0090,		//	sd			ra, $0090(sp)               
	0x00a0b02d,		//	daddu		s6, a1, zero                
	0xffb50060,		//	sd			s5, $0060(sp)               
	0x3242003f,		//	andi		v0, s2, $003f               
	0xffb40050,		//	sd			s4, $0050(sp)               
	0xffb30040,		//	sd			s3, $0040(sp)               
	0xffb10020,		//	sd			s1, $0020(sp)               
	0x10400000,		//	beq			v0, zero, $XXXXXXXX         
	0xffb00010,		//	sd			s0, $0010(sp)               
	0x3c040000,		//	lui			a0, $XXXX                   
	0x0240282d,		//	daddu		a1, s2, zero
	0x0c000000		//	jal			printf
};
static u32 padPortOpenpattern2_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xfc000000
};

/*******************************************************
 * For libpad support
 *
 * libpad          1.5.0.0
 */
static u32 padPortOpenpattern3[] = {
	0x27bdff70,		//	addiu		sp, sp, $ff70
	0xffb20030,		//	sd			s2, $0030(sp)
	0xffb50060,		//	sd			s5, $0060(sp)
	0x00c0902d,		//	daddu		s2, a2, zero
	0xffb40050,		//	sd			s4, $0050(sp)
	0x0080a82d,		//	daddu		s5, a0, zero
	0xffbf0080,		//	sd			ra, $0080(sp)
	0x00a0a02d,		//	daddu		s4, a1, zero
	0xffb60070,		//	sd			s6, $0070(sp)
	0x3242000f,		//	andi		v0, s2, $000f
	0xffb30040,		//	sd			s3, $0040(sp)
	0xffb10020,		//	sd			s1, $0020(sp)
	0x10400000,		//	beq			v0, zero, $XXXXXXXX
	0xffb00010,		//	sd			s0, $0010(sp)
	0x3c040000,		//	lui			a0, $XXXX
	0x0c000000		//	jal			printf
};
static u32 padPortOpenpattern3_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xfc000000
};

/**************************************************************************
 * For libpad2 support
 *
 * libpad2  2.7.1.0
 * libpad2  3.0.0.0
 * libpad2  3.0.2.0
 * libpad2  3.1.0.0
 */
static u32 pad2CreateSocketpattern0[] = {
	0x27bdff90,		//	addiu		sp, sp, $ff90
	0x0080302d,		//	daddu		a2, a0, zero
	0xffb10040,		//	sd			s1, $0040(sp)
	0x00a0882d,		//	daddu		s1, a1, zero
	0xffbf0060,		//	sd			ra, $0060(sp)
	0xffb20050,		//	sd			s2, $0050(sp)
	0x3222003f,		//	andi		v0, s1, $003f
	0x10400000,		//	beq			v0, zero, $XXXXXXXX
	0xffb00030,		//	sd			s0, $0030(sp)
	0x3c040000,		//	lui			a0, $XXXX
	0x0c000000,		//	jal			scePrintf
	0x24840000,		//	addiu		a0, a0, $XXXX
	0x10000000,		//	beq			zero, zero, $XXXXXXXX
	0x2402ffff,		//	addiu		v0, zero, $ffff
	0x50c00000,		//	beql		a2, zero, $XXXXXXXX
	0xafa00000,		//	sw			zero, $0000(sp)
	0x8cc20000,		//	lw			v0, $0000(a2)
	0x8cc30004,		//	lw			v1, $0004(a2)
	0x8cc40008,		//	lw			a0, $0008(a2)
	0x8cc5000c,		//	lw			a1, $000c(a2)
	0xafa20000,		//	sw			v0, $0000(sp)
	0xafa30008,		//	sw			v1, $0008(sp)
	0xafa4000c,		//	sw			a0, $000c(sp)
	0xafa50010		//	sw			a1, $0010(sp)
};
static u32 pad2CreateSocketpattern0_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xfc000000,
	0xffff0000,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff
};

/**************************************************************************
 * For libpad2 support
 *
 * libpad2	2.7.0.0
 */
static u32 pad2CreateSocketpattern1[] = {
	0x27bdff70,		//	addiu		sp, sp, $ff70
	0x0080302d,		//	daddu		a2, a0, zero
	0xffb30060,		//	sd			s3, $0060(sp)
	0x00a0982d,		//	daddu		s3, a1, zero
	0xffbf0080,		//	sd			ra, $0080(sp)
	0xffb40070,		//	sd			s4, $0070(sp)
	0x3262003f,		//	andi		v0, s3, $003f
	0xffb20050,		//	sd			s2, $0050(sp)
	0xffb10040,		//	sd			s1, $0040(sp)
	0x10400000,		//	beq			v0, zero, $XXXXXXXX
	0xffb00030,		//	sd			s0, $0030(sp)
	0x3c040000,		//	lui			a0, $XXXX
	0x0c000000,		//	jal			scePrintf
	0x24840000,		//	addiu		a0, a0, $XXXX
	0x10000000,		//	beq			zero, zero, $XXXXXXXX
	0x2402ffff,		//	addiu		v0, zero, $ffff
	0x50c00000,		//	beql		a2, zero, $XXXXXXXX
	0xafa00000,		//	sw			zero, $0000(sp)
	0x8cc20000,		//	lw			v0, $0000(a2)
	0x8cc30004,		//	lw			v1, $0004(a2)
	0x8cc40008,		//	lw			a0, $0008(a2)
	0x8cc5000c,		//	lw			a1, $000c(a2)
	0xafa20000,		//	sw			v0, $0000(sp)
	0xafa30008,		//	sw			v1, $0008(sp)
	0xafa4000c		//	sw			a0, $000c(sp)
};
static u32 pad2CreateSocketpattern1_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xfc000000,
	0xffff0000,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff
};

/**************************************************************************
 * For libpad2 support
 *
 * libpad2	2.5.0.0
 */
static u32 pad2CreateSocketpattern2[] = {
	0x27bdff70,		//	addiu		sp, sp, $ff70
	0x0080302d,		//	daddu		a2, a0, zero
	0xffb30060,		//	sd			s3, $0060(sp)
	0x00a0982d,		//	daddu		s3, a1, zero
	0xffbf0080,		//	sd			ra, $0080(sp)
	0xffb40070,		//	sd			s4, $0070(sp)
	0x3262003f,		//	andi		v0, s3, $003f
	0xffb20050,		//	sd			s2, $0050(sp)
	0xffb10040,		//	sd			s1, $0040(sp)
	0x10400000,		//	beq			v0, zero, $XXXXXXXX
	0xffb00030,		//	sd			s0, $0030(sp)
	0x10000000,		//	beq			zero, zero, $XXXXXXXX
	0x2402ffff,		//	addiu		v0, zero, $ffff
	0x50c00000,		//	beql		a2, zero, $XXXXXXXX
	0xafa00000,		//	sw			zero, $0000(sp)
	0x8cc20000,		//	lw			v0, $0000(a2)
	0x8cc30004,		//	lw			v1, $0004(a2)
	0x8cc40008,		//	lw			a0, $0008(a2)
	0x8cc5000c,		//	lw			a1, $000c(a2)
	0xafa20000,		//	sw			v0, $0000(sp)
	0xafa30008,		//	sw			v1, $0008(sp)
	0xafa4000c,		//	sw			a0, $000c(sp)
	0xafa50010		//	sw			a1, $0010(sp)
};
static u32 pad2CreateSocketpattern2_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff
};
