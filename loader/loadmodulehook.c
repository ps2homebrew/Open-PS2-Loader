/*
  Copyright 2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "loader.h"

static u32 loadModuleBuffer_pattern0[] = {
	0x27bdff70,	// addiu sp, sp, $ff70
	0xffb60070,	// sd 	 s6, $0070(sp)
	0xffb30040,	// sd 	 s3, $0040(sp)
	0x00e0b02d,	// daddu s6, a3, zero
	0xffb10020,	// sd 	 s1, $0020(sp)
	0x0080982d,	// daddu s3, a0, zero
	0xffb00010,	// sd 	 s0, $0010(sp)
	0x00a0882d,	// daddu s1, a1, zero
	0xffbf0080,	// sd 	 ra, $0080(sp)
	0x00c0802d,	// daddu s0, a2, zero
	0xffb50060,	// sd 	 s5, $0060(sp)
	0xffb40050,	// sd 	 s4, $0050(sp)
	0x0c000000,	// jal	 SifLoadFileInit
	0xffb20030,	// sd 	 s2, $0030(sp)
	0x04400000,	// bltz	 v0, xf
	0x3c02ffff	// lui	 v0, $ffff
};
static u32 loadModuleBuffer_pattern0_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xfc000000,
	0xffffffff,
	0xffff0000,
	0xffffffff
};

static u32 unloadModule_pattern0[] = {
	0x27bdffc0, 	// addiu sp, sp, $ffc0
	0xffb10020, 	// sd	 s1, $0020(sp)
	0xffbf0030, 	// sd	 ra, $0030(sp)
	0x0080882d, 	// daddu s1, a0, zero
	0x0c000000, 	// jal	 SifLoadFileInit
	0xffb00010, 	// sd	 s0, $0010(sp)
	0x04400018, 	// bltz	 v0, xf
	0x3c02ffff, 	// lui	 v0, $ffff
	0x0c000000, 	// jal	 unknown
	0x00000000, 	// nop
	0x10400004, 	// beq	 v0, zero, xf
	0x3c100000, 	// lui	 s0, $XXXX
	0x3c02fffe, 	// lui	 v0, $fffe
	0x10000011, 	// beq	 zero, zero, xf
	0x3442fffc, 	// ori	 v0, v0, $fffc
	0x3c040000 	// lui	 a0, $XXXX
};
static u32 unloadModule_pattern0_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xfc000000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xfc000000,
	0xffffffff,
	0xffffffff,
	0xffff0000,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffff0000
};

static u32 unloadModule_patchcode[] = {
	0x03e00008,	// jr	 ra
	0x24020000	// addiu v0, zero, $0
};

// modules list to fake loading
char *usb_modulefake_list[] = {
	"USB_driver",
	"cdvd_st_driver",
	NULL
};
char *eth_modulefake_list[] = {
	"dev9",
	"INET_SMAP_driver",
	"cdvd_st_driver",
	NULL
};
char *hdd_modulefake_list[] = {
	"atad_driver",
	"dev9",
	"cdvd_st_driver",
	NULL
};

void *modulefake_list[3] = {
	usb_modulefake_list,
	eth_modulefake_list,
	hdd_modulefake_list
};

#define	JAL(addr)	(0x0c000000 | ((addr & 0x03ffffff) >> 2))

// _sceSifLoadModuleBuffer prototype
int (*_SifLoadModuleBuffer)(void *ptr, int arg_len, const char *args, int *modres);

// hook function for _sceSifLoadModuleBuffer
static int Hook_SifLoadModuleBuffer(void *ptr, int arg_len, const char *args, int *modres)
{
	int i = 0;
	u8 modname_buf[256];

	// read IOP mem
	smem_read((void *)(ptr + 0x8e), modname_buf, 255);
	modname_buf[255] = 0; // just as security
	FlushCache(0);

	char **modlist = (char **)modulefake_list[GameMode];

	// check if module is in the list
	while (modlist[i]) {
		// fake module load if needed
		if (!strcmp(modname_buf, modlist[i])) {
			if (modres)
				*modres = 0;
			return 0;
		}
		i++;
	}

	// normal module load
	return _SifLoadModuleBuffer(ptr, arg_len, args, modres);
}

// patch function for LoadModuleBuffer
void loadModuleBuffer_patch(void)
{
	u32 *ptr = (u32 *)0x00100000;
	u32 pattern[1];
	u32 mask[1];

	// search for _sceSifLoadModuleBuffer function
	ptr = find_pattern_with_mask(ptr, 0x01f00000 - (u32)ptr, loadModuleBuffer_pattern0, loadModuleBuffer_pattern0_mask, sizeof(loadModuleBuffer_pattern0));
	if (ptr) {
		GS_BGCOLOUR = 0x004040; // olive while _sceSifLoadModuleBuffer calls search

		// get original SifLoadModuleBuffer pointer
		_SifLoadModuleBuffer = (void *)ptr;

		pattern[0] = JAL((u32)_SifLoadModuleBuffer);
		mask[0] = 0xffffffff;

		// search & hook _sceSifLoadModuleBuffer function calls
		ptr = (u32 *)0x00100000;
		while (ptr) {
			ptr = find_pattern_with_mask(ptr, 0x01f00000 - (u32)ptr, pattern, mask, sizeof(pattern));
			if (ptr)
				*ptr++ = JAL((u32)Hook_SifLoadModuleBuffer); // function hooking
		}

		GS_BGCOLOUR = 0x000000; // black, done
	}
}

// patch function for Module unloading
void unloadModule_patch(void)
{
	u32 *ptr = (u32 *)0x00100000;

	GS_BGCOLOUR = 0x404000; //

	// search for _sceSifLoadModuleBuffer function
	ptr = find_pattern_with_mask(ptr, 0x01f00000 - (u32)ptr, loadModuleBuffer_pattern0, loadModuleBuffer_pattern0_mask, sizeof(loadModuleBuffer_pattern0));
	if (ptr) {
		ptr += (sizeof(loadModuleBuffer_pattern0) >> 2);

		// search for _sceSifStopModule function (same pattern, but comes after)
		ptr = find_pattern_with_mask(ptr, 0x01f00000 - (u32)ptr, loadModuleBuffer_pattern0, loadModuleBuffer_pattern0_mask, sizeof(loadModuleBuffer_pattern0));
		if (ptr) {
			memcpy((void *)ptr, (void *)unloadModule_patchcode, sizeof(unloadModule_patchcode));

			// search for _sceSifUnloadModule function
			ptr = (u32 *)0x00100000;
			ptr = find_pattern_with_mask(ptr, 0x01f00000 - (u32)ptr, unloadModule_pattern0, unloadModule_pattern0_mask, sizeof(unloadModule_pattern0));
			if (ptr) {
				memcpy((void *)ptr, (void *)unloadModule_patchcode, sizeof(unloadModule_patchcode));
				GS_BGCOLOUR = 0x000000;
			}
		}
	}
}

