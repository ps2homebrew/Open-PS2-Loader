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
#include <sifrpc.h>
#include <string.h>
#include <sbv_patches.h>
#include <smem.h>
#include <smod.h>

#ifdef __EESIO_DEBUG
#include <sio.h>
#define DPRINTF(args...)	sio_printf(args)
#define DINIT()			sio_init(38400, 0, 0, 0, 0)
#else
#define DPRINTF(args...)	do { } while(0)
#define DINIT()			do { } while(0)
#endif

u8 *g_buf;

extern int set_reg_hook;
extern int set_reg_disabled;
extern int iop_reboot_count;

extern int padOpen_hooked;

#define IPCONFIG_MAX_LEN	64
char g_ipconfig[IPCONFIG_MAX_LEN] __attribute__((aligned(64)));
int g_ipconfig_len;
char g_ps2_ip[16];
char g_ps2_netmask[16];
char g_ps2_gateway[16];
u32 g_compat_mask;

#define COMPAT_MODE_1 		0x01
#define COMPAT_MODE_2 		0x02
#define COMPAT_MODE_3 		0x04
#define COMPAT_MODE_4 		0x08
#define COMPAT_MODE_5 		0x10
#define COMPAT_MODE_6 		0x20
#define COMPAT_MODE_7 		0x40
#define COMPAT_MODE_8 		0x80

char GameID[16];
int GameMode;
#define USB_MODE 	0
#define ETH_MODE 	1
#define HDD_MODE 	2

char ExitPath[32];
int USBDelay;
int HDDSpindown;

int DisableDebug;
#define GS_BGCOLOUR *((volatile unsigned long int*)0x120000E0)

extern void (*InitializeTLB)(void);

#endif
