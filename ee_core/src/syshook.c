/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#include "ee_core.h"
#include "asm.h"
#include "iopmgr.h"
#include "modmgr.h"
#include "util.h"
#include "spu.h"
#include "patches.h"
#include "padhook.h"
#include "syshook.h"

#include <syscallnr.h>
#include <ee_regs.h>
#include <ps2_reg_defs.h>

extern void *cddev_irx;
extern int size_cddev_irx;

#define MAX_ARGS 	15

int g_argc;
char *g_argv[1 + MAX_ARGS];
static char g_ElfPath[1024];

int set_reg_hook;
int set_reg_disabled;
int iop_reboot_count = 0;

int padOpen_hooked = 0;

/*----------------------------------------------------------------------------------------*/
/* This fonction is call when SifSetDma catch a reboot request.                           */
/*----------------------------------------------------------------------------------------*/
u32 New_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
	// Hook padOpen function to install In Game Reset
	if( !(g_compat_mask & COMPAT_MODE_6) && padOpen_hooked == 0 )
		padOpen_hooked = Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_HOOK);

	SifCmdResetData *reset_pkt = (SifCmdResetData*)sdd->src;

	// does IOP reset
	New_Reset_Iop(reset_pkt->arg, reset_pkt->flag);

	return 1;
}

// ------------------------------------------------------------------------
void t_loadElf(void)
{
	int i, r;
	t_ExecData elf;

	DPRINTF("t_loadElf()\n");

	ResetSPU();

	SifExitRpc();

	DPRINTF("t_loadElf: Resetting IOP...\n");

	set_reg_disabled = 0;
	New_Reset_Iop("rom0:UDNL rom0:EELOADCNF", 0);
	set_reg_disabled = 1;

	iop_reboot_count = 1;

	SifInitRpc(0);
	LoadFileInit();

	DPRINTF("t_loadElf: Loading cddev IOP module...\n");
	LoadIRXfromKernel(cddev_irx, size_cddev_irx, 0, NULL);

	strncpy(g_ElfPath, g_argv[0], 1024);
	g_ElfPath[1023] = 0;
	DPRINTF("t_loadElf: elf path = '%s'\n", g_ElfPath);

	// replacing cdrom in elf path by cddev
	if (_strstr(g_argv[0], "cdrom")) {
		u8 *ptr = (u8 *)g_argv[0];
		_strcpy(g_ElfPath, "cddev");
		_strcat(g_ElfPath, &ptr[5]);		
	}

	DPRINTF("t_loadElf: elf path = '%s'\n", g_ElfPath);

	if(!DisableDebug)
		GS_BGCOLOUR = 0x00ff00;

	DPRINTF("t_loadElf: cleaning user memory...");

	// wipe user memory
	for (i = 0x00100000; i < 0x02000000; i += 64) {
		__asm__ __volatile__ (
			"\tsq $0, 0(%0) \n"
			"\tsq $0, 16(%0) \n"
			"\tsq $0, 32(%0) \n"
			"\tsq $0, 48(%0) \n"
			:: "r" (i)
		);
	}
	DPRINTF(" done\n");

	DPRINTF("t_loadElf: loading elf...");
	r = LoadElf(g_ElfPath, &elf);

	if ((!r) && (elf.epc)) {
		DPRINTF(" done\n");

		DPRINTF("t_loadElf: exiting services...\n");
		// exit services
		fioExit();
		SifExitIopHeap();
		LoadFileExit();
		SifExitRpc();

		// replacing cddev in elf path by cdrom
		if (_strstr(g_ElfPath, "cddev"))
			memcpy(g_ElfPath, "cdrom", 5);

		DPRINTF("t_loadElf: real elf path = '%s'\n", g_ElfPath);

		DPRINTF("t_loadElf: trying to apply patches...\n");
		// applying needed patches
		apply_patches();

		FlushCache(0);
		FlushCache(2);

		DPRINTF("t_loadElf: executing...\n");
		ExecPS2((void*)elf.epc, (void*)elf.gp, g_argc, g_argv);
	}

	DPRINTF(" failed\n");

	if(!DisableDebug)
		GS_BGCOLOUR = 0xffffff; // white screen: error
	SleepThread();
}

/*----------------------------------------------------------------------------------------*/
/* Replace SifSetDma, SifSetReg, LoadExecPS2 syscalls in kernel. (Game Loader)            */
/* Replace CreateThread and ExecPS2 syscalls in kernel. (In Game Reset)                   */
/*----------------------------------------------------------------------------------------*/
void Install_Kernel_Hooks(void)
{
	Old_SifSetDma  = GetSyscallHandler(__NR_SifSetDma);
	SetSyscall(__NR_SifSetDma, &Hook_SifSetDma);

	Old_SifSetReg  = GetSyscallHandler(__NR_SifSetReg);
	SetSyscall(__NR_SifSetReg, &Hook_SifSetReg);

	Old_LoadExecPS2 = GetSyscallHandler(__NR_LoadExecPS2);
	SetSyscall(__NR_LoadExecPS2, &Hook_LoadExecPS2);

	// If IGR is enabled hook ExecPS2 & CreateThread syscalls
	if(!(g_compat_mask & COMPAT_MODE_6))
	{
		Old_CreateThread = GetSyscallHandler(__NR_CreateThread);
		SetSyscall(__NR_CreateThread, &Hook_CreateThread);

		Old_ExecPS2 = GetSyscallHandler(__NR_ExecPS2);
		SetSyscall(__NR_ExecPS2, &Hook_ExecPS2);
	}
}

/*----------------------------------------------------------------------------------------*/
/* Restore original SifSetDma, SifSetReg, LoadExecPS2 syscalls in kernel. (Game loader)   */
/* Restore original CreateThread and ExecPS2 syscalls in kernel. (In Game Reset)          */
/*----------------------------------------------------------------------------------------*/
void Remove_Kernel_Hooks(void)
{
	SetSyscall(__NR_SifSetDma, Old_SifSetDma);
	SetSyscall(__NR_SifSetReg, Old_SifSetReg);	
	SetSyscall(__NR_LoadExecPS2, Old_LoadExecPS2);

	// If IGR is enabled unhook ExecPS2 & CreateThread syscalls
	if(!(g_compat_mask & COMPAT_MODE_6))
	{
		SetSyscall(__NR_CreateThread, Old_CreateThread);
		SetSyscall(__NR_ExecPS2, Old_ExecPS2);
	}
}
