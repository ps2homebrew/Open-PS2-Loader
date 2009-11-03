/*
  Copyright 2009, Ifcaro & jimmikaelkael
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#include "loader.h"
#include "iopmgr.h"

static int g_argc;
static char *g_argv[1 + MAX_ARGS];
static char g_argbuf[256];
static char g_ElfPath[1024];

int set_reg_hook;
int set_reg_disabled;
int iop_reboot_count = 0;

/*----------------------------------------------------------------------------------------*/
/* This fonction is call when SifSetDma catch a reboot request.                           */
/*----------------------------------------------------------------------------------------*/
u32 New_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
	SifCmdResetData *reset_pkt = (SifCmdResetData*)sdd->src;
	New_Reset_Iop(reset_pkt->arg, reset_pkt->flag);

	return 1;
}


/*----------------------------------------------------------------------------------------*/
/* This fonction replace SifSetDma syscall in kernel.                                     */
/*----------------------------------------------------------------------------------------*/
u32 Hook_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
	if((sdd->attr == 0x44) && ((sdd->size==0x68) || (sdd->size==0x70)))
	{
		SifCmdResetData *reset_pkt = (SifCmdResetData*)sdd->src;
		if(((reset_pkt->chdr.psize == 0x68) || (reset_pkt->chdr.psize == 0x70)) && (reset_pkt->chdr.fcode == 0x80000003))
		{
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
/* This fonction replace SifSetReg syscall in kernel.                                     */
/*----------------------------------------------------------------------------------------*/
int Hook_SifSetReg(u32 register_num, int register_value)
{
	if(set_reg_hook)
	{
		set_reg_hook--;

		if (set_reg_hook == 0) {

			GS_BGCOLOUR = 0x000000;

			// We should have a mode to do this: this is corresponding to HD-Loader's mode 3
			if (iop_reboot_count == 2) {
				//SetSyscall(119, Old_SifSetDma);
				//SetSyscall(121, Old_SifSetReg);
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
void NewLoadExecPS2(const char *filename, int argc, char *argv[])
{
	char *p = g_argbuf;
	int i, r;
	t_ExecData elf;

	DI();
	ee_kmode_enter();
	
	// copy args from main ELF
	g_argc = argc > MAX_ARGS ? MAX_ARGS : argc;

	memset(g_argbuf, 0, sizeof(g_argbuf));

	strcpy(p, filename);
	g_argv[0] = p;
	p += strlen(filename) + 1;
	g_argc++;

	for (i = 0; i < argc; i++) {
		strcpy(p, argv[i]);
		g_argv[i + 1] = p;
		p += strlen(argv[i]) + 1;
	}
	
	ee_kmode_exit();
	EI();
		
	ResetEE(0x7f);

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

	// clear scratchpad memory
	memset((void*)0x70000000, 0, 16 * 1024);
	
    SifExitRpc();

    set_reg_disabled = 0;
    New_Reset_Iop("rom0:UDNL rom0:EELOADCNF", 0);
    set_reg_disabled = 1;
        
	SifInitRpc(0);

    cdInit(CDVD_INIT_INIT);
	
	// replacing cdrom in elf path by iso
	if (strstr(g_argv[0], "cdrom")) {
		u8 *ptr = (u8 *)g_argv[0];
		strcpy(g_ElfPath, "iso");
		strcat(g_ElfPath, &ptr[5]);		
	}
	
	SifInitRpc(0);
	LoadFileInit();

	GS_BGCOLOUR = 0x00ff00; 
		
	r = LoadElf(g_ElfPath, &elf);
		
	if ((!r) && (elf.epc)) {
		// exit services
		fioExit();
		LoadFileExit();
		SifExitIopHeap();
		SifExitRpc();

		FlushCache(0);
		FlushCache(2);
		
		ExecPS2((void*)elf.epc, (void*)elf.gp, g_argc, g_argv);
	}

	GS_BGCOLOUR = 0xffffff; // white screen: error
	SleepThread();	
}

// ------------------------------------------------------------------------
void HookLoadExecPS2(const char *filename, int argc, char *argv[])
{
	__asm__(
		"la $v1, NewLoadExecPS2\n\t"
		"sw $v1, 8($sp)\n\t"
		"jr $ra\n\t"
		"nop\n\t"
	);	
}

/*----------------------------------------------------------------------------------------*/
/* Replace SifSetDma and SifSetReg syscalls in kernel.                                    */
/*----------------------------------------------------------------------------------------*/
void Install_Kernel_Hooks(void)
{
	Old_SifSetDma  = GetSyscallHandler(119);
	SetSyscall(119, &Hook_SifSetDma);

	Old_SifSetReg  = GetSyscallHandler(121);
	SetSyscall(121, &Hook_SifSetReg);
	
	Old_LoadExecPS2 = GetSyscallHandler(6);
	SetSyscall(6, &HookLoadExecPS2);	
}
