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

#ifndef PADHOOK_H
#define PADHOOK_H

#include <tamtypes.h>

#define PADOPEN_HOOK 0
#define PADOPEN_CHECK 1

int Install_PadOpen_Hook(u32 mem_start, u32 mem_end, int mode);
void IGR_Exit(s32 exit_code);

// DEV9 Registers
#define DEV9_R_1460 ((volatile u16 *)0xBF801460)
#define DEV9_R_1464 ((volatile u16 *)0xBF801464)
#define DEV9_R_1466 ((volatile u16 *)0xBF801466)
#define DEV9_R_146C ((volatile u16 *)0xBF80146C)
#define DEV9_R_146E ((volatile u16 *)0xBF80146E)
#define DEV9_R_1474 ((volatile u16 *)0xBF801474)


// CDVD Registers
#define CDVD_R_NDIN ((volatile u8 *)0xBF402005)
#define CDVD_R_POFF ((volatile u8 *)0xBF402008)
#define CDVD_R_SCMD ((volatile u8 *)0xBF402016)
#define CDVD_R_SDIN ((volatile u8 *)0xBF402017)


typedef struct
{
    u32 option;
    int port;
    int slot;
    int number;
    u8 name[16];
} pad2socketparam_t;

typedef struct
{
    u16 libpad;
    u16 libversion;
    u8 *pad_buf;
    int vb_count;
    int pos_combo1;
    int pos_combo2;
    int pos_state;
    int pos_frame;
    u8 combo_type;
    u8 prev_frame;
} paddata_t;

typedef struct
{
    int press;
    int vb_count;
} powerbuttondata_t;

typedef struct
{
    u32 *pattern;
    u32 *mask;
    int size;
    u16 type; //Whether it's libpad or libpad2
    u16 version;
} pattern_t;

#define IGR_LIBPAD 1
#define IGR_LIBPAD2 2

#define IGR_PAD_STABLE_V1 0x06
#define IGR_PAD_STABLE_V2 0x01

#define IGR_COMBO_R1_L1_R2_L2 0xF0
#define IGR_COMBO_START_SELECT 0xF6
#define IGR_COMBO_R3_L3 0xF9
#define IGR_COMBO_UP 0xEF

#define NB_PADOPEN_PATTERN 7

#endif
