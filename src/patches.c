/*
  Copyright 2009, Ifcaro
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"

#define MAX_PATCHES		63

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

static patchlist_t patch_list[24] = {
	{ "SLES_524.58", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Disgaea Hour of Darkness PAL - disable cdvd timeout stuff
	{ "SLES_524.58", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Disgaea Hour of Darkness PAL - reduce snd buffer allocated on IOP
	{ "SLUS_206.66", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Disgaea Hour of Darkness NTSC U - disable cdvd timeout stuff
	{ "SLUS_206.66", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Disgaea Hour of Darkness NTSC U - reduce snd buffer allocated on IOP
	{ "SLPS_202.51", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Makai Senki Disgaea NTSC J - disable cdvd timeout stuff
	{ "SLPS_202.51", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Makai Senki Disgaea NTSC J - reduce snd buffer allocated on IOP
	{ "SLPS_202.50", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Makai Senki Disgaea (limited edition) NTSC J - disable cdvd timeout stuff
	{ "SLPS_202.50", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Makai Senki Disgaea (limited edition) NTSC J - reduce snd buffer allocated on IOP
	{ "SLPS_731.03", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Makai Senki Disgaea (PlayStation2 the Best) NTSC J - disable cdvd timeout stuff
	{ "SLPS_731.03", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Makai Senki Disgaea (PlayStation2 the Best) NTSC J - reduce snd buffer allocated on IOP
	{ "SLES_529.51", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave PAL - disable cdvd timeout stuff
	{ "SLES_529.51", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave PAL - reduce snd buffer allocated on IOP
	{ "SLUS_209.55", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave NTSC U - disable cdvd timeout stuff
	{ "SLUS_209.55", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave NTSC U - reduce snd buffer allocated on IOP
	{ "SLPS_203.45", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave NTSC J - disable cdvd timeout stuff
	{ "SLPS_203.45", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave NTSC J - reduce snd buffer allocated on IOP
	{ "SLPS_203.44", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave (limited edition) NTSC J - disable cdvd timeout stuff
	{ "SLPS_203.44", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave (limited edition) NTSC J - reduce snd buffer allocated on IOP
	{ "SLPS_731.08", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave: 2-shuume Hajime Mashita (PlayStation 2 the Best) NTSC J - disable cdvd timeout stuff
	{ "SLPS_731.08", ETH_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave: 2-shuume Hajime Mashita (PlayStation 2 the Best) NTSC J - reduce snd buffer allocated on IOP	
	{ "SLUS_212.00", USB_MODE, { 0xdeadbee1, 0x00000000, 0x00000000 }}, // Armored Core Nine Breaker NTSC U - skip failing case on binding a RPC server
	{ "SLES_538.19", USB_MODE, { 0xdeadbee1, 0x00000000, 0x00000000 }}, // Armored Core Nine Breaker PAL - skip failing case on binding a RPC server
	{ "SLPS_254.08", USB_MODE, { 0xdeadbee1, 0x00000000, 0x00000000 }}, // Armored Core Nine Breaker NTSC J - skip failing case on binding a RPC server
	{ NULL, 0, { 0, 0, 0 }}												// terminater
};

static int tbl_offset = 0;
game_patch_t patches_tbl[MAX_PATCHES];

static u32 NIScdtimeoutpattern[] = { // @0x800304c0 - 0x28
	0x3c010000,
	0x8c230000,
	0x24630001,
	0x3c010000,
	0xac230000,
	0x3c010000,
	0x8c230000,
	0x2861037b,
	0x14200000,
	0x00000000
};
static u32 NIScdtimeoutpattern_mask[] = { // @0x800304e8 - 0x28
	0xffff0000,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffff0000,
	0xffff0000,
	0xffff0000,
	0xffffffff,
	0xffff0000,
	0xffffffff
};

static u32 NISsndiopmemallocpattern[] = { // @0x80030510 - 0x28
	0x8fa20030,
	0x2443003f,
	0x2402ffc0,
	0x00621024,
	0x3c010000,
	0xac220000,
	0x3c020000,
	0x34440000,
	0x0c000000,
	0x00000000
};
static u32 NISsndiopmemallocpattern_mask[] = { // @0x80030538 - 0x28
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffff0000,
	0xffff0000,
	0xffff0000,
	0xfc000000,
	0xffffffff
};

static u32 AC9Bpattern[] = { // @0x80030700 - 0x28 
	0x8e450000,
	0x0220202d,
	0x0c000000,
	0x0000302d,
	0x04410003,
	0x00000000,
	0x10000005,
	0x2402ffff,
	0x8e020000,
	0x1040fff6
};
static u32 AC9Bpattern_mask[] = { // @0x80030728 - 0x28
	0xffffffff,
	0xffffffff,
	0xfc000000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff
};

static u8 *find_pattern_with_mask(u8 *buf, u32 bufsize, u8 *bytes, u8 *mask, u32 len); // @0x80030410 - 0xb0
static void NIS_generic_patches(void); // @0x80030560 - 0x1a0
static void AC9B_generic_patches(void); // @0x80030750 - 0xb0
static void apply_game_patches(void); // @0x80030800

void clear_patches_code(void)
{
	tbl_offset = 0;
	memset(&patches_tbl[0], 0, sizeof(patches_tbl));

	DIntr();
	ee_kmode_enter();
	memset((void *)0x80030000, 0, 0x1000);
	ee_kmode_exit();
	EIntr();
}

void fill_patches_code(char *game, int mode)
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

	memcpy((void *)0x80030000, (void *)&patches_tbl[0], sizeof(game_patch_t) * MAX_PATCHES);

	memcpy((void *)0x80030400, (void *)&mode, 4);
	memcpy((void *)0x80030410, (void *)find_pattern_with_mask, 0xb0);

	memcpy((void *)0x800304c0, (void *)NIScdtimeoutpattern, 0x28);
	memcpy((void *)0x800304e8, (void *)NIScdtimeoutpattern_mask, 0x28);
	memcpy((void *)0x80030510, (void *)NISsndiopmemallocpattern, 0x28);
	memcpy((void *)0x80030538, (void *)NISsndiopmemallocpattern_mask, 0x28);
	memcpy((void *)0x80030560, (void *)NIS_generic_patches, 0x1a0);

	memcpy((void *)0x80030700, (void *)AC9Bpattern, 0x28);
	memcpy((void *)0x80030728, (void *)AC9Bpattern_mask, 0x28);
	memcpy((void *)0x80030750, (void *)AC9B_generic_patches, 0xb0);

	memcpy((void *)0x80030800, (void *)apply_game_patches, 0x200);

	ee_kmode_exit();
	EIntr();
}

// All of the following datas/functions must be copied into Kernel ram
// at the address specified for each one.
// To perform the patching, the main patch function (apply_game_patches())
// must be called in kernel mode.

// int *game_mode = (int *)0x80030400;

static u8 *find_pattern_with_mask(u8 *buf, u32 bufsize, u8 *bytes, u8 *mask, u32 len) // @0x80030410 - 0xb0
{
	register u32 i, j;

	for (i = 0; i < bufsize - len; i++) {
		for (j = 0; j < len; j++) {
			if ((buf[i + j] & mask[j]) != bytes[j])
				break;
		}
		if (j == len)
			return &buf[i];
	}

	return NULL;
}

static void NIS_generic_patches(void) // @0x80030560 - 0x1a0
{
	u8 *ptr;
	int *game_mode = (int *)0x80030400;
	u8 *pNIScdtimeoutpattern = (u8 *)0x800304c0;
	u8 *pNIScdtimeoutpattern_mask = (u8 *)0x800304e8;
	u8 *pNISsndiopmemallocpattern = (u8 *)0x80030510;
	u8 *pNISsndiopmemallocpattern_mask = (u8 *)0x80030538;
	u8 *(*_find_pattern_with_mask)(u8 *buf, u32 bufsize, u8 *bytes, u8 *mask, u32 len);
	_find_pattern_with_mask = (void *)0x80030410;
	
	if (*game_mode == USB_MODE) { // Nippon Ichi Sofwtare games generic patch to disable cdvd timeout
		ptr = _find_pattern_with_mask((u8 *)0x100000, 0x01f00000 - 0x100000, pNIScdtimeoutpattern, pNIScdtimeoutpattern_mask, 40);
		if (ptr) {
			u16 jmp = _lw((u32)ptr+32) & 0xffff;
			_sw(0x10000000|jmp, (u32)ptr+32);
		}
	}
	else if (*game_mode == ETH_MODE) { // Nippon Ichi Sofwtare games generic patch to lower memory allocation for sounds
		ptr = _find_pattern_with_mask((u8 *)0x100000, 0x01f00000 - 0x100000, pNISsndiopmemallocpattern, pNISsndiopmemallocpattern_mask, 40);
		if (ptr) {
			u16 val = _lw((u32)ptr+24) & 0xffff;
			u16 val2 = _lw((u32)ptr+28) & 0xffff;
			if (val2 < 0x8000) {
				_sw(0x3c020000|(val-1), (u32)ptr+24);
				_sw(0x34440000|(val2+0x8000), (u32)ptr+28);
			}
			else
				_sw(0x34440000|(val2-0x8000), (u32)ptr+28);
		}		
	}
}

static void AC9B_generic_patches(void) // @0x80030750 - 0xb0
{
	u8 *ptr;
	int *game_mode = (int *)0x80030400;
	u8 *pAC9Bpattern = (u8 *)0x80030700;
	u8 *pAC9Bpattern_mask = (u8 *)0x80030728;
	u8 *(*_find_pattern_with_mask)(u8 *buf, u32 bufsize, u8 *bytes, u8 *mask, u32 len);
	_find_pattern_with_mask = (void *)0x80030410;

	if (*game_mode == USB_MODE) { // Armored Core 9 Breaker generic USB patch
		ptr = _find_pattern_with_mask((u8 *)0x100000, 0x01f00000 - 0x100000, pAC9Bpattern, pAC9Bpattern_mask, 40);
		if (ptr)
			_sw(0, (u32)ptr+36);
	}
}

static void apply_game_patches(void) // @0x80030800
{
	game_patch_t *p = (game_patch_t *)0x80030000;
	void (*_NIS_generic_patches)(void);
	_NIS_generic_patches = (void *)0x80030560;
	void (*_AC9B_generic_patches)(void);
	_AC9B_generic_patches = (void *)0x80030750;

	while (p->addr) {

		// testing first for generic patches case
		if (p->addr == 0xdeadbee0)
			_NIS_generic_patches(); 	// Nippon Ichi Software games generic patch
		else if (p->addr == 0xdeadbee1)
			_AC9B_generic_patches(); 	// Armored Core 9 Breaker USB generic patch
		// non-generic patches
		else if (_lw(p->addr) == p->check)
			_sw(p->val, p->addr);

		p++;
	}
}
