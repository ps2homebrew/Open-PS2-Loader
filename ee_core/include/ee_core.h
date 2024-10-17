/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#ifndef _LOADER_H_
#define _LOADER_H_

#include <tamtypes.h>
#include <kernel.h>
#include <stdio.h>
#include <iopheap.h>
#include <ps2lib_err.h>
#include <sifrpc.h>
#include <string.h>
#include <sbv_patches.h>
#include <smem.h>
#include <smod.h>
#include <stdbool.h>


#ifdef __EESIO_DEBUG
#define DPRINTF(args...) _print(args);
#define DINIT()          InitDebug();
#else
#define DPRINTF(args...) \
    do {                 \
    } while (0)
#define DINIT() \
    do {        \
    } while (0)
#endif

extern int set_reg_hook;
extern int set_reg_disabled;
extern int iop_reboot_count;

extern int padOpen_hooked;

enum ETH_OP_MODES {
    ETH_OP_MODE_AUTO = 0,
    ETH_OP_MODE_100M_FDX,
    ETH_OP_MODE_100M_HDX,
    ETH_OP_MODE_10M_FDX,
    ETH_OP_MODE_10M_HDX,

    ETH_OP_MODE_COUNT
};

#define IPCONFIG_MAX_LEN 64
extern char g_ipconfig[IPCONFIG_MAX_LEN];
extern int g_ipconfig_len;
extern u32 g_compat_mask;

#define COMPAT_MODE_1 0x01
#define COMPAT_MODE_2 0x02
#define COMPAT_MODE_3 0x04
#define COMPAT_MODE_4 0x08
#define COMPAT_MODE_5 0x10
#define COMPAT_MODE_6 0x20
#define COMPAT_MODE_7 0x40
#define COMPAT_MODE_8 0x80

enum GAME_MODE {
    BDM_ILK_MODE,
    BDM_M4S_MODE,
    BDM_USB_MODE,
    ETH_MODE,
    HDD_MODE,
};

extern int EnableDebug;
extern void BlinkColour(u8 x, u32 colour, u8 forever);
#define GS_BGCOLOUR                                                 *((volatile unsigned long int *)0x120000E0)
#define DBGCOL(color, type, description)                            GS_BGCOLOUR = color                      // this wrapper macro only serves the purpose of allowing us to grep from outside for documentation
#define BGCOLND(color)                                              GS_BGCOLOUR = color                      // same as DBGCOL() but this one is not passed to debug color documentation. used for blinking or setting screen to black wich usually means nothing
#define DBGCOL_BLNK(blinkCount, colour, forever, type, description) BlinkColour(blinkCount, colour, forever) //same as DBGCOL() but this one includes screen blinking effect


#endif
