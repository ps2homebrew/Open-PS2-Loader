/*
  padhook.h Open PS2 Loader In Game Reset
 
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  SPU definitions taken from PS2SDK freesd.
  Copyright (c) 2004 TyRaNiD <tiraniddo@hotmail.com>
  Copyright (c) 2004,2007 Lukasz Bruun <mail@lukasz.dk>

  scePadRead Pattern taken from ps2rd.
  Copyright (C) 2009 jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright (C) 2009 misfire <misfire@xploderfreax.de>
*/

#include <tamtypes.h>


// SD_CORE_ATTR Macros
#define SD_SPU2_ON					(1 << 15)

// SPU DMA Channels 0,1 - 1088 bytes apart
#define SD_DMA_CHCR(ch)     ((volatile u32*)(0xBF8010C8+(ch*1088)))
#define SD_DMA_START		    (1 << 24)

// SPDIF OUT
#define SD_C_SPDIF_OUT			((volatile u16*)0xBF9007C0)

// Base of SPU2 regs is 0x0xBF900000
#define U16_REGISTER(x)    ((volatile u16 *) (0xBF900000 | (x)))
#define U32_REGISTER(x)	   ((volatile u32 *) (0xBF800000 | (x)))

#define SD_BASE_REG(reg)   ((volatile u16 *)(0xBF900000 + reg))

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



/**************************************************************************
 * For libpad support
 *
 * libpad	2.1.1.0
 * libpad	2.1.3.0
 * libpad	2.2.0.0
 * libpad	2.3.0.0
 * libpad	2.4.0.0
 * libpad	2.4.1.0
 * libpad	2.5.0.0
 * libpad	2.6.0.0
 * libpad	2.7.0.0
 */
static u32 padReadpattern0[] = {
	0x0080382d,		// daddu a3, a0, zero
	0x24030070,		// li 	 v1, $00000070
	0x2404001c,		// li  	 a0, $0000001c
	0x70e31818,		// mult1 v1, a3, v1
	0x00a42018,		// mult	 a0, a1, a0
	0x27bdff00,		// addiu sp, sp, $ffXX
	0x3c020000,		// lui 	 v0, $XXXX
	0xff000000,		// sd 	 XX, $XXXX(XX)
	0xffbf0000,		// sd  	 ra, $XXXX(sp)
	0x24420000,		// addiu v0, v0, $XXXX
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x0c000000		// jal   scePadGetDmaStr
};

/**************************************************************************
 * For libpad support
 *
 * libpad	2.1.0.0
 */
static u32 padReadpattern1[] = {
	0x0080382d,		// daddu a3, a0, zero
	0x24020060,		// li 	 v0, $00000060
	0x24040180,		// li  	 a0, $00000180
	0x00a21018,		// mult  v0, a1, v0
	0x70e42018,		// mult1 a0, a3, a0
	0x27bdff00,		// addiu sp, sp, $ffXX
	0x3c030000,		// lui 	 v1, $XXXX
	0xff000000,		// sd 	 XX, $XXXX(XX)
	0xffbf0000,		// sd  	 ra, $XXXX(sp)
	0x24630000,		// addiu v1, v1, $XXXX
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x00000000, 		// ...
	0x0c000000		// jal   scePadGetDmaStr
};
static u32 padReadpattern0_1_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffff00,
	0xffff0000,
	0xff000000,
	0xffff0000,
	0xffff0000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xfc000000
};

/**************************************************************************
 * For libpad support
 *
 * libpad	2.7.1.0
 * libpad	2.8.0.0
 */
static u32 padReadpattern2[] = {
	0x27bdff00,		// addiu sp, sp, $ffXX
	0x24030070,		// li 	 v1, $00000070
	0xffb10000,		// sd 	 s1, $XXXX(sp)
	0x3c020000,		// lui	 v0, $XXXX
	0xffb20000,		// sd 	 s2, $XXXX(sp)
	0x0080882d,		// daddu s1, a0, zero
	0x00a0902d,		// daddu s2, a1, zero
	0x2404001c,		// li 	 a0, $0000001c
	0x72231818,		// mult1 v1, s1, v1
	0x02442018,		// mult  a0, s2, a0
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x0c000000		// jal   DIntr
};
static u32 padReadpattern2_mask[] = {
	0xffffff00,
	0xffffffff,
	0xffff0000,
	0xffff0000,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xfc000000
};

/**************************************************************************
 * For libpad support
 *
 * libpad      3.0.0.0
 */
static u32 padReadpattern3[] = {
	0x27bdff00,		// addiu sp, sp, $ffXX
	0x24030070,		// li 	 v1, $00000070
	0xffb10000,		// sd 	 s1, $XXXX(sp)
	0x3c020000,		// lui	 v0, $XXXX
	0xffb20000,		// sd 	 s2, $XXXX(sp)
	0x0080882d,		// daddu s1, a0, zero
	0x00a0902d,		// daddu s2, a1, zero
	0x2404001c,		// li 	 a0, $0000001c
	0x72231818,		// mult1 v1, s1, v1
	0x02442018,		// mult  a0, s2, a0
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x0c000000		// jal   DIntr
};
static u32 padReadpattern3_mask[] = {
	0xffffff00,
	0xffffffff,
	0xffff0000,
	0xffff0000,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xfc000000
};

/**************************************************************************
 * For libpad2 support
 *
 * libpad2	2.4.0.0
 * libpad2	2.5.0.0
 * libpad2	2.7.0.0
 * libpad2	2.7.1.0
 * libpad2	2.8.0.0
 * libpad2      3.0.0.0
 * libpad2      3.0.2.0
 */
static u32 pad2Readpattern0[] = {
	0x27bdffc0,		// addiu sp, sp, $ffc0
	0x24020000, 		// li 	 v0, $XXXX
	0xffb10010,		// sd 	 s1, $0010(sp)
	0x3c030000, 		// lui 	 v1, $XXXX
	0x0080882d,		// daddu s1, a0, zero
	0xffb20020,		// sd 	 s2, $0020(sp)
	0x02222018,		// mult  a0, s1, v0
	0x24660000, 		// addiu a2, v1, $XXXX
	0xffbf0030,		// sd 	 ra, $0030(sp)
	0x00a0902d,		// daddu s2, a1, zero
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x0c000000		// jal   scePad2LinkDriver
};
static u32 pad2Readpattern0_mask[] = {
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xfc000000
};

/**************************************************************************
 * For libpad support
 *
 * libpad	1.5.0.0
 */
static u32 padReadpattern4[] = {
	0x27bdffc0,		// addiu	sp, sp, $ffc0
	0x00052900,		// sll		a1, a1, 4
	0x000421c0,		// sll		a0, a0, 7
	0x3c020000,		// lui		v0, PadDataAddrHi
	0x00a42821,		// addu		a1, a1, a0
	0xffb20020,		// sd		s2, $0020(sp)
	0xffb10010,		// sd		s1, $0010(sp)
	0x24420000,		// addiu	v0, v0, PadDataAddrLo
	0xffbf0030,		// sd		ra, $0030(sp)
	0x00451021,		// addu		v0, v0, a1
	0xffb00000,		// sd		s0, $0000(sp)
	0x00c0902d,		// daddu	s2, a2, zero
	0x8c50000c,		// lw		s0, $000c(v0)
	0x24050100,		// li		a1, $00000100
	0x0c000000		// jal		sceSifWriteBackDCache
};
static u32 padReadpattern4_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xfc000000
};

/**************************************************************************
 * For libpad support
 *
 * libpad	1.6.2.0
 * libpad	1.6.3.0
 */
static u32 padReadpattern5[] = {
	0x24020060,		// li		v0, $00000060
	0x24070300,		// li		a3, $00000300
	0x00a21818,		// mult		v1, a1, v0
	0x70872018,		// mult1	a0, a0, a3
	0x27bdffc0,		// addiu	sp, sp, $ffc0
	0x3c020000,		// lui		v0, PadDataAddrHi
	0xffb20020,		// sd		s2, $0020(sp)
	0x24420000,		// addiu	v0, v0, PadDataAddrLo
	0xffb10010,		// sd		s1, $0010(sp)
	0x00c0902d,		// daddu	s2, a2, zero
	0x00641821,		// addu		v1, v1, a0
	0xffbf0030,		// sd		ra, $0030(sp)
	0xffb00000,		// sd		s0, $0000(sp)
	0x00621821,		// addu		v1, v1, v0
	0x8c70000c,		// lw		s0, $000c(v1)
	0x24050100,		// li		a1, $00000100
	0x0c000000,		// jal		sceSifWriteBackDCache
};
static u32 padReadpattern5_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
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
	0xfc000000
};

/**************************************************************************
 * For libpad support
 *
 * libpad	2.0.0.0
 * libpad	2.0.5.0
 */
static u32 padReadpattern6[] = {
	0x24020060,		// li		v0, $00000060
	0x24030180,		// li		v1, $00000180
	0x00a22818,		// mult		a1, a1, v0
	0x70832018,		// mult1	a0, a0, v1
	0x27bdffe0,		// addiu	sp, sp, $ffe0
	0x3c020000,		// lui		v0, PadDataAddrHi
	0xffbf0010,		// sd		ra, $0010(sp)
	0x24420000,		// addiu	v0, v0, PadDataAddrLo
	0xffb00000,		// sd		s0, $0000(sp)
	0x00a42821,		// addu		a1, a1, a0
	0x00a22821,		// addu		a1, a1, v0
	0x8cb0000c,		// lw		s0, $000c(a1)
	0x0200202d,		// daddu	a0, s0, zero
	0x0c000000,		// jal		SyncDCache
};
static u32 padReadpattern6_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xfc000000
};

/**************************************************************************
 * For libpad support
 *
 * libpad          3.0.1.0
 * libpad          3.0.2.0
 */
static u32 padReadpattern7[] = {
	0x27bdffb0,		// addiu	sp, sp, $ffb0
	0xffb20020,		// sd		s2, $0020(sp)
	0xffb10010,		// sd		s1, $0010(sp)
	0x00c0902d,		// daddu	s2, a2, zero
	0xffb00000,		// sd		s0, $0000(sp)
	0x0080882d,		// daddu	s1, a0, zero
	0xffb30030,		// sd		s3, $0030(sp)
	0xffbf0040,		// sd		ra, $0040(sp)
	0x0c000000,		// jal		DI
	0x00a0802d,		// daddu	s0, a1, zero
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x00000000,		// ...
	0x0c000000,		// jal		scePadGetDmaStr
};
static u32 padReadpattern7_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xfc000000,
	0xffffffff,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xfc000000
};
