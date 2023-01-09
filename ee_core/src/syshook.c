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
#include "patches.h"
#include "padhook.h"
#include "syshook.h"

#include <syscallnr.h>
#include <ee_regs.h>
#include <ps2_reg_defs.h>

ConfigParam CustomOSDConfigParam;

int set_reg_hook;
int set_reg_disabled;
int iop_reboot_count = 0;

int padOpen_hooked = 0;
int disable_padOpen_hook = 1;

extern void *ModStorageStart, *ModStorageEnd;
extern void *eeloadCopy, *initUserMemory;
extern void *_end;

// Global data
u32 (*Old_SifSetDma)(SifDmaTransfer_t *sdd, s32 len);
int (*Old_SifSetReg)(u32 register_num, int register_value);
int (*Old_ExecPS2)(void *entry, void *gp, int num_args, char *args[]);
int (*Old_CreateThread)(ee_thread_t *thread_param);
void (*Old_Exit)(s32 exit_code);
void (*Old_SetOsdConfigParam)(ConfigParam *config);
void (*Old_GetOsdConfigParam)(ConfigParam *config);

/*----------------------------------------------------------------------------------------*/
/* This function is called when SifSetDma catches a reboot request.                       */
/*----------------------------------------------------------------------------------------*/
u32 New_SifSetDma(SifDmaTransfer_t *sdd, s32 len)
{
    // Hook padOpen function to install In Game Reset
    if (!(g_compat_mask & COMPAT_MODE_6) && padOpen_hooked == 0) {
        Install_IGR();
        padOpen_hooked = Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_HOOK);
    }

    struct _iop_reset_pkt *reset_pkt = (struct _iop_reset_pkt *)sdd->src;

    disable_padOpen_hook = 1;

    // does IOP reset
    New_Reset_Iop(reset_pkt->arg, reset_pkt->arglen);

    disable_padOpen_hook = 0;

    return 1;
}

// ------------------------------------------------------------------------
void sysLoadElf(char *filename, int argc, char **argv)
{
    int r;
    t_ExecData elf;

    SifInitRpc(0);

    DPRINTF("t_loadElf()\n");

    DPRINTF("t_loadElf: Resetting IOP...\n");

    set_reg_disabled = 0;
    New_Reset_Iop(NULL, 0);
    set_reg_disabled = 1;

    iop_reboot_count = 1;

    SifInitRpc(0);
    LoadFileInit();

    DPRINTF("t_loadElf: elf path = '%s'\n", filename);

    if (EnableDebug)
        GS_BGCOLOUR = 0x00ff00; // Green

    DPRINTF("t_loadElf: cleaning user memory...");

    // wipe user memory
    WipeUserMemory((void *)&_end, (void *)ModStorageStart);
    // The upper half (from ModStorageEnd to GetMemorySize()) is taken care of by LoadExecPS2().
    // WipeUserMemory((void *)ModStorageEnd, (void *)GetMemorySize());

    FlushCache(0);

    DPRINTF(" done\n");

    DPRINTF("t_loadElf: loading elf...");
    r = LoadElf(filename, &elf);

    if (!r) {
        DPRINTF(" done\n");

        DPRINTF("t_loadElf: trying to apply patches...\n");
        // applying needed patches
        apply_patches(filename);

        FlushCache(0);
        FlushCache(2);

        DPRINTF("t_loadElf: exiting services...\n");
        // exit services
        SifExitIopHeap();
        LoadFileExit();
        SifExitRpc();

        disable_padOpen_hook = 0;

        DPRINTF("t_loadElf: executing...\n");
        CleanExecPS2((void *)elf.epc, (void *)elf.gp, argc, argv);
    }

    DPRINTF(" failed\n");

    // Error
    GS_BGCOLOUR = 0xffffff; // White    - shouldn't happen.
    SleepThread();
}

static void unpatchEELOADCopy(void)
{
    vu32 *p = (vu32 *)eeloadCopy;

    p[1] = 0x0240302D; /* daddu    a2, s2, zero */
    p[2] = 0x8FA50014; /* lw       a1, 0x0014(sp) */
    p[3] = 0x8C67000C; /* lw       a3, 0x000C(v1) */
}

static void unpatchInitUserMemory(void)
{
    vu16 *p = (vu16 *)initUserMemory;

    /*
     * Reset the start of user memory to 0x00082000, by changing the immediate value being loaded into $a0.
     *  lui  $a0, 0x0008
     *  jal  InitializeUserMemory
     *  ori  $a0, $a0, 0x2000
     */
    p[0] = 0x0008;
    p[4] = 0x2000;
}

void sysExit(s32 exit_code)
{
    Remove_Kernel_Hooks();
    IGR_Exit(exit_code);
}

void hook_SetOsdConfigParam(ConfigParam *config)
{
    DPRINTF("%s: called\n", __func__);
    CustomOSDConfigParam.spdifMode = config->spdifMode;
    CustomOSDConfigParam.screenType = config->screenType;
    CustomOSDConfigParam.videoOutput = config->videoOutput;
    CustomOSDConfigParam.japLanguage = config->japLanguage;
    CustomOSDConfigParam.ps1drvConfig = config->ps1drvConfig;
    CustomOSDConfigParam.version = config->version;
    CustomOSDConfigParam.language = config->language;
    CustomOSDConfigParam.timezoneOffset = config->timezoneOffset;
}

void hook_GetOsdConfigParam(ConfigParam *config)
{
    DPRINTF("%s: called\n", __func__);
    config->spdifMode = CustomOSDConfigParam.spdifMode;
    config->screenType = CustomOSDConfigParam.screenType;
    config->videoOutput = CustomOSDConfigParam.videoOutput;
    config->japLanguage = CustomOSDConfigParam.japLanguage;
    config->ps1drvConfig = CustomOSDConfigParam.ps1drvConfig;
    config->version = CustomOSDConfigParam.version;
    config->language = CustomOSDConfigParam.language;
    config->timezoneOffset = CustomOSDConfigParam.timezoneOffset;
}

/*----------------------------------------------------------------------------------------*/
/* Replace SifSetDma, SifSetReg, LoadExecPS2 syscalls in kernel. (Game Loader)            */
/* Replace CreateThread and ExecPS2 syscalls in kernel. (In Game Reset)                   */
/*----------------------------------------------------------------------------------------*/
void Install_Kernel_Hooks(void)
{
    if (enforceLanguage) {
        Old_SetOsdConfigParam = GetSyscallHandler(__NR_SetOsdConfigParam);
        SetSyscall(__NR_SetOsdConfigParam, &hook_SetOsdConfigParam);
        Old_GetOsdConfigParam = GetSyscallHandler(__NR_GetOsdConfigParam);
        SetSyscall(__NR_GetOsdConfigParam, &hook_GetOsdConfigParam);
    }

    Old_SifSetDma = GetSyscallHandler(__NR_SifSetDma);
    SetSyscall(__NR_SifSetDma, &Hook_SifSetDma);

    Old_SifSetReg = GetSyscallHandler(__NR_SifSetReg);
    SetSyscall(__NR_SifSetReg, &Hook_SifSetReg);

    // If IGR is enabled hook ExecPS2 & CreateThread syscalls
    if (!(g_compat_mask & COMPAT_MODE_6)) {
        Old_CreateThread = GetSyscallHandler(__NR_CreateThread);
        SetSyscall(__NR_CreateThread, &Hook_CreateThread);

        Old_ExecPS2 = GetSyscallHandler(__NR__ExecPS2);
        SetSyscall(__NR__ExecPS2, &Hook_ExecPS2);
    }

    Old_Exit = GetSyscallHandler(__NR_KExit);
    SetSyscall(__NR_KExit, &Hook_Exit);
}

/*----------------------------------------------------------------------------------------------*/
/* Restore original SifSetDma, SifSetReg, LoadExecPS2, Exit syscalls in kernel. (Game loader)   */
/* Restore original CreateThread and ExecPS2 syscalls in kernel. (In Game Reset)                */
/*----------------------------------------------------------------------------------------------*/
void Remove_Kernel_Hooks(void)
{
    if (enforceLanguage) {
        SetSyscall(__NR_SetOsdConfigParam, Old_SetOsdConfigParam);
        SetSyscall(__NR_GetOsdConfigParam, Old_GetOsdConfigParam);
    }
    SetSyscall(__NR_SifSetDma, Old_SifSetDma);
    SetSyscall(__NR_SifSetReg, Old_SifSetReg);
    SetSyscall(__NR_KExit, Old_Exit);

    DI();
    ee_kmode_enter();

    unpatchEELOADCopy();
    unpatchInitUserMemory();

    ee_kmode_exit();
    EI();

    // If IGR is enabled unhook ExecPS2 & CreateThread syscalls
    if (!(g_compat_mask & COMPAT_MODE_6)) {
        SetSyscall(__NR_CreateThread, Old_CreateThread);
        SetSyscall(__NR__ExecPS2, Old_ExecPS2);
    }

    FlushCache(0);
    FlushCache(2);
}
