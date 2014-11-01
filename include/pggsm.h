/* Header For Per-Game GSM - INCOMPLETE!

#ifndef __PGGSM_H
#define __PGGSM_H

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <string.h>
#include <loadfile.h>
#include <stdio.h>
#include <sbv_patches.h>
#include <libcdvd.h>
#include <libpad.h>
#include <libmc.h>
#include <debug.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <malloc.h>
#include <math.h>
#include <libpwroff.h>
#include <fileXio_rpc.h>
#include <smod.h>
#include <smem.h>
#include <debug.h>
#include <ps2smb.h>
#include "config.h"
#include "opl.h"
#ifdef VMC
#include <sys/fcntl.h>
#endif

/// DTV 576 Progressive Scan (720x576)
#define GS_MODE_DTV_576P  0x53

/// DTV 1080 Progressive Scan (1920x1080)
#define GS_MODE_DTV_1080P  0x54

#ifdef GSM
#define GSM_VERSION "0.36.R"

#define PS1_VMODE	1
#define SDTV_VMODE	2
#define HDTV_VMODE	3
#define VGA_VMODE	4

#define make_display_magic_number(dh, dw, magv, magh, dy, dx) \
	(((u64)(dh)<<44) | ((u64)(dw)<<32) | ((u64)(magv)<<27) | \
	((u64)(magh)<<23) | ((u64)(dy)<<12)   | ((u64)(dx)<<0)     )

typedef struct predef_vmode_struct {
	u8	category;
	char desc[34];
	u8	interlace;
	u8	mode;
	u8	ffmd;
	u64	display;
	u64	syncv;
} predef_vmode_struct;

int	EnableGSM; // Enables GSM - 0 for Off, 1 for On
int	GSMVMode;  // See the related predef_vmode
int	GSMXOffset; // 0 - Off, Any other positive or negative value - Relative position for X Offset
int	GSMYOffset; // 0 - Off, Any other positive or negative value - Relative position for Y Offset
int	GSMSkipVideos; // 0 - Off, 1 - On

#endif
*/ 
