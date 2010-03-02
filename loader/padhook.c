/*
  padhook.c Open PS2 Loader In Game Reset
 
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Reset SPU function taken from PS2SDK freesd.
  Copyright (c) 2004 TyRaNiD <tiraniddo@hotmail.com>
  Copyright (c) 2004,2007 Lukasz Bruun <mail@lukasz.dk>

  PadOpen Hooking function inspired from ps2rd.
  Hook scePadPortOpen/scePad2CreateSocket instead of scePadRead/scePad2Read
  Copyright (C) 2009 jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright (C) 2009 misfire <misfire@xploderfreax.de>
*/

#include "loader.h"
#include "padhook.h"

/* scePadPortOpen & scePad2CreateSocket prototypes */
static int (*scePadPortOpen)( int port, int slot, void* addr );
static int (*scePad2CreateSocket)( pad2socketparam_t *SocketParam, void *addr );

/* Monitored pad data */
static paddata_t Pad_Data;

/* Monitored power button data */
static powerbuttondata_t Power_Button;

/* IGR Interrupt handler & Thread ID */
static int IGR_Intc_ID   = -1;
static int IGR_Thread_ID = -1;

/* IGR thread stack & stack size */
#define IGR_STACK_SIZE (16 * 512)
static u8 IGR_Stack[IGR_STACK_SIZE] __attribute__ ((aligned(16)));

/* Extern symbol */
extern void *_gp;

// Reset SPU sound processor
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

// Load home ELF
static void t_loadElf(void)
{
	int ret;
	char *argv[2];
	t_ExecData elf;

	if(!DisableDebug)
		GS_BGCOLOUR = 0x80FF00; // Blue Green

	// Init RPC & CMD
	SifInitRpc(0);

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

// Poweroff PlayStation 2
static void PowerOff_PS2(void)
{
	// Shutdown Dev9 hardware
	Shutdown_Dev9();

	DIntr();
	ee_kmode_enter();

	// PowerOff PS2
	*CDVD_R_SDIN = 0x00;
	*CDVD_R_SCMD = 0x0F;

	ee_kmode_exit();
	EIntr();
}

// In Game Reset Thread
static void IGR_Thread(void *arg)
{
	u32 Cop0_Index, Cop0_Perf;

	// Place our IGR thread in WAIT state
	// It will be woken up by our IGR interrupt handler
	SleepThread();
	
	// If Pad Combo is Start + Select then Return to Home
	if(Pad_Data.combo_type == IGR_COMBO_START_SELECT)
	{
		if(!DisableDebug)
			GS_BGCOLOUR = 0xFFFFFF; // White

		// Re-Init RPC & CMD
		SifExitRpc();
		SifInitRpc(0);

		if(!DisableDebug)
			GS_BGCOLOUR = 0x800000; // Dark Blue

		// Remove kernel hook
		Remove_Kernel_Hooks();

		if(!DisableDebug)
			GS_BGCOLOUR = 0x008000; // Dark Green

		// Reset Data Decompression Vector Unit 0 & 1
		ResetEE(0x04);

		if(!DisableDebug)
			GS_BGCOLOUR = 0x800080; // Purple

		// Reset SPU Sound processor
		ResetSPU();

		if(!DisableDebug)
			GS_BGCOLOUR = 0x0000FF; // Red

		// Check Translation Look-Aside Buffer
		// Some game (GT4, GTA) modify memory map
		// A re-init is needed to properly access memory
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
		// Some game (GT4) start performance counter
		// When counter overflow, an exception occur, so stop them
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

		if(!DisableDebug)
			GS_BGCOLOUR = 0x00FFFF; // Yellow

		// Exit services
		fioExit();
		LoadFileExit();
		SifExitIopHeap();
		SifExitRpc();

		FlushCache(0);
		FlushCache(2);

		// Execute home loader
		if ( ExitMode != OSDS_MODE)
			ExecPS2(t_loadElf, &_gp, 0, NULL);

		// Return to PS2 Browser
		Go_Browser();
	}

	// If combo is R3 + L3 or Reset failed, Poweroff PS2
	PowerOff_PS2();
}

// IGR VBLANK_END interrupt handler install to monitor combo trick in pad data aera
static int IGR_Intc_Handler(int cause)
{
	int i;

	// First check pad state
	if ( ( (Pad_Data.libpad == IGR_LIBPAD_V1) && (Pad_Data.pad_buf[Pad_Data.pos_state] == IGR_PAD_STABLE_V1) ) ||
			 ( (Pad_Data.libpad == IGR_LIBPAD_V2) && (Pad_Data.pad_buf[Pad_Data.pos_state] == IGR_PAD_STABLE_V2) ) )
	{
		// Combo R1 + L1 + R2 + L2
		if ( Pad_Data.pad_buf[Pad_Data.pos_combo1] == IGR_COMBO_R1_L1_R2_L2 )
		{
			// Combo Start + Select OR R3 + L3
			if ( ( Pad_Data.pad_buf[Pad_Data.pos_combo2] == IGR_COMBO_START_SELECT ) || // Start + Select combo, so reset
				   ( Pad_Data.pad_buf[Pad_Data.pos_combo2] == IGR_COMBO_R3_L3 ) )         // R3 + L3 combo, so poweroff
				Pad_Data.combo_type = Pad_Data.pad_buf[Pad_Data.pos_combo2];
		}
	}

	ee_kmode_enter();

	// Check power button press
	if ( (*CDVD_R_NDIN & 0x20) && (*CDVD_R_POFF & 0x04) )
	{
		// Increment button press counter
		Power_Button.press++;

		// Cancel poweroff to catch the second button press
		*CDVD_R_SDIN = 0x00;
		*CDVD_R_SCMD = 0x1B;
	}

	// Start VBlank counter when power button is pressed
	if( Power_Button.press )
	{
		// Check number of power button press after 1 sec
		if( Power_Button.vb_count++ >= 50 )
		{
			if( Power_Button.press == 1 )
				Pad_Data.combo_type = IGR_COMBO_R3_L3; // power button press 1 time, so poweroff
			else
				Pad_Data.combo_type = IGR_COMBO_START_SELECT; // power button press 2 time, so reset
		}
	}

	ee_kmode_exit();

	// If power button or combo is press
	// Suspend all threads other then our IGR thread
	// Disable all interrupts other then VBLANK_END ( and SBUS, TIMER2, TIMER3 use by kernel )
	if ( Power_Button.vb_count == 1 || Pad_Data.combo_type != 0x00 )
	{
		// Suspend all threads
		for(i = 3; i < 256; i++)
		{
			if(i != IGR_Thread_ID )
				iSuspendThread( i );
		}

		// Disable INTC
		iDisableIntc(kINTC_GS          );
		iDisableIntc(kINTC_VBLANK_START); 
		iDisableIntc(kINTC_VIF0        );
		iDisableIntc(kINTC_VIF1        );
		iDisableIntc(kINTC_VU0         );
		iDisableIntc(kINTC_VU1         );
		iDisableIntc(kINTC_IPU         );
		iDisableIntc(kINTC_TIMER0      );
		iDisableIntc(kINTC_TIMER1      );
	}

	// If a combo is set
	// Disable VBLANK_END interrupts
	// WakeUp our IGR thread
	if ( Pad_Data.combo_type != 0x00 )
	{
		// Disable VBLANK_END Interrupts
		iDisableIntc(kINTC_VBLANK_END);

		// WakeUp IGR thread
		iWakeupThread(IGR_Thread_ID);
	}

	// Exit handler
	__asm__ __volatile__(
		" sync.l;"
		" ei;"
	);

	return 0;
}

// Install IGR thread, and Pad interrupt handler
static void Install_IGR(void *addr, int libpad)
{
	ee_thread_t thread_param;

	// Reset power button data
	Power_Button.press    = 0;
	Power_Button.vb_count = 0;
	
	// Set pad library version, and buffer
	Pad_Data.libpad     = libpad;
	Pad_Data.pad_buf    = addr;
	Pad_Data.combo_type = 0x00;

	// Set positions of pad data and pad state in buffer
	if(libpad == IGR_LIBPAD_V1)
	{
		Pad_Data.pos_combo1 = 3;
		Pad_Data.pos_combo2 = 2;
		Pad_Data.pos_state  = 112;
	}
	else
	{
		Pad_Data.pos_combo1 = 29;
		Pad_Data.pos_combo2 = 28;
		Pad_Data.pos_state  = 4;
	}

	// Create and start IGR thread
	thread_param.gp_reg           = &_gp;
	thread_param.func             = IGR_Thread;
	thread_param.stack            = (void*)IGR_Stack;
	thread_param.stack_size       = IGR_STACK_SIZE;
	thread_param.initial_priority = -1;
	thread_param.current_priority = 99;
	IGR_Thread_ID                 = CreateThread(&thread_param);

	StartThread(IGR_Thread_ID, NULL);

	// Create IGR interrupt handler
	DisableIntc(kINTC_VBLANK_END);
	IGR_Intc_ID = AddIntcHandler(kINTC_VBLANK_END, IGR_Intc_Handler, 0);
	EnableIntc(kINTC_VBLANK_END);
}

// Hook function for libpad scePadPortOpen
static int Hook_scePadPortOpen( int port, int slot, void* addr )
{
	int ret;

	ret = scePadPortOpen(port, slot, addr);

	// Install IGR with libpad1 parameters
	if(port == 0 && slot == 0)
		Install_IGR(addr, IGR_LIBPAD_V1);

	return ret;
}

// Hook function for libpad2 scePad2CreateSocket
static int Hook_scePad2CreateSocket( pad2socketparam_t *SocketParam, void *addr )
{
	int ret;

	ret = scePad2CreateSocket(SocketParam, addr);

	// Install IGR with libpad2 parameters
	if(SocketParam->port == 0 && SocketParam->slot == 0)
		Install_IGR(addr, IGR_LIBPAD_V2);

	return ret;
}

/*
 * This function patch the padOpen calls
 * scePadPortOpen or scePad2CreateSocket
 */
int Install_PadOpen_Hook(u32 mem_start, u32 mem_end)
{
	int i, found;
	u8 *ptr, *ptr2;
	u32 inst, fncall;
	u32 mem_size, mem_size2;
	u32 pattern[1], mask[1];

	pattern_t padopen_patterns[NB_PADOPEN_PATTERN] = {
		{ padPortOpenpattern0     , padPortOpenpattern0_mask     , sizeof(padPortOpenpattern0)     , 1 },
		{ pad2CreateSocketpattern0, pad2CreateSocketpattern0_mask, sizeof(pad2CreateSocketpattern0), 2 },
		{ padPortOpenpattern1     , padPortOpenpattern1_mask     , sizeof(padPortOpenpattern1)     , 1 },
		{ pad2CreateSocketpattern1, pad2CreateSocketpattern1_mask, sizeof(pad2CreateSocketpattern1), 2 },
		{ padPortOpenpattern2     , padPortOpenpattern2_mask     , sizeof(padPortOpenpattern2)     , 1 }
	};

	found = 0;

	/* Loop for each libpad version */
	for(i = 0; i < NB_PADOPEN_PATTERN; i++)
	{
		ptr = (u8 *)mem_start;
		while (ptr)
		{
			if(!DisableDebug)
				GS_BGCOLOUR = 0x800080; /* Purple while PadOpen pattern search */

			mem_size = mem_end - (u32)ptr;

			/* First try to locate the orginal libpad's PadOpen function */
			ptr = find_pattern_with_mask(ptr, mem_size, padopen_patterns[i].pattern, padopen_patterns[i].mask, padopen_patterns[i].size);
			if (ptr)
			{				
				found = 1;

				if(!DisableDebug)
				 	GS_BGCOLOUR = 0x008000; /* Green while PadOpen patches */

				/* Save original PadOpen ptr */
				if (padopen_patterns[i].version == 1)
					scePadPortOpen = (void *)ptr;
				else
					scePad2CreateSocket = (void *)ptr;

				/* Retrieve PadOpen call Instruction code */
				inst = 0x0c000000;
				inst |= 0x03ffffff & ((u32)ptr >> 2);

				/* Make pattern with function call code saved above */
				pattern[0] = inst;
				mask[0] = 0xffffffff;

				/* Get Hook_PadOpen call Instruction code */
				inst = 0x0c000000;
				if (padopen_patterns[i].version == 1)
					inst |= 0x03ffffff & ((u32)Hook_scePadPortOpen >> 2);
				else
					inst |= 0x03ffffff & ((u32)Hook_scePad2CreateSocket >> 2);

				/* Search & patch for calls to PadOpen */
				ptr2 = (u8 *)0x00100000;
				while (ptr2)
				{
					mem_size2 = 0x01f00000 - (u32)ptr2;

					ptr2 = find_pattern_with_mask(ptr2, mem_size2, (u8 *)pattern, (u8 *)mask, sizeof(pattern));
					if (ptr2)
					{
						fncall = (u32)ptr2;
						_sw(inst, fncall); /* overwrite the original PadOpen function call with our function call */

						ptr2 += 8;
					}
				}
				ptr += 8;
			}
		}

		/* Assume that only one libpad version is use per game... Not sure ... */
		if(found == 1)
			break;
	}

	if(!DisableDebug)
		GS_BGCOLOUR = 0x000000; /* Black, done */

	return found;
}
