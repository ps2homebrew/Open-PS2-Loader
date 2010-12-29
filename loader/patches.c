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

static patchlist_t patch_list[38] = {
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
	{ "SLUS_213.17", ALL_MODE, { 0xbabecafe, 0x00149210, 0x00000000 }}, // SFA anthology US
	{ "SLES_540.85", ALL_MODE, { 0xbabecafe, 0x00148db0, 0x00000000 }}, // SFA anthology EUR
	{ "SLPM_659.98", ALL_MODE, { 0xbabecafe, 0x00146fd0, 0x00000000 }}, // Vampire: Darkstakers collection JP
	{ "SLUS_212.00", USB_MODE, { 0xdeadbee1, 0x00000000, 0x00000000 }}, // Armored Core Nine Breaker NTSC U - skip failing case on binding a RPC server
	{ "SLES_538.19", USB_MODE, { 0xdeadbee1, 0x00000000, 0x00000000 }}, // Armored Core Nine Breaker PAL - skip failing case on binding a RPC server
	{ "SLPS_254.08", USB_MODE, { 0xdeadbee1, 0x00000000, 0x00000000 }}, // Armored Core Nine Breaker NTSC J - skip failing case on binding a RPC server
	{ "SLUS_210.05", ALL_MODE, { 0xdeadbee2, 0x00100000, 0x001ac514 }}, // Kingdom Hearts 2 US - [Gummi mission freezing fix (check addr is where to patch,
	{ "SLES_541.14", ALL_MODE, { 0xdeadbee2, 0x00100000, 0x001ac60c }}, // Kingdom Hearts 2 UK - val is the amount of delay cycles)]
	{ "SLES_542.32", ALL_MODE, { 0xdeadbee2, 0x00100000, 0x001ac60c }}, // Kingdom Hearts 2 FR
	{ "SLES_542.33", ALL_MODE, { 0xdeadbee2, 0x00100000, 0x001ac60c }}, // Kingdom Hearts 2 DE
	{ "SLES_542.34", ALL_MODE, { 0xdeadbee2, 0x00100000, 0x001ac60c }}, // Kingdom Hearts 2 IT
	{ "SLES_542.35", ALL_MODE, { 0xdeadbee2, 0x00100000, 0x001ac60c }}, // Kingdom Hearts 2 ES
	{ "SLPM_662.33", ALL_MODE, { 0xdeadbee2, 0x00100000, 0x001ac44c }}, // Kingdom Hearts 2 JPN
	{ "SLUS_212.87", ETH_MODE, { 0xdeadbee2, 0x000c0000, 0x006cd15c }}, // Prince of Persia: The Two Thrones NTSC U - slow down cdvd reads
	{ "SLUS_212.87", HDD_MODE, { 0xdeadbee2, 0x00040000, 0x006cd15c }}, // Prince of Persia: The Two Thrones NTSC U - slow down cdvd reads
	{ "SLES_537.77", ETH_MODE, { 0xdeadbee2, 0x000c0000, 0x006cd6dc }}, // Prince of Persia: The Two Thrones PAL - slow down cdvd reads
	{ "SLES_537.77", HDD_MODE, { 0xdeadbee2, 0x00040000, 0x006cd6dc }}, // Prince of Persia: The Two Thrones PAL - slow down cdvd reads
	{ "SLUS_214.32", ALL_MODE, { 0xdeadbee2, 0x00080000, 0x002baf34 }}, // NRA Gun Club NTSC U
	{ "SLPM_654.05", HDD_MODE, { 0xdeadbee2, 0x00200000, 0x00249b84 }}, // Super Dimensional Fortress Macross JPN
	/*
	{ "SCES_525.82", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // EveryBody's Golf PAL
	{ "SCUS_974.01", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Hot Shots Golf FORE! NTSC U
	{ "SCUS_975.15", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Hot Shots Golf FORE! (GH) NTSC U
	{ "SCUS_976.10", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Hot Shots Tennis NTSC U
	{ "SLUS_209.51", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Viewtiful Joe NTSC U
	{ "SLES_526.78", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Viewtiful Joe PAL
	{ "SLPM_656.99", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Viewtiful Joe NTSC J
	{ "SCUS_973.30", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Jak 3 US
	{ "SCUS_975.16", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Jak 3 US (GH)
	{ "SCES_524.60", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Jak 3 PAL
	{ "SCKA_200.40", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Jak 3 NTSC K
	{ "SCUS_974.29", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Jak X NTSC U
	{ "SCES_532.86", ALL_MODE, { 0xdeadbee3, 0x00000000, 0x00000000 }}, // Jak X PAL
	{ "SLES_820.28", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET PAL Disc1
	{ "SLES_820.29", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET PAL Disc2
	{ "SLUS_204.88", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET NTSC-U Disc1
	{ "SLUS_208.91", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET NTSC-U Disc2
	{ "SCPS_550.19", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET NTSC-J
	{ "SLPM_664.78", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET NTSC-J (Ultimate Hits) Disc1
	{ "SLPM_664.79", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET NTSC-J (Ultimate Hits) Disc2
	{ "SLPM_652.09", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET NTSC-J (Limited Edition)
	{ "SLPM_654.38", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET NTSC-J (Director's Cut) Disc1
	{ "SLPM_654.39", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET NTSC-J (Director's Cut) Disc2
	{ "SCAJ_200.70", ALL_MODE, { 0xbabecafe, 0x00000000, 0x00000000 }}, // SOTET NTSC-? (Director's Cut)
	*/
	{ "SLUS_202.30", ALL_MODE, { 0x00132d14, 0x10000018, 0x0c046744 }}, // Max Payne NTSC U - skip IOP reset before to exec demo elfs
	{ "SLES_503.25", ALL_MODE, { 0x00132ce4, 0x10000018, 0x0c046744 }}, // Max Payne PAL - skip IOP reset before to exec demo elfs
	{ "SLUS_204.40", ALL_MODE, { 0x0021bb00, 0x03e00008, 0x27bdff90 }}, // Kya: Dark Lineage NTSC U - disable game debug prints
	{ "SLES_514.73", ALL_MODE, { 0x0021bd10, 0x03e00008, 0x27bdff90 }}, // Kya: Dark Lineage PAL - disable game debug prints
	{ "SLUS_204.96", ALL_MODE, { 0x00104900, 0x03e00008, 0x27bdff90 }}, // V-Rally 3 NTSC U - disable game debug prints
	{ "SLES_507.25", ALL_MODE, { 0x00104518, 0x03e00008, 0x27bdff70 }}, // V-Rally 3 PAL - disable game debug prints
	{ "SLUS_201.99", ALL_MODE, { 0x0012a6d0, 0x24020001, 0x0c045e0a }}, // Shaun Palmer's Pro Snowboarder NTSC U
	{ "SLUS_201.99", ALL_MODE, { 0x0013c55c, 0x10000012, 0x04400012 }}, // Shaun Palmer's Pro Snowboarder NTSC U
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

static void generic_delayed_cdRead_patches(u32 patch_addr, u32 delay_cycles)
{
	// set configureable delay cycles
	g_delay_cycles = delay_cycles;

	// get original cdRead() pointer
	cdRead = (void *)FNADDR(_lw(patch_addr));

	// overwrite with a JAL to our delayed_cdRead function
	_sw(JAL((u32)delayed_cdRead), patch_addr);
}


static int (*capcom_lmb)(void *modpack_addr, int mod_index, int mod_argc, char **mod_argv);

static void apply_capcom_protection_patch(void *modpack_addr, int mod_index, int mod_argc, char **mod_argv)
{
	u32 iop_addr = _lw((u32)modpack_addr + (mod_index << 3) + 8);
	u32 opcode = 0x10000025;
	smem_write((void *)(iop_addr+0x270), (void *)&opcode, sizeof(opcode));
	FlushCache(0);
	FlushCache(2);

	capcom_lmb(modpack_addr, mod_index, mod_argc, mod_argv);
}

static void generic_capcom_protection_patches(u32 patch_addr)
{
	capcom_lmb = (void *)FNADDR(_lw(patch_addr));
	_sw(JAL((u32)apply_capcom_protection_patch), patch_addr);
}


void apply_patches(void)
{
	patchlist_t *p = (patchlist_t *)&patch_list[0];

	// if there are patches matching game name/mode then fill the patch table
	while (p->game) {
		if ((!_strcmp(GameID, p->game)) && ((p->mode == ALL_MODE) || (GameMode == p->mode))) {

			if (p->patch.addr == 0xdeadbee0)
				NIS_generic_patches(); 	// Nippon Ichi Software games generic patch
			else if (p->patch.addr == 0xdeadbee1)
				AC9B_generic_patches(); // Armored Core 9 Breaker USB generic patch
			else if (p->patch.addr == 0xdeadbee2)
				generic_delayed_cdRead_patches(p->patch.check, p->patch.val); // slow reads generic patch
			else if (p->patch.addr == 0xbabecafe)
				generic_capcom_protection_patches(p->patch.val); // Capcom anti cdvd emulator protection patch

			// non-generic patches
			else if (_lw(p->patch.addr) == p->patch.check)
				_sw(p->patch.val, p->patch.addr);
		}
		p++;
	}
}
