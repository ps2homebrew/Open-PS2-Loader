/*
  padhook.c Open PS2 Loader In Game Reset
 
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Reset SPU function taken from PS2SDK freesd.
  Copyright (c) 2004 TyRaNiD <tiraniddo@hotmail.com>
  Copyright (c) 2004,2007 Lukasz Bruun <mail@lukasz.dk>

  scePadRead Hooking function taken from ps2rd.
  Copyright (C) 2009 jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright (C) 2009 misfire <misfire@xploderfreax.de>
*/

#include "loader.h"
#include "padhook.h"


/* scePadRead prototypes */
static int (*scePadRead)(int port, int slot, u8* data);
static int (*scePad2Read)(int socket, u8* data);


// Reset SPU Sound processor
static void ResetSPU()
{
	u32 core;
	volatile u16 *statx;

	DIntr();
	ee_kmode_enter();

	// Stop SPU Dma Core 0
	*SD_DMA_CHCR(0) &= ~SD_DMA_START;
	*U16_REGISTER(0x1B0) = 0;

	// Stop SPU Dma Core 1
	*SD_DMA_CHCR(1) &= ~SD_DMA_START;
	*U16_REGISTER(0x1B0 +  1024) = 0;

	// Initialize SPU2
	*U32_REGISTER(0x1404)  = 0xBF900000;
	*U32_REGISTER(0x140C)  = 0xBF900800;
	*U32_REGISTER(0x10F0) |= 0x80000;
	*U32_REGISTER(0x1570) |= 8;
	*U32_REGISTER(0x1014)  = 0x200B31E1;
	*U32_REGISTER(0x1414)  = 0x200B31E1;

	*SD_C_SPDIF_OUT = 0;
	delay(1);
	*SD_C_SPDIF_OUT = 0x8000;
	delay(1);

	*U32_REGISTER(0x10F0) |= 0xB0000;

	for(core=0; core < 2; core++)
	{
		*U16_REGISTER(0x1B0)	= 0;
		*SD_CORE_ATTR(core)		= 0;
		delay(1);
		*SD_CORE_ATTR(core)		= SD_SPU2_ON;

		*SD_P_MVOLL(core)		= 0;
		*SD_P_MVOLR(core)		= 0;
		
		statx = U16_REGISTER(0x344 + (core * 1024));

		while(*statx & 0x7FF);
		
		*SD_A_KOFF_HI(core)		= 0xFFFF;
		*SD_A_KOFF_LO(core)		= 0xFFFF; // Should probably only be 0xFF
	}

	*SD_S_PMON_HI(1)	= 0;
	*SD_S_PMON_LO(1)	= 0;
	*SD_S_NON_HI(1)		= 0;
	*SD_S_NON_LO(1)		= 0;

	ee_kmode_exit();
	EIntr();
}

// Go home function is call when combo trick is press
static void Go_Home(void)
{
	int r, argc;
	t_ExecData elf;
	char *argv[2];

	GS_BGCOLOUR = 0xFFFFFF; // White

	// Remove kernel hook
	Remove_Kernel_Hooks();

	// Reset EE Processor help with most games, not with some other ...
	// TODO: Find the correct value
	ResetEE(0x7F);

	GS_BGCOLOUR = 0x800000; // Dark Blue

	// Exit Services
	SifExitRpc();
	SifExitIopHeap();
	LoadFileExit();

	GS_BGCOLOUR = 0x008000; // Dark Green

	// Reset IO Processor
	while (!Reset_Iop("rom0:UDNL rom0:EELOADCNF", 0));
	while (!Sync_Iop());

	GS_BGCOLOUR = 0x000080; // Dark Red

	// Init RPC
	SifInitRpc(0);

	// Init Services
	SifInitIopHeap();
	LoadFileInit();
	Sbv_Patch();
		
	GS_BGCOLOUR = 0xFF8000; // Blue sky

	// Load basic modules
	LoadModule("rom0:SIO2MAN", 0, NULL);
	LoadModule("rom0:MCMAN", 0, NULL);
	LoadModule("rom0:MCSERV", 0, NULL);

	GS_BGCOLOUR = 0x800080; // Purple

	// Reset SPU Sound processor
	ResetSPU();

	// Load BOOT.ELF
	r = LoadElf("mc0:/BOOT/BOOT.ELF", &elf);
	if (!r && elf.epc) {

		GS_BGCOLOUR = 0x00FFFF; // Yellow

		// Exit services
		fioExit();
		LoadFileExit();
		SifExitIopHeap();
		SifExitRpc();

		FlushCache(0);
		FlushCache(2);
		
		argc = 1;
		argv[0] = "mc0:/BOOT/BOOT.ELF";
		argv[1] = NULL;

		GS_BGCOLOUR = 0x0080FF; // Orange

		// Execute BOOT.ELF
		ExecPS2((void*)elf.epc, (void*)elf.gp, argc, argv);
	}

	GS_BGCOLOUR = 0x0000FF; // Red
	delay(5);

	// Exit to PS2Browser if LoadElf failed
	__asm__ __volatile__(
		"	li $3, 0x04;"
		"	syscall;"
		"	nop;"
	);
}

// Hook function for libpad scePadRead
static int Hook_scePadRead(int port, int slot, u8* data)
{
	int ret;

	ret = scePadRead(port, slot, data);

	// Combo R1 + R2 + L1 + L2 + Start + Select
	if ( (data[2] == 0xf6) && (data[3] == 0xf0) )
	{
		Go_Home();
	}

	return ret;
}

// Hook function for libpad2 scePad2Read
static int Hook_scePad2Read(int socket, u8* data)
{
	int ret;

	ret = scePad2Read(socket, data);

	// Combo R1 + R2 + L1 + L2 + Start + Select
	if ( (data[0] == 0xf6) && (data[1] == 0xf0) )
	{
		Go_Home();
	}

	return ret;
}

/*
 * This function patch the padRead calls
 */
int Install_PadRead_Hook(void)
{
	u8 *ptr;
	u32 memscope, inst, fncall;
	u32 pattern[1], mask[1];
	u32 start = 0x00100000;
	int scePadRead_style = 1;

	GS_BGCOLOUR = 0x800080; /* Purple while padRead pattern search */

	memscope = 0x01f00000 - start;

	/* First try to locate the orginal libpad's scePadRead function */
	ptr = find_pattern_with_mask((u8 *)start, memscope, (u8 *)padReadpattern0, (u8 *)padReadpattern0_1_mask, sizeof(padReadpattern0));
	if (!ptr) {
		ptr = find_pattern_with_mask((u8 *)start, memscope, (u8 *)padReadpattern1, (u8 *)padReadpattern0_1_mask, sizeof(padReadpattern1));
		if (!ptr) {
			ptr = find_pattern_with_mask((u8 *)start, memscope, (u8 *)padReadpattern2, (u8 *)padReadpattern2_mask, sizeof(padReadpattern2));
			if (!ptr) {
				ptr = find_pattern_with_mask((u8 *)start, memscope, (u8 *)padReadpattern3, (u8 *)padReadpattern3_mask, sizeof(padReadpattern3));
				if (!ptr) {
					ptr = find_pattern_with_mask((u8 *)start, memscope, (u8 *)pad2Readpattern0, (u8 *)pad2Readpattern0_mask, sizeof(pad2Readpattern0));
					if (!ptr) {
						ptr = find_pattern_with_mask((u8 *)start, memscope, (u8 *)padReadpattern4, (u8 *)padReadpattern4_mask, sizeof(padReadpattern4));
						if (!ptr) {
							ptr = find_pattern_with_mask((u8 *)start, memscope, (u8 *)padReadpattern5, (u8 *)padReadpattern5_mask, sizeof(padReadpattern5));
							if (!ptr) {
								ptr = find_pattern_with_mask((u8 *)start, memscope, (u8 *)padReadpattern6, (u8 *)padReadpattern6_mask, sizeof(padReadpattern6));
								if (!ptr) {
									ptr = find_pattern_with_mask((u8 *)start, memscope, (u8 *)padReadpattern7, (u8 *)padReadpattern7_mask, sizeof(padReadpattern7));
									if (!ptr) {
										GS_BGCOLOUR = 0x000000;
										return 0;
									}
								}
							}
						}
					}
					else /* If found scePad2Read pattern */
						scePadRead_style = 2;
				}
			}
		}
	}

 	GS_BGCOLOUR = 0x008000; /* Green while padRead patches */

	/* Save original scePadRead ptr */
	if (scePadRead_style == 2)
		scePad2Read = (void *)ptr;
	else
		scePadRead = (void *)ptr;

	/* Retrieve scePadRead call Instruction code */
	inst = 0x0c000000;
	inst |= 0x03ffffff & ((u32)ptr >> 2);

	/* Make pattern with function call code saved above */
	pattern[0] = inst;
	mask[0] = 0xffffffff;

	/* Get Hook_scePadRead call Instruction code */
	if (scePadRead_style == 2)
	{
		inst = 0x0c000000;
		inst |= 0x03ffffff & ((u32)Hook_scePad2Read >> 2);
	}
	else
	{
		inst = 0x0c000000;
		inst |= 0x03ffffff & ((u32)Hook_scePadRead >> 2);
	}

	/* Search & patch for calls to scePadRead */
	ptr = (u8 *)start;
	while (ptr)
	{
		memscope = 0x01f00000 - (u32)ptr;

		ptr = find_pattern_with_mask(ptr, memscope, (u8 *)pattern, (u8 *)mask, sizeof(pattern));
		if (ptr)
		{
			fncall = (u32)ptr;
			_sw(inst, fncall); /* overwrite the original scePadRead function call with our function call */

			ptr += 8;
		}
	}

	GS_BGCOLOUR = 0x000000; /* Black, done */

	return 1;
}
