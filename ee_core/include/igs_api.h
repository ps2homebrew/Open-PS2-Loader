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

#include "ee_core.h"
#include <iopcontrol.h>
#include "iopmgr.h"
#include "modmgr.h"
#include "util.h"
#include <stdio.h>

#define GS_BITBLTBUF 0x50
#define GS_TRXPOS    0x51
#define GS_TRXREG    0x52
#define GS_TRXDIR    0x53
#define GS_FINISH    0x61

// Pixel storage format
#define GS_PSM_CT32  0x00 // 32 bits = 4 bytes    Example: SLUS_202.73 (Namco Museum 50th Anniversary)
#define GS_PSM_CT24  0x01 // 24 bits = 3 bytes    Example: SLPS_250.88 (Final Fantasy X International)
#define GS_PSM_CT16  0x02 // 16 bits = 2 bytes    Example: SLUS_215.56 (Konami Kids Playground: Dinosaurs Shapes & Colors)
#define GS_PSM_CT16S 0x0A // 16 bits = 2 bytes    Example: SLUS_215.41 (Ratatouille)

// Channel #1 (VIF1)
#define GS_VIF1_STAT           ((volatile u32 *)(0x10003c00)) // VIF Status Register
#define GS_VIF1_STAT_FDR       (1 << 23)                      // VIF1-FIFO transfer direction: VIF1 -> Main memory/SPRAM"
#define GS_VIF1_MSKPATH3(mask) ((u32)(mask) | ((u32)0x06 << 24))
#define GS_VIF1_NOP            0
#define GS_VIF1_FLUSHA         (((u32)0x13 << 24))
#define GS_VIF1_DIRECT(count)  ((u32)(count) | ((u32)(0x50) << 24))
#define GS_VIF1_FIFO           ((volatile u128 *)(0x10005000))

// DMA CH1 REGISTERS (Linked to VIF1)
#define GS_D1_CHCR ((volatile u32 *)(0x10009000))
#define GS_D1_MADR ((volatile u32 *)(0x10009010))
#define GS_D1_QWC  ((volatile u32 *)(0x10009020))
#define GS_D1_TADR ((volatile u32 *)(0x10009030))
#define GS_D1_ASR0 ((volatile u32 *)(0x10009040))
#define GS_D1_ASR1 ((volatile u32 *)(0x10009050))

// Channel #2 (GIF)
#define GS_GIF_AD 0x0e

#define GS_GIFTAG(NLOOP, EOP, PRE, PRIM, FLG, NREG) \
    ((u64)(NLOOP) << 0) |                           \
        ((u64)(EOP) << 15) |                        \
        ((u64)(PRE) << 46) |                        \
        ((u64)(PRIM) << 47) |                       \
        ((u64)(FLG) << 58) |                        \
        ((u64)(NREG) << 60)

// GS Registers
#define GS_GSBITBLTBUF_SET(sbp, sbw, spsm, dbp, dbw, dpsm) \
    ((u64)(sbp) | ((u64)(sbw) << 16) |                     \
     ((u64)(spsm) << 24) | ((u64)(dbp) << 32) |            \
     ((u64)(dbw) << 48) | ((u64)(dpsm) << 56))

#define GS_GSTRXREG_SET(rrw, rrh) \
    ((u64)(rrw) | ((u64)(rrh) << 32))

#define GS_GSTRXPOS_SET(ssax, ssay, dsax, dsay, dir) \
    ((u64)(ssax) | ((u64)(ssay) << 16) |             \
     ((u64)(dsax) << 32) | ((u64)(dsay) << 48) |     \
     ((u64)(dir) << 59))

#define GS_GSTRXDIR_SET(xdr) ((u64)(xdr))

#define GS_GSBITBLTBUF 0x50
#define GS_GSFINISH    0x61
#define GS_GSTRXPOS    0x51
#define GS_GSTRXREG    0x52
#define GS_GSTRXDIR    0x53

// GS Priviledge Registers
#define GS_CSR_FINISH (1 << 1)
#define GS_CSR        (volatile u64 *)0x12001000 // GS CSR (GS System Status) Register
#define GS_IMR        (volatile u64 *)0x12001010 // GS IMR (GS Interrupt Mask) Register
#define GS_BUSDIR     (volatile u64 *)0x12001040 // GS BUSDIR (GS Bus Direction) Register

#define GET_PMODE_EN1(x)      (u8)((x >> 0) & 0x1)
#define GET_PMODE_EN2(x)      (u8)((x >> 1) & 0x1)
#define GET_SMODE2_INT(x)     (u8)((x >> 0) & 0x1)
#define GET_SMODE2_FFMD(x)    (u8)((x >> 1) & 0x1)
#define GET_SMODE2_DPMS(x)    (u8)((x >> 2) & 0x3)
#define GET_SMODE2_INTFFMD(x) (u8)((x >> 0) & 0x3)
#define GET_DISPFB_FBP(x)     (u16)((x >> 0) & 0x1FF)
#define GET_DISPFB_FBW(x)     (u8)((x >> 9) & 0x3F)
#define GET_DISPFB_PSM(x)     (u8)((x >> 15) & 0x1F)
#define GET_DISPFB_DBX(x)     (u16)((x >> 32) & 0x7FF)
#define GET_DISPFB_DBY(x)     (u16)((x >> 43) & 0x7FF)
#define GET_DISPLAY_DH(x)     (u16)((x >> 44) & 0x7FF)

#define GS_WRITEBACK_DCACHE 0

// GSM Stuff
struct GSMSourceSetGsCrt
{
    s16 interlace;
    s16 mode;
    s16 ffmd;
} __attribute__((packed));

struct GSMSourceGSRegs
{
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
} __attribute__((packed));

extern struct GSMSourceSetGsCrt GSMSourceSetGsCrt;
extern struct GSMSourceGSRegs GSMSourceGSRegs;

// BMP File Structure and example
// https://www.siggraph.org/education/materials/HyperVis/asp_data/compimag/bmpfile.htm
// http://paulbourke.net/dataformats/bmp/
// http://atlc.sourceforge.net/bmp.html
// Example: 24bit 16 x 8 An ordinay Logo (An Yellow "M" Word Surrounded by Red)

typedef struct
{
    /* 42 4D        = "BM"                                                                                                                           */
    u32 filesize;        /* B6 01 00 00  = 438 bytes = 384 bytes (image) + 54 bytes (header)                                                                              */
    u32 reserved;        /* 00 00 00 00                                                                                                                                   */
    u32 headersize;      /* 36 00 00 00  = 54 bytes                                                                                                                       */
    u32 infoSize;        /* 28 00 00 00  = 40 bytes = 54 bytes (header size) - 14 bytes                                                                                   */
    u32 width;           /* 10 00 00 00  = 16 pixels                                                                                                                      */
    u32 depth;           /* 08 00 00 00  = 8 pixels                                                                                                                       */
    u16 biPlanes;        /* 01 00        = 1 (for 24 bit images)                                                                                                          */
    u16 bits;            /* 18 00        = 24 (for 24 bit images)                                                                                                         */
    u32 biCompression;   /* 00 00 00 00  = 0 (no compression)                                                                                                             */
    u32 biSizeImage;     /* 80 01 00 00  = Image Size = width x (bits/8) x height = 16 x 8 x 3 = 384 bytes                                                                */
    u32 biXPelsPerMeter; /* 00 00 00 00                                                                                                                                   */
    u32 biYPelsPerMeter; /* 00 00 00 00                                                                                                                                   */
    u32 biClrUsed;       /* 00 00 00 00                                                                                                                                   */
    u32 biClrImportant;  /* 00 00 00 00                                                                                                                                   */
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

int InGameScreenshot(void);

#endif /* _IGSAPI_H_ */
