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
} game_patch_t;

static int tbl_offset = 0;
game_patch_t patches_tbl[MAX_PATCHES];

void clear_game_patches(void)
{
	tbl_offset = 0;
	memset(&patches_tbl[0], 0, sizeof(patches_tbl));

	DIntr();
	ee_kmode_enter();
	memset((void *)PATCH_TABLE_ADDR, 0, (MAX_PATCHES+1)<<2);
	ee_kmode_exit();
	EIntr();
}

void apply_game_patches(char *game, int mode)
{
	if (!strcmp(game, "SLES_524.58")) { // Disgaea Hour of Darkness PAL
		if (mode == USB_MODE) {
			patches_tbl[tbl_offset].addr = 0x00122244; 	// disable cdvd timeout stuff
			patches_tbl[tbl_offset++].val = 0x100000a0;
		}
		else if (mode == ETH_MODE) {
			patches_tbl[tbl_offset].addr = 0x001f8d38; 	// reduce buffer allocated on IOP from 921600 bytes
			patches_tbl[tbl_offset++].val = 0x3c02000d;	// to 851968 bytes thus allowing ethernet modules to
			patches_tbl[tbl_offset].addr = 0x001f8d3c;	// load properly
			patches_tbl[tbl_offset++].val = 0x34449000;
		}
	}

	DIntr();
	ee_kmode_enter();
	memcpy((void *)PATCH_TABLE_ADDR, &patches_tbl[0], MAX_PATCHES<<2);
	ee_kmode_exit();
	EIntr();
}
