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

/* Stack pointer variable used to check $sp value */
static u32 Stack_Pointer = 0;


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

// Shutdown Dev9 hardware
static void Shutdown_Dev9()
{
	u16 dev9_hw_type;
	
	DIntr();
	ee_kmode_enter();

	// Get dev9 hardware type
	dev9_hw_type = *DEV9_R_146E & 0xf0;
	
	// Shutdown Pcmcia
	if ( dev9_hw_type == 0x20 )
	{
		*DEV9_R_146C = 0;
		*DEV9_R_1474 = 0;
	}
	// Shutdown Expansion Bay
	else if ( dev9_hw_type == 0x30 )
	{
		*DEV9_R_1466 = 1;
		*DEV9_R_1464 = 0;
		*DEV9_R_1460 = *DEV9_R_1464;
		*DEV9_R_146C = *DEV9_R_146C & ~4;
		*DEV9_R_1460 = *DEV9_R_146C & ~1;
	}

	//Wait a sec
	delay(5);
	
	ee_kmode_exit();
	EIntr();
}

// Return to PS2 Browser
static void Go_Browser(void)
{
	// Shutdown Dev9 hardware
	Shutdown_Dev9();
	
	// FlushCache before exiting
	FlushCache(0);
	FlushCache(2);
		
	// Exit to PS2Browser
	__asm__ __volatile__(
		"	li $3, 0x04;"
		"	syscall;"
		"	nop;"
	);
}

// Poweroff PlayStation 2
static void Power_Off(void)
{
	// Shutdown Dev9 hardware
	Shutdown_Dev9();

	DIntr();
	ee_kmode_enter();

	// PowerOff PS2
	*CDVD_R_SDIN = 0;
	*CDVD_R_SCMD = 0xF;

	ee_kmode_exit();
	EIntr();
}

// Load home ELF
static void t_loadElf(void)
{
	int ret;
	char *argv[2];
	t_ExecData elf;

	if(!DisableDebug)
		GS_BGCOLOUR = 0x008000; // Dark Green

	// Exit RPC & CMD
	SifExitRpc();

	if(!DisableDebug)
		GS_BGCOLOUR = 0x000080; // Dark Red

	// Reset IO Processor
	while (!Reset_Iop("rom0:UDNL rom0:EELOADCNF", 0)) {;}
	while (!Sync_Iop()){;}

	if(!DisableDebug)
		GS_BGCOLOUR = 0xFF80FF; // Pink

	// Init RPC & CMD
	SifInitRpc(0);

	// Apply Sbv patches
	Sbv_Patch();
	
	if(!DisableDebug)
		GS_BGCOLOUR = 0xFF8000; // Blue sky

	// Load basic modules
	LoadModule("rom0:SIO2MAN", 0, NULL);
	LoadModule("rom0:MCMAN", 0, NULL);

	// Load BOOT.ELF
	if ( ExitMode == BOOT_MODE)
		argv[0] = "mc0:/BOOT/BOOT.ELF";
	else if ( ExitMode == APPS_MODE)
		argv[0] = "mc0:/APPS/BOOT.ELF";

	argv[1] = NULL;

	ret = LoadElf(argv[0], &elf);
	
	if (!ret && elf.epc) {

		if(!DisableDebug)
			GS_BGCOLOUR = 0x00FFFF; // Yellow

		// Exit services
		fioExit();
		LoadFileExit();
		SifExitIopHeap();
		SifExitRpc();

		FlushCache(0);
		FlushCache(2);

		if(!DisableDebug)
			GS_BGCOLOUR = 0x0080FF; // Orange

		// Execute BOOT.ELF
		ExecPS2((void*)elf.epc, (void*)elf.gp, 1, argv);
	}

	if(!DisableDebug)
	{
		GS_BGCOLOUR = 0x0000FF; // Red
		delay(5);
	}

	// Return to PS2 Browser
	Go_Browser();
}

// Go home function is call when combo trick is press
static void Go_Home(void)
{
	u32 Cop0_Index, Cop0_Perf;

	if(!DisableDebug)
		GS_BGCOLOUR = 0xFFFFFF; // White

	// Remove kernel hook
	Remove_Kernel_Hooks();

	if(!DisableDebug)
		GS_BGCOLOUR = 0x800000; // Dark Blue

	// Reset EE Coprocessors & Controlers
	ResetEE(0x07);

	if(!DisableDebug)
		GS_BGCOLOUR = 0x800080; // Purple

	// Reset SPU Sound processor
	ResetSPU();
	
	// Check Translation Look-Aside Buffer
	Cop0_Index = GetCop0(0);
	
	// Init TLB
	if(Cop0_Index != 0x26)
	{
		__asm__ __volatile__(
			"	li $3, 0x82;"
			"	syscall;"
			"	nop;"
		);
	}

	// Check Performance Counter
	Cop0_Perf = GetCop0(25);

	// Stop Performance Counter
	if(Cop0_Perf & 0x80000000)
	{
		__asm__ __volatile__(
			" mfc0 $3, $25;"
			" lui	 $2, 0x8000;"
			" or	 $3, $3, $2;"
			" xor	 $3, $3, $2;"
			" mtc0 $3, $25;"
			" sync.p;"
		);
	}

	// Check Stack Pointer
	__asm__ __volatile__(
		"	sd $29, Stack_Pointer;"
	);
		
	// Some game use a stack pointer in scratchpad mem, so fix it to 0x2000000
	if(Stack_Pointer >= 0x70000000)
	{
		__asm__ __volatile__(
			"	la $29, 0x2000000;"
		);
	}

		// Execute home loader
	if ( ExitMode != OSDS_MODE)
		ExecPS2(t_loadElf, NULL, 0, NULL);

	// Return to PS2 Browser
	Go_Browser();
}

// Hook function for libpad scePadRead
static int Hook_scePadRead(int port, int slot, u8* data)
{
	int ret;

	ret = scePadRead(port, slot, data);

	// Combo R1 + L1 + R2 + L2
	if ( data[3] == 0xf0 )
	{
		// + Start + Select
		if ( data[2] == 0xf6 )
		{
			// Return to home
			Go_Home();
		}
		// + R3 + L3
		else if ( data[2] == 0xf9 )
		{
			// Turn off PS2
			Power_Off();
		}
	}

	return ret;
}

// Hook function for libpad2 scePad2Read
static int Hook_scePad2Read(int socket, u8* data)
{
	int ret;

	ret = scePad2Read(socket, data);

	// Combo R1 + L1 + R2 + L2
	if ( data[1] == 0xf0 )
	{
		// + Start + Select
		if ( data[0] == 0xf6 )
		{
			// Return to home
			Go_Home();
		}
		// + R3 + L3
		else if ( data[0] == 0xf9 )
		{
			// Turn off PS2
			Power_Off();
		}
	}

	return ret;
}

/*
 * This function patch the padRead calls
 */
int Install_PadRead_Hook(u32 start, u32 memscope)
{
	int ret;
	u8 *ptr;
	u32 inst, fncall;
	u32 pattern[1], mask[1];
	int scePadRead_style = 1;
	
	ret = 0;

	if(!DisableDebug)
		GS_BGCOLOUR = 0x800080; /* Purple while padRead pattern search */

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
									if (!ptr)
									{
										if(!DisableDebug)
											GS_BGCOLOUR = 0x000000;
										return ret;
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

	if(!DisableDebug)
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
			ret = 1;

			fncall = (u32)ptr;
			_sw(inst, fncall); /* overwrite the original scePadRead function call with our function call */

			ptr += 8;
		}
	}

	if(!DisableDebug)
		GS_BGCOLOUR = 0x000000; /* Black, done */

	return ret;
}
