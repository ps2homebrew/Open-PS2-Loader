/*
  Copyright 2009, Ifcaro
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"

#define MAX_PATCHES		127

typedef struct {
	u32 addr;
	u32 val;
	u32 check;
} game_patch_t;

static int tbl_offset = 0;
game_patch_t patches_tbl[MAX_PATCHES];

void clear_game_patches(void)
{
	tbl_offset = 0;
	memset(&patches_tbl[0], 0, sizeof(patches_tbl));

	DIntr();
	ee_kmode_enter();
	memset((void *)PATCH_TABLE_ADDR, 0, sizeof(game_patch_t) * (MAX_PATCHES+1));
	ee_kmode_exit();
	EIntr();
}

void apply_game_patches(char *game, int mode)
{
	if (!strcmp(game, "SLES_524.58")) { // Disgaea Hour of Darkness PAL
		if (mode == USB_MODE) {
			patches_tbl[tbl_offset].addr = 0x00122244; 		// disable cdvd timeout stuff
			patches_tbl[tbl_offset].val = 0x100000a0;
			patches_tbl[tbl_offset++].check = 0x142000a0;
		}
		else if (mode == ETH_MODE) {
			patches_tbl[tbl_offset].addr = 0x001f8d38; 		// reduce buffer allocated on IOP from 921600 bytes
			patches_tbl[tbl_offset].val = 0x3c02000d;		// to 888832 bytes thus allowing ethernet modules to
			patches_tbl[tbl_offset++].check = 0x3c02000e;	// load properly
			patches_tbl[tbl_offset].addr = 0x001f8d3c;
			patches_tbl[tbl_offset].val = 0x34449000;
			patches_tbl[tbl_offset++].check = 0x34441000;
		}
	}
	else if (!strcmp(game, "SLUS_212.00")) { // Armored Core Nine Breaker NTSC U
		if (mode == USB_MODE) {
			patches_tbl[tbl_offset].addr = 0x0014a834;		// Skip failing case on binding a RPC server
			patches_tbl[tbl_offset].val = 0x00000000;
			patches_tbl[tbl_offset++].check = 0x1040fff6;
		}
	}

	DIntr();
	ee_kmode_enter();
	memcpy((void *)PATCH_TABLE_ADDR, &patches_tbl[0], sizeof(game_patch_t) * MAX_PATCHES);
	ee_kmode_exit();
	EIntr();
}
