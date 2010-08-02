/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#include "loader.h"
#include "iopmgr.h"
#include <syscallnr.h>
#include <ee_regs.h>
#include <ps2_reg_defs.h>

extern void *cddev_irx;
extern int size_cddev_irx;

#define MAX_G_ARGS 15
static int g_argc;
static char *g_argv[1 + MAX_ARGS];
static char g_argbuf[580];
static char g_ElfPath[1024];
static void *g_patchInitializeUserMem_addr;
static u32 g_patchInitializeUserMem_val;

int set_reg_hook;
int set_reg_disabled;
int iop_reboot_count = 0;

int padOpen_hooked = 0;

static u32 systemRestartpattern[] = {
	0x00000000,		//	nop
	0x0c000000,		//	jal	restartEE()
	0x00000000,		//	nop
	0x8fa30010,		//	lw	v1, $0010(sp)
	0x0240302d,		//	daddu	a2, s2, zero
	0x8fa50014,		//	lw	a1, $0014(sp)
	0x8c67000c,		//	lw	a3, $000c(v1)
	0x18e00009,		//	blez	a3, 2f
	0x0000202d,		//	daddu	a0, zero, zero
	0x00000000,		//	nop
	0x8ca30000,		//	lw	v1, $0000(a1)
	0x24840004,		//	addiu	a0, a0, $0004
	0x24a50004,		//	addiu	a1, a1, $0004
	0x0087102a,		//	slt	v0, a0, a3
	0xacc30000		//	sw	v1, $0000(a2)
};
static u32 systemRestartpattern_mask[] = {
	0xffffffff,
	0xfc000000,
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
	0xffffffff
};

static void (*systemRestart)(void);

static u32 InitializeUserMempattern[] = {
	0x27bdffe0,		//	addiu	sp, sp, $ffe0
	0xffb00000,		//	sd	s0, $0000(sp)
	0xffbf0010,		//	sd	ra, $0010(sp)
	0x0c000000,		//	jal 	GetMemorySize
	0x00000000,		//	daddu	s0, a0, zero	<-- some modchips are patching here, so we'll repatch
	0x0040202d,		//	daddu	a0, v0, zero
	0x0204102b,		//	sltu	v0, s0, a0
	0x1040000a,		//	beq	v0, zero, 2f
	0xdfbf0010,		//	ld	ra, $0010(sp)
	0x700014a9,		//	por	v0, zero, zero
	0x7e020000,		//	sq	v0, $0000(s0)
	0x26100010,		//	addiu	s0, s0, $10
	0x0204102b,		//	sltu	v0, s0, a0
	0x00000000,		//	nop
	0x00000000,		//	nop
	0x1440fffa,		//	bne	v0, zero, 1b
	0x700014a9		//	por	v0, zero, zero 
/*
	0xdfbf0010,		//	ld	ra, $0010(sp)
	0xdfbf0000,		//	ld	s0, $0000(sp)
	0x03e00008,		//	jr	ra
	0x27bd0020		//	addiu	sp, sp, $0020
*/
};

static u32 InitializeUserMempattern_mask[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xfc000000,
	0x00000000,
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
	0xffffffff
};

/*----------------------------------------------------------------------------------------*/
/* This fonction is call when SifSetDma catch a reboot request.                           */
/*----------------------------------------------------------------------------------------*/
u32 New_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
	// we will use a different stack pointer for IOP reset, for the following reason:
	// some games are using the top of ScratchPad memory as stack, trigerring a
	// stack overflow during the 'hooked' IOP reset.
	// Information provided by crazyc

	// change stack pointer
	u32 _sp = 0;
#ifdef LOAD_EECORE_DOWN
	u32 _new_sp = 0x01700000;
#else
	u32 _new_sp = 0x000e7000;
#endif
	__asm__(
		"move %0, $sp\n\t"
		"move $sp, %1\n\t"		
		::"r"((u32)_sp), "r"((u32)_new_sp)
	);

	// Hook padOpen function to install In Game Reset
	if( !(g_compat_mask & COMPAT_MODE_6) && padOpen_hooked == 0 )
		padOpen_hooked = Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_HOOK);

	SifCmdResetData *reset_pkt = (SifCmdResetData*)sdd->src;

	// does IOP reset
	New_Reset_Iop(reset_pkt->arg, reset_pkt->flag);

	// restore original stack pointer
	__asm__(
		"move $sp, %0\n\t"		
		::"r"((u32)_sp)
	);

	return 1;
}


/*----------------------------------------------------------------------------------------*/
/* This fonction replace SifSetDma syscall in kernel.                                     */
/*----------------------------------------------------------------------------------------*/
u32 Hook_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
	if((sdd->attr == 0x44) && ((sdd->size==0x68) || (sdd->size==0x70))) {

		SifCmdResetData *reset_pkt = (SifCmdResetData*)sdd->src;
		if(((reset_pkt->chdr.psize == 0x68) || (reset_pkt->chdr.psize == 0x70)) && (reset_pkt->chdr.fcode == 0x80000003)) {

			__asm__(
				"la $v1, New_SifSetDma\n\t"
				"sw $v1, 8($sp)\n\t"
				"jr $ra\n\t"
				"nop\n\t"
			);
		}
	}
	__asm__(
		"move $a0, %1\n\t"
		"move $a1, %2\n\t"
		"jr   %0\n\t"
		::"r"((u32)Old_SifSetDma), "r"((u32)sdd), "r"((u32)len)
	);

	return 1;
}


/*----------------------------------------------------------------------------------------*/
/* This fonction unhook SifSetDma/SifSetReg sycalls		                          */
/*----------------------------------------------------------------------------------------*/
void Apply_Mode3(void)
{
	SetSyscall(__NR_SifSetDma, Old_SifSetDma);
	SetSyscall(__NR_SifSetReg, Old_SifSetReg);	
}


/*----------------------------------------------------------------------------------------*/
/* This fonction replace SifSetReg syscall in kernel.                                     */
/*----------------------------------------------------------------------------------------*/
int Hook_SifSetReg(u32 register_num, int register_value)
{
	if(set_reg_hook) {

		set_reg_hook--;

		if (set_reg_hook == 0) {

			if(!DisableDebug)
				GS_BGCOLOUR = 0x000000;

			// We should have a mode to do this: this is corresponding to HD-Loader's mode 3
			if ((g_compat_mask & COMPAT_MODE_3) && (iop_reboot_count == 2)) {
				__asm__(
					"la $v1, Apply_Mode3\n\t"
					"sw $v1, 8($sp)\n\t"
					"jr $ra\n\t"
					"nop\n\t"
				);
			}
		}
		return 1;
	}

	__asm__(
		"move $a0, %1\n\t"
		"move $a1, %2\n\t"
		"jr   %0\n\t"
		::"r"((u32)Old_SifSetReg), "r"((u32)register_num), "r"((u32)register_value)
	);

	return 1;
}

// ------------------------------------------------------------------------
static void init_systemRestart(void)
{
	u32 *ptr;

	DIntr();
	ee_kmode_enter();

	// scan to find kernel InitializeUserMemory()
	ptr = (u32 *)0x80001000;
	ptr = find_pattern_with_mask(ptr, 0x7f000, InitializeUserMempattern, InitializeUserMempattern_mask, sizeof(InitializeUserMempattern));
	if (!ptr)
		goto err;
	// keep original opcode address and value
	g_patchInitializeUserMem_addr = (void *)&ptr[4];
	g_patchInitializeUserMem_val = ptr[4];
	// patch InitializeUserMemory() to avoid user mem to be cleared
	_sw(0x3c100200, (u32)g_patchInitializeUserMem_addr); // it will exit at first s0/a0 comparison

	// scan to find kernel systemRestart() function
	ptr = (u32 *)0x80001000;
	ptr = find_pattern_with_mask(ptr, 0x7f000, systemRestartpattern, systemRestartpattern_mask, sizeof(systemRestartpattern));
	if (!ptr)
		goto err;
	// get systemRestart function pointer
	systemRestart = (void *)(((ptr[1] & 0x03ffffff) << 2) | 0x80000000);

	ee_kmode_exit();
	EIntr();

	FlushCache(0);

	return;

err:
	GS_BGCOLOUR = 0x0000ff; // hangs on red screen
	while (1) {;}
}

// ------------------------------------------------------------------------
static void deinit_systemRestart(void)
{
	DIntr();
	ee_kmode_enter();

	// unpatch InitializeUserMemory()
	_sw(g_patchInitializeUserMem_val, (u32)g_patchInitializeUserMem_addr);

	ee_kmode_exit();
	EIntr();

	FlushCache(0);
}

// ------------------------------------------------------------------------
static void t_loadElf(void)
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
	DPRINTF("t_loadElf: elf path = '%s'\n", g_ElfPath);

	// replacing cdrom in elf path by cddev
	if (strstr(g_argv[0], "cdrom")) {
		u8 *ptr = (u8 *)g_argv[0];
		strcpy(g_ElfPath, "cddev");
		strcat(g_ElfPath, &ptr[5]);		
	}

	DPRINTF("t_loadElf: elf path = '%s'\n", g_ElfPath);

	DPRINTF("t_loadElf: System Restart...\n");
	DIntr();
	ee_kmode_enter();
	systemRestart();
	while (!(*(vu32 *)R_EE_SBUS_SMFLAG & SBUS_CTRL_MSINT)) {;}
	*(vu32 *)R_EE_SBUS_SMFLAG = SBUS_CTRL_MSINT;
	ee_kmode_exit();
	EIntr();

	if(!DisableDebug)
		GS_BGCOLOUR = 0x00ff00;

	DPRINTF("t_loadElf: cleaning user memory...");

	// wipe user memory
	for (i = 0x00100000; i < 0x02000000; i += 64) {
		__asm__ (
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
		if (strstr(g_ElfPath, "cddev"))
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

// ------------------------------------------------------------------------
void NewLoadExecPS2(const char *filename, int argc, char *argv[])
{
	char *p = g_argbuf;
	int i, arglen;

	DIntr();
	ee_kmode_enter();

	// copy args from main ELF
	g_argc = argc > MAX_G_ARGS ? MAX_G_ARGS : argc;

	memset(g_argbuf, 0, sizeof(g_argbuf));

	strncpy(p, filename, 256);
	g_argv[0] = p;
	p += strlen(filename) + 1;
	g_argc++;

	for (i = 0; i < (g_argc-1); i++) {
		arglen = strlen(argv[i]) + 1;
		memcpy(p, argv[i], arglen);
		g_argv[i + 1] = p;
		p += arglen;
	}

	ee_kmode_exit();
	EIntr();

	ExecPS2(t_loadElf, NULL, 0, NULL);

	if(!DisableDebug)
		GS_BGCOLOUR = 0xffffff; // white screen: error
	SleepThread();
}

// ------------------------------------------------------------------------
void Hook_LoadExecPS2(const char *filename, int argc, char *argv[])
{
	__asm__(
		"la $v1, NewLoadExecPS2\n\t"
		"sw $v1, 8($sp)\n\t"
		"jr $ra\n\t"
		"nop\n\t"
	);
}

// ------------------------------------------------------------------------
int Hook_CreateThread(ee_thread_t *thread_param)
{
	// Hook padOpen function to install In Game Reset
	if( padOpen_hooked == 0 && ( thread_param->initial_priority == 0 || (thread_param->initial_priority < 5 && thread_param->current_priority == 0) ) )
		padOpen_hooked = Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_HOOK);

	return Old_CreateThread(thread_param);
}

// ------------------------------------------------------------------------
int Hook_ExecPS2(void *entry, void *gp, int num_args, char *args[])
{
	// Hook padOpen function to install In Game Reset
	if( (u32)entry >= 0x00100000 )
		padOpen_hooked = Install_PadOpen_Hook( 0x00100000, 0x01ff0000, PADOPEN_HOOK );

	__asm__(
		"move $a0, %1\n\t"
		"move $a1, %2\n\t"
		"move $a2, %3\n\t"
		"move $a3, %4\n\t"
		"jr   %0\n\t"
		::"r"((u32)Old_ExecPS2), "r"((u32)entry), "r"((u32)gp), "r"((u32)num_args), "r"((u32)args)
	);

	return 1;
}

/*----------------------------------------------------------------------------------------*/
/* Replace SifSetDma, SifSetReg, LoadExecPS2 syscalls in kernel. (Game Loader)            */
/* Replace CreateThread and ExecPS2 syscalls in kernel. (In Game Reset)                   */
/*----------------------------------------------------------------------------------------*/
void Install_Kernel_Hooks(void)
{
	init_systemRestart();

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
	deinit_systemRestart();

	if(!(g_compat_mask & COMPAT_MODE_3))
		Apply_Mode3();

	SetSyscall(__NR_LoadExecPS2, Old_LoadExecPS2);

	// If IGR is enabled unhook ExecPS2 & CreateThread syscalls
	if(!(g_compat_mask & COMPAT_MODE_6))
	{
		SetSyscall(__NR_CreateThread, Old_CreateThread);
		SetSyscall(__NR_ExecPS2, Old_ExecPS2);
	}
}
