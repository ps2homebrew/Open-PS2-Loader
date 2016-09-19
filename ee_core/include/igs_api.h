/*
 * PS2 IGS (InGame Screenshot)
 *
 * Copyright (C) 2010-2014 maximus32
 * Copyright (C) 2014,2015,2016 doctorxyz
 *
 * This file is part of PS2 IGS Feature.
 *
 * PS2 IGS Feature is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PS2 IGS Feature is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PS2 IGS.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id$
 */

#ifndef _IGSAPI_H_
#define _IGSAPI_H_

#include "fileio.h"
#include "ee_core.h"
#include <iopcontrol.h>
#include "iopmgr.h"
#include "modmgr.h"
#include "util.h"
#include <stdio.h>
#include <libmc.h>

#define MAXWAITTIME		0x1000000

//Channel #1 (VIF1)
#define D1_CHCR			(volatile u32 *)0x10009000
#define D1_MADR         (volatile u32 *)0x10009010
#define D1_SIZE         (volatile u32 *)0x10009020
#define D1_TADR         (volatile u32 *)0x10009030
#define D1_ASR0         (volatile u32 *)0x10009040
#define D1_ASR1         (volatile u32 *)0x10009050
#define D1_SADR         (volatile u32 *)0x10009080

#define VIF1_STAT		(volatile u32 *)0x10003C00
#define VIF1_FBRST      (volatile u32 *)0x10003C10
#define VIF1_FIFO       (volatile u128 *)0x10005000


//Channel #2 (GIF)
#define D2_CHCR			(volatile u32 *)0x1000A000
#define D2_MADR         (volatile u32 *)0x1000A010
#define D2_SIZE         (volatile u32 *)0x1000A020
#define D2_TADR         (volatile u32 *)0x1000A030
#define D2_ASR0         (volatile u32 *)0x1000A040
#define D2_ASR1         (volatile u32 *)0x1000A050
#define D2_SADR         (volatile u32 *)0x1000A080

#define GIF_CTRL		(volatile u32 *)0x10003000		// GIF Control Register
#define GIF_STAT		(volatile u32 *)0x10003020		// GIF Status Register

//GS Priviledge Registers
#define GS_CSR			(volatile u64 *)0x12001000		// GS CSR (GS System Status) Register
#define GS_IMR			(volatile u64 *)0x12001010		// GS IMR (GS Interrupt Mask) Register
#define GS_BUSDIR		(volatile u64 *)0x12001040		// GS BUSDIR (GS Bus Direction) Register

#define GS_BITBLTBUF    0x50
#define GS_TRXPOS       0x51
#define GS_TRXREG       0x52
#define GS_TRXDIR       0x53
#define GS_FINISH       0x61

#define GS_SET_BITBLTBUF(sbp, sbw, spsm, dbp, dbw, dpsm) \
    ((u64)(sbp)         | ((u64)(sbw) << 16) | \
    ((u64)(spsm) << 24) | ((u64)(dbp) << 32) | \
    ((u64)(dbw) << 48)  | ((u64)(dpsm) << 56))
#define GS_SET_TRXPOS(ssax, ssay, dsax, dsay, dir) \
    ((u64)(ssax)        | ((u64)(ssay) << 16) | \
    ((u64)(dsax) << 32) | ((u64)(dsay) << 48) | \
    ((u64)(dir) << 59))
#define GS_SET_TRXREG(rrw, rrh) \
    ((u64)(rrw) | ((u64)(rrh) << 32))
#define GS_SET_TRXDIR(xdr) ((u64)(xdr))

#define DPUT_D1_CHCR(x)		(*D1_CHCR = (x))
#define DPUT_D1_MADR(x)		(*D1_MADR = (x))
#define DPUT_D1_SIZE(x)		(*D1_SIZE = (x))
#define DPUT_D1_TADR(x)		(*D1_TADR = (x))

#define DGET_VIF1_STAT()	(*VIF1_STAT)
#define DGET_VIF1_FIFO()	(*VIF1_FIFO)
#define DPUT_VIF1_FBRST(x)	(*VIF1_FBRST = (x))
#define DPUT_VIF1_FIFO(x)	(*VIF1_FIFO = (x))

#define DGET_GIF_STAT()		(*GIF_STAT)
#define DPUT_GIF_CTRL(x)	(*GIF_CTRL = (x))

#define VIF1_NOP(irq)			((u32)(irq) << 31)
#define VIF1_MSKPATH3(msk, irq)	((u32)(msk) | ((u32)0x06 << 24) | ((u32)(irq) << 31))
#define VIF1_FLUSHA(irq)		(((u32)0x13 << 24) | ((u32)(irq) << 31))
#define VIF1_DIRECT(count, irq)	((u32)(count) | ((u32)(0x50) << 24) | ((u32)(irq) << 31))

#define GIF_CLEAR_TAG(tp)		(*(u128 *)(tp) = (u128)0)

#define DPUT_GS_CSR(x)          (*GS_CSR = (x))
#define DPUT_GS_IMR(x)          (*GS_IMR = (x))

/// R8 G8 B8 A8 (RGBA32) Texture
#define GS_PSM_CT32 0x00
/// R8 G8 B8 (RGB24) Texture
#define GS_PSM_CT24 0x01
/// RGBA16 Texture
#define GS_PSM_CT16 0x02
/// RGBA16 Texture ?
#define GS_PSM_CT16S 0x0A
/// 8 Bit Texture with CLUT
#define GS_PSM_T8 0x13
/// 8 Bit Texture with CLUT ?
#define GS_PSM_T8H 0x1B
/// 4 Bit Texture with CLUT
#define GS_PSM_T4 0x14
/// 4 Bit Texture with CLUT ?
#define GS_PSM_T4HL 0x24
/// 4 Bit Texture with CLUT ?
#define GS_PSM_T4HH 0x2C

#define GET_PMODE_EN1(x)	(u8)(( x >> 0  ) & 0x1  )
#define GET_PMODE_EN2(x)	(u8)(( x >> 1  ) & 0x1  )
#define GET_DISPFB_FBP(x)	(u32)(( x >> 0  ) & 0x1FF )
#define GET_DISPFB_FBW(x)	(u8)(( x >> 9  ) & 0x3F  )
#define GET_DISPFB_PSM(x)	(u8)(( x >> 15 ) & 0x1F  )
#define GET_DISPFB_DBX(x)	(u32)(( x >> 32 ) & 0x7FF )
#define GET_DISPFB_DBY(x)	(u32)(( x >> 43 ) & 0x7FF )
#define GET_DISPLAY_DH(x)	(u32)(( x >> 44 ) & 0x7FF )

#define BGR2RGB16(x)		(u16)( (x & 0x8000) | ((x << 10) & 0x7C00) | (x & 0x3E0) | ((x >> 10) & 0x1F) )
#define BGR2RGB32(x)		(u32)( ((x >> 16) & 0xFF) | ((x << 16) & 0xFF0000) | (x & 0xFF00FF00) )

typedef struct {
    u64 NLOOP:15;
    u64 EOP:1;
    u64 pad16:16;
    u64 id:14;
    u64 PRE:1;
    u64 PRIM:11;
    u64 FLG:2;
    u64 NREG:4;
    u64 REGS0:4;
    u64 REGS1:4;
    u64 REGS2:4;
    u64 REGS3:4;
    u64 REGS4:4;
    u64 REGS5:4;
    u64 REGS6:4;
    u64 REGS7:4;
    u64 REGS8:4;
    u64 REGS9:4;
    u64 REGS10:4;
    u64 REGS11:4;
    u64 REGS12:4;
    u64 REGS13:4;
    u64 REGS14:4;
    u64 REGS15:4;
} GifTag __attribute__((aligned(16)));

typedef struct {
    u64 SBP:14;
    u64 pad14:2;
    u64 SBW:6;
    u64 pad22:2;
    u64 SPSM:6;
    u64 pad30:2;
    u64 DBP:14;
    u64 pad46:2;
    u64 DBW:6;
    u64 pad54:2;
    u64 DPSM:6;
    u64 pad62:2;
} GsBitbltbuf;

typedef struct {
    u64 SSAX:11;
    u64 pad11:5;
    u64 SSAY:11;
    u64 pad27:5;
    u64 DSAX:11;
    u64 pad43:5;
    u64 DSAY:11;
    u64 DIR:2;
    u64 pad61:3;
} GsTrxpos;

typedef struct {
    u64 RRW:12;
    u64 pad12:20;
    u64 RRH:12;
    u64 pad44:20;
} GsTrxreg;

typedef struct {
    u64 pad00;
} GsFinish;

typedef struct {
    u64 XDR:2;
    u64 pad02:62;
} GsTrxdir;

typedef struct {
	u32 vifcode[4];
	GifTag giftag;
	GsBitbltbuf bitbltbuf;
	u64 bitbltbufaddr;
	GsTrxpos trxpos;
	u64 trxposaddr;
	GsTrxreg trxreg;
	u64 trxregaddr;
	GsFinish finish;
	u64 finishaddr;
	GsTrxdir trxdir;
	u64 trxdiraddr;
} GsStoreImage __attribute__((aligned(16)));

struct VModeSettings{
	unsigned int interlace;
	unsigned int mode;
	unsigned int ffmd;
};

struct GSRegisterValues{
	u64 pmode;
	u64 smode1;
	u64 smode2;
	u64 srfsh;
	u64 synch1;
	u64 synch2;
	u64 syncv;
	u64 dispfb1;
	u64 display1;
	u64 dispfb2;
	u64 display2;
};

extern struct VModeSettings Source_VModeSettings;
extern struct GSRegisterValues Source_GSRegisterValues;

// BMP File Structure and example
// https://www.siggraph.org/education/materials/HyperVis/asp_data/compimag/bmpfile.htm
// http://paulbourke.net/dataformats/bmp/
// http://atlc.sourceforge.net/bmp.html
// Example: 24bit 16 x 8 An ordinay Logo (An Yellow "M" Word Surrounded by Red)

typedef struct {
	                      /* 42 4D        = "BM"                                                                                                                           */
	u32 filesize;         /* B6 01 00 00  = 438 bytes = 384 bytes (image) + 54 bytes (header)                                                                              */
	u32 reserved;         /* 00 00 00 00                                                                                                                                   */
	u32 headersize;       /* 36 00 00 00  = 54 bytes                                                                                                                       */
	u32 infoSize;         /* 28 00 00 00  = 40 bytes = 54 bytes (header size) - 14 bytes                                                                                   */
	u32 width;            /* 10 00 00 00  = 16 pixels                                                                                                                      */
	u32 depth;            /* 08 00 00 00  = 8 pixels                                                                                                                       */
	u16 biPlanes;         /* 01 00        = 1 (for 24 bit images)                                                                                                          */
	u16 bits;             /* 18 00        = 24 (for 24 bit images)                                                                                                         */
	u32 biCompression;    /* 00 00 00 00  = 0 (no compression)                                                                                                             */
	u32 biSizeImage;      /* 80 01 00 00  = Image Size = width x (bits/8) x height = 16 x 8 x 3 = 384 bytes                                                                */
	u32 biXPelsPerMeter;  /* 00 00 00 00                                                                                                                                   */
	u32 biYPelsPerMeter;  /* 00 00 00 00                                                                                                                                   */
	u32 biClrUsed;        /* 00 00 00 00                                                                                                                                   */
	u32 biClrImportant;   /* 00 00 00 00                                                                                                                                   */
} BmpHeader;
                        /* Image example                                                                                                                                   */
                        /* 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED */
                        /* 24 1C ED 24 1C ED 00 F2 FF 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 00 F2 FF 24 1C ED 24 1C ED */
                        /* 24 1C ED 24 1C ED 00 F2 FF 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 00 F2 FF 24 1C ED 24 1C ED */
                        /* 24 1C ED 24 1C ED 00 F2 FF 00 F2 FF 24 1C ED 24 1C ED 24 1C ED 00 F2 FF 00 F2 FF 24 1C ED 24 1C ED 24 1C ED 24 1C ED 00 F2 FF 24 1C ED 24 1C ED */
                        /* 24 1C ED 24 1C ED 24 1C ED 00 F2 FF 24 1C ED 24 1C ED 00 F2 FF 00 F2 FF 00 F2 FF 00 F2 FF 24 1C ED 24 1C ED 24 1C ED 00 F2 FF 24 1C ED 24 1C ED */
                        /* 24 1C ED 24 1C ED 24 1C ED 00 F2 FF 00 F2 FF 24 1C ED 00 F2 FF 24 1C ED 24 1C ED 00 F2 FF 00 F2 FF 00 F2 FF 00 F2 FF 00 F2 FF 24 1C ED 24 1C ED */
                        /* 24 1C ED 24 1C ED 24 1C ED 24 1C ED 00 F2 FF 00 F2 FF 00 F2 FF 24 1C ED 24 1C ED 24 1C ED 24 1C ED 00 F2 FF 00 F2 FF 24 1C ED 24 1C ED 24 1C ED */
                        /* 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED 24 1C ED */

#define _IGS_ENGINE_	__attribute__((section(".igs_engine")))

int InGameScreenshot(void) _IGS_ENGINE_;

#endif /* _IGSAPI_H_ */
