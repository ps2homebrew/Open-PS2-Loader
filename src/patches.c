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

void clear_game_patches(u32 patch_table_addr)
{
	tbl_offset = 0;
	memset(&patches_tbl[0], 0, MAX_PATCHES+1);
}

void apply_game_patches(u32 patch_table_addr, char *game, int mode)
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

	memcpy((void *)patch_table_addr, &patches_tbl[0], MAX_PATCHES);
}
