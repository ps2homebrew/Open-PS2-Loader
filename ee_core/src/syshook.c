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

#define MAX_ARGS 15

int g_argc;
char *g_argv[1 + MAX_ARGS];

int set_reg_hook;
int set_reg_disabled;
int iop_reboot_count = 0;

int padOpen_hooked = 0;
int disable_padOpen_hook = 1;

extern void *ModStorageStart, *ModStorageEnd;

/*----------------------------------------------------------------------------------------*/
/* This function is called when SifSetDma catches a reboot request.                       */
/*----------------------------------------------------------------------------------------*/
u32 New_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
    // Hook padOpen function to install In Game Reset
    if (!(g_compat_mask & COMPAT_MODE_6) && padOpen_hooked == 0)
        padOpen_hooked = Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_HOOK);

    struct _iop_reset_pkt *reset_pkt = (struct _iop_reset_pkt *)sdd->src;

    disable_padOpen_hook = 1;

    // does IOP reset
    New_Reset_Iop(reset_pkt->arg, reset_pkt->arglen);

    disable_padOpen_hook = 0;

    return 1;
}

// ------------------------------------------------------------------------
extern void *_end;

static void WipeUserMemory(void *start, void *end)
{
    unsigned int i;

    for (i = (unsigned int)start; i < (unsigned int)end; i += 64) {
        if (i == (unsigned int)ModStorageStart)
            i = (unsigned int)ModStorageEnd;

        __asm__ __volatile__(
            "\tsq $0, 0(%0) \n"
            "\tsq $0, 16(%0) \n"
            "\tsq $0, 32(%0) \n"
            "\tsq $0, 48(%0) \n" ::"r"(i));
    }
}

void t_loadElf(void)
{
    int r;
    t_ExecData elf;

    DPRINTF("t_loadElf()\n");

    ResetSPU();

    DPRINTF("t_loadElf: Resetting IOP...\n");

    set_reg_disabled = 0;
    New_Reset_Iop(NULL, 0);
    set_reg_disabled = 1;

    iop_reboot_count = 1;

    SifInitRpc(0);
    LoadFileInit();

    DPRINTF("t_loadElf: elf path = '%s'\n", g_argv[0]);

    if (!DisableDebug)
        GS_BGCOLOUR = 0x00ff00; //Green

    DPRINTF("t_loadElf: cleaning user memory...");

    // wipe user memory
    WipeUserMemory((void *)&_end, (void *)ModStorageStart);
    WipeUserMemory((void *)ModStorageEnd, (void *)GetMemorySize());

    FlushCache(0);
    FlushCache(2);

    DPRINTF(" done\n");

    DPRINTF("t_loadElf: loading elf...");
    r = LoadElf(g_argv[0], &elf);

    if ((!r) && (elf.epc)) {
        DPRINTF(" done\n");

        DPRINTF("t_loadElf: trying to apply patches...\n");
        // applying needed patches
        apply_patches();

        FlushCache(0);
        FlushCache(2);

        DPRINTF("t_loadElf: exiting services...\n");
        // exit services
        SifExitIopHeap();
        LoadFileExit();
        SifExitRpc();

        disable_padOpen_hook = 0;

        DPRINTF("t_loadElf: executing...\n");
        _ExecPS2((void *)elf.epc, (void *)elf.gp, g_argc, g_argv);
    }

    DPRINTF(" failed\n");

    //Error
    GS_BGCOLOUR = 0xffffff; //White	- shouldn't happen.
    SleepThread();
}

/*----------------------------------------------------------------------------------------*/
/* Replace SifSetDma, SifSetReg, LoadExecPS2 syscalls in kernel. (Game Loader)            */
/* Replace CreateThread and ExecPS2 syscalls in kernel. (In Game Reset)                   */
/*----------------------------------------------------------------------------------------*/
void Install_Kernel_Hooks(void)
{
    Old_SifSetDma = GetSyscallHandler(__NR_SifSetDma);
    SetSyscall(__NR_SifSetDma, &Hook_SifSetDma);

    Old_SifSetReg = GetSyscallHandler(__NR_SifSetReg);
    SetSyscall(__NR_SifSetReg, &Hook_SifSetReg);

    Old_LoadExecPS2 = GetSyscallHandler(__NR__LoadExecPS2);
    SetSyscall(__NR__LoadExecPS2, &Hook_LoadExecPS2);

    // If IGR is enabled hook ExecPS2 & CreateThread syscalls
    if (!(g_compat_mask & COMPAT_MODE_6)) {
        Old_CreateThread = GetSyscallHandler(__NR_CreateThread);
        SetSyscall(__NR_CreateThread, &Hook_CreateThread);

        Old_ExecPS2 = GetSyscallHandler(__NR__ExecPS2);
        SetSyscall(__NR__ExecPS2, &Hook_ExecPS2);
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
    SetSyscall(__NR__LoadExecPS2, Old_LoadExecPS2);

    // If IGR is enabled unhook ExecPS2 & CreateThread syscalls
    if (!(g_compat_mask & COMPAT_MODE_6)) {
        SetSyscall(__NR_CreateThread, Old_CreateThread);
        SetSyscall(__NR__ExecPS2, Old_ExecPS2);
    }
}
