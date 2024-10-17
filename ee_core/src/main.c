/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

#include "ee_core.h"
#include "modmgr.h"
#include "util.h"
#include "syshook.h"
#include "gsm_api.h"
#include "cheat_api.h"
#include "coreconfig.h"

int isInit = 0;

// Global data
char g_ipconfig[IPCONFIG_MAX_LEN];
int g_ipconfig_len;
u32 g_compat_mask;
int EnableDebug;

// This function is defined as weak in ps2sdkc, so how
// we are not using time zone, so we can safe some KB
void _ps2sdk_timezone_update() {}

DISABLE_PATCHED_FUNCTIONS();      // Disable the patched functionalities
DISABLE_EXTRA_TIMERS_FUNCTIONS(); // Disable the extra functionalities for timers

struct EECoreConfig_t g_ee_core_config = {.magic[0] = EE_CORE_MAGIC_0, .magic[1] = EE_CORE_MAGIC_1};

static int eecoreInit(int argc, char **argv)
{
    USE_LOCAL_EECORE_CONFIG;

    SifInitRpc(0);

    DINIT();
    DPRINTF("OPL EE core start!\n");

    if (!_strncmp(config->GameModeDesc, "BDM_ILK_MODE", 12))
        config->GameMode = BDM_ILK_MODE;
    else if (!_strncmp(config->GameModeDesc, "BDM_M4S_MODE", 12))
        config->GameMode = BDM_M4S_MODE;
    else if (!_strncmp(config->GameModeDesc, "BDM_USB_MODE", 12))
        config->GameMode = BDM_USB_MODE;
    else if (!_strncmp(config->GameModeDesc, "ETH_MODE", 8))
        config->GameMode = ETH_MODE;
    else if (!_strncmp(config->GameModeDesc, "HDD_MODE", 8))
        config->GameMode = HDD_MODE;
    DPRINTF("Game Mode = %d %s\n", config->GameMode, config->GameModeDesc);

    EnableDebug = config->EnableDebug;
    DPRINTF("EnableDebug = %d\n", config->EnableDebug);

    if (!_strncmp(config->ExitPath, "Browser", 7))
        config->ExitPath[0] = '\0';

    DPRINTF("Exit Path = (%s)\n", config->ExitPath);
    DPRINTF("HDD Spindown = %d\n", config->HDDSpindown);

    DPRINTF("IP=%s NM=%s GW=%s mode: %d\n", config->g_ps2_ip, config->g_ps2_netmask, config->g_ps2_gateway, config->g_ps2_ETHOpMode);

    DPRINTF("PS2RD Cheat Engine = %s\n", config->gCheatList == NULL ? "Disabled" : "Enabled");

    DPRINTF("GSM = %s\n", config->EnableGSMOp == 0 ? "Disabled" : "Enabled");

#ifdef PADEMU
    DPRINTF("PADEMU = %s\n", config->EnablePadEmuOp == 0 ? "Disabled" : "Enabled");
#endif

    DPRINTF("enforceLanguage = %s\n", config->enforceLanguage == 0 ? "Disabled" : "Enabled");

    DPRINTF("eeloadCopy = 0x%08X\n", config->eeloadCopy);
    DPRINTF("initUserMemory = 0x%08X\n", config->initUserMemory);

    DPRINTF("ModStorageStart = 0x%08X\n", config->ModStorageStart);
    DPRINTF("ModStorageEnd = 0x%08X\n", config->ModStorageEnd);

    DPRINTF("GameID = %s\n", config->GameID);

    // g_compat_mask is used on ASM directly.
    g_compat_mask = config->_CompatMask;
    DPRINTF("Compat Mask = 0x%02x\n", g_compat_mask);

    if (config->gCheatList) {
        EnableCheats();
    }

    if (config->EnableGSMOp) {
        UpdateGSMParams(
            config->GsmConfig.interlace,
            config->GsmConfig.mode,
            config->GsmConfig.ffmd,
            config->GsmConfig.display,
            config->GsmConfig.syncv,
            config->GsmConfig.smode2,
            config->GsmConfig.dx_offset,
            config->GsmConfig.dy_offset,
            config->GsmConfig.k576P_fix,
            config->GsmConfig.kGsDxDyOffsetSupported,
            config->GsmConfig.FIELD_fix);
        EnableGSM();
    }

    set_ipconfig();

    /* installing kernel hooks */
    DPRINTF("Installing Kernel Hooks...\n");
    Install_Kernel_Hooks();

    if (config->EnableDebug)
        DBGCOL(0xFF0000, MAIN, "Reached end of eecoreinit()");

    SifExitRpc();

    return 0;
}

int main(int argc, char **argv)
{
    int argOffset;

    if (isInit) { // Ignore argv[0], as it contains the name of this module ("EELOAD"), as passed by the LoadExecPS2 syscall itself (2nd invocation and later will be from LoadExecPS2).
        argv++;
        argc--;

        sysLoadElf(argv[0], argc, argv);
    } else {
        argOffset = eecoreInit(argc, argv);
        isInit = 1;

        LoadExecPS2(argv[argOffset], argc - 1 - argOffset, &argv[1 + argOffset]);
    }

    return 0;
}
