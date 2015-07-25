// Header For Per-Game GSM -Bat-

#ifndef __PGGSM_H
#define __PGGSM_H

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

int	gEnableGSM; // Enables GSM - 0 for Off, 1 for On
int	gGSMVMode;  // See the related predef_vmode
int	gGSMXOffset; // 0 - Off, Any other positive or negative value - Relative position for X Offset
int	gGSMYOffset; // 0 - Off, Any other positive or negative value - Relative position for Y Offset
int	gGSMSkipVideos; // 0 - Off, 1 - On

#endif
