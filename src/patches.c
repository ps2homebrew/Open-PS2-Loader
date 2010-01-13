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

typedef struct {
	char *game;
	int mode;
	game_patch_t patch;
} patchlist_t;

static patchlist_t patch_list[5] = {
	{ "SLES_524.58", USB_MODE, { 0x00122244, 0x100000a0, 0x142000a0 }}, // Disgaea Hour of Darkness PAL - disable cdvd timeout stuff
	{ "SLES_524.58", ETH_MODE, { 0x001f8d38, 0x3c02000d, 0x3c02000e }}, // Disgaea Hour of Darkness PAL - reduce buffer allocated on IOP from 921600 bytes
	{ "SLES_524.58", ETH_MODE, { 0x001f8d3c, 0x34449000, 0x34441000 }}, // to 888832 bytes allowing modules to not conflict with IOP ram usage
	{ "SLUS_212.00", USB_MODE, { 0x0014a834, 0x00000000, 0x1040fff6 }}, // Armored Core Nine Breaker NTSC U - skip failing case on binding a RPC server
	{ NULL, 0, { 0, 0, 0 }}												// terminater
};

static int tbl_offset = 0;
game_patch_t patches_tbl[MAX_PATCHES];

void clear_patches_table(void)
{
	tbl_offset = 0;
	memset(&patches_tbl[0], 0, sizeof(patches_tbl));

	DIntr();
	ee_kmode_enter();
	memset((void *)PATCH_TABLE_ADDR, 0, sizeof(game_patch_t) * (MAX_PATCHES+1));
	ee_kmode_exit();
	EIntr();
}

void fill_patches_table(char *game, int mode)
{
	patchlist_t *p = (patchlist_t *)&patch_list[0];

	while (p->game) {
		if ((!strcmp(game, p->game)) && (mode == p->mode)) {
			patches_tbl[tbl_offset].addr = p->patch.addr;
			patches_tbl[tbl_offset].val = p->patch.val;
			patches_tbl[tbl_offset++].check = p->patch.check;
		}
		p++;
	}

	DIntr();
	ee_kmode_enter();
	memcpy((void *)PATCH_TABLE_ADDR, &patches_tbl[0], sizeof(game_patch_t) * MAX_PATCHES);
	ee_kmode_exit();
	EIntr();
}
