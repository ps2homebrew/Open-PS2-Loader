/*
  Copyright 2009, Ifcaro, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "loader.h"

#define ALL_MODE		-1

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

static patchlist_t patch_list[16] = {
	{ "SLES_524.58", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Disgaea Hour of Darkness PAL - disable cdvd timeout stuff
	{ "SLUS_206.66", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Disgaea Hour of Darkness NTSC U - disable cdvd timeout stuff
	{ "SLPS_202.51", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Makai Senki Disgaea NTSC J - disable cdvd timeout stuff
	{ "SLPS_202.50", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Makai Senki Disgaea (limited edition) NTSC J - disable cdvd timeout stuff
	{ "SLPS_731.03", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Makai Senki Disgaea (PlayStation2 the Best) NTSC J - disable cdvd timeout stuff
	{ "SLES_529.51", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave PAL - disable cdvd timeout stuff
	{ "SLUS_209.55", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave NTSC U - disable cdvd timeout stuff
	{ "SLPS_203.45", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave NTSC J - disable cdvd timeout stuff
	{ "SLPS_203.44", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave (limited edition) NTSC J - disable cdvd timeout stuff
	{ "SLPS_731.08", USB_MODE, { 0xdeadbee0, 0x00000000, 0x00000000 }}, // Phantom Brave: 2-shuume Hajime Mashita (PlayStation 2 the Best) NTSC J - disable cdvd timeout stuff
	{ "SLUS_212.00", USB_MODE, { 0xdeadbee1, 0x00000000, 0x00000000 }}, // Armored Core Nine Breaker NTSC U - skip failing case on binding a RPC server
	{ "SLES_538.19", USB_MODE, { 0xdeadbee1, 0x00000000, 0x00000000 }}, // Armored Core Nine Breaker PAL - skip failing case on binding a RPC server
	{ "SLPS_254.08", USB_MODE, { 0xdeadbee1, 0x00000000, 0x00000000 }}, // Armored Core Nine Breaker NTSC J - skip failing case on binding a RPC server
	{ "SLUS_210.05", ALL_MODE, { 0xdeadbee2, 0x00100000, 0x001ac514 }}, // Kingdom Hearts 2 NTSC U - Gummi mission freezing fix (check addr is where to patch,
									    // val is the amount of delay cycles)
	{ "SLUS_202.30", ALL_MODE, { 0x00132d14, 0x10000018, 0x0c046744 }}, // Max Payne NTSC U - skip IOP reset before to exec demo elfs
	{ NULL,                 0, { 0x00000000, 0x00000000, 0x00000000 }}  // terminater
};

static u32 NIScdtimeoutpattern[] = {
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
static u32 NIScdtimeoutpattern_mask[] = {
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

static u32 AC9Bpattern[] = {
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
static u32 AC9Bpattern_mask[] = {
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

#define	JAL(addr)	(0x0c000000 | ((addr & 0x03ffffff) >> 2))
#define	FNADDR(jal)	((jal & 0x03ffffff) << 2)

static int (*cdRead)(u32 lsn, u32 nsectors, void *buf, int *mode);
static u32 g_delay_cycles;


static void NIS_generic_patches(void)
{
	u32 *ptr;

	if (GameMode == USB_MODE) { // Nippon Ichi Sofwtare games generic patch to disable cdvd timeout
		ptr = find_pattern_with_mask((u32 *)0x100000, 0x01e00000, NIScdtimeoutpattern, NIScdtimeoutpattern_mask, 0x28);
		if (ptr) {
			u16 jmp = _lw((u32)ptr+32) & 0xffff;
			_sw(0x10000000|jmp, (u32)ptr+32);
		}
	}
}

static void AC9B_generic_patches(void)
{
	u32 *ptr;

	if (GameMode == USB_MODE) { // Armored Core 9 Breaker generic USB patch
		ptr = find_pattern_with_mask((u32 *)0x100000, 0x01e00000, AC9Bpattern, AC9Bpattern_mask, 0x28);
		if (ptr)
			_sw(0, (u32)ptr+36);
	}
}

static int delayed_cdRead(u32 lsn, u32 nsectors, void *buf, int *mode)
{
	register int r;
	register u32 count;

	r = cdRead(lsn, nsectors, buf, mode);
	count = g_delay_cycles;
	while(count--)
		asm("nop\nnop\nnop\nnop");

	return r;
}

static void KH2_generic_patches(u32 patch_addr, u32 delay_cycles)
{
	// set configureable delay cycles
	g_delay_cycles = delay_cycles;

	// get original cdRead() pointer
	cdRead = (void *)FNADDR(_lw(patch_addr));

	// overwrite with a JAL to our delayed_cdRead function
	_sw(JAL((u32)delayed_cdRead), patch_addr);
}

void apply_game_patches(void)
{
	patchlist_t *p = (patchlist_t *)&patch_list[0];

	// if there are patches matching game name/mode then fill the patch table
	while (p->game) {
		if ((!strcmp(GameID, p->game)) && ((p->mode == ALL_MODE) || (GameMode == p->mode))) {

			if (p->patch.addr == 0xdeadbee0)
				NIS_generic_patches(); 	// Nippon Ichi Software games generic patch
			else if (p->patch.addr == 0xdeadbee1)
				AC9B_generic_patches(); // Armored Core 9 Breaker USB generic patch
			else if (p->patch.addr == 0xdeadbee2)
				KH2_generic_patches(p->patch.check, p->patch.val); // KH2 generic patch

			// non-generic patches
			else if (_lw(p->patch.addr) == p->patch.check)
				_sw(p->patch.val, p->patch.addr);
		}
		p++;
	}
}
