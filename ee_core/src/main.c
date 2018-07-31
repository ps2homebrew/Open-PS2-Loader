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
#ifdef GSM
#include "gsm_api.h"
#endif
#ifdef CHEAT
#include "cheat_api.h"
#endif

void *ModStorageStart, *ModStorageEnd;
void *eeloadCopy, *initUserMemory;

int isInit = 0;

static int eecoreInit(int argc, char **argv)
{
    SifInitRpc(0);

    DINIT();
    DPRINTF("OPL EE core start!\n");

    int i = 0;

    if (!_strncmp(argv[i], "USB_MODE", 8))
        GameMode = USB_MODE;
    else if (!_strncmp(argv[i], "ETH_MODE", 8))
        GameMode = ETH_MODE;
    else if (!_strncmp(argv[i], "HDD_MODE", 8))
        GameMode = HDD_MODE;
    DPRINTF("Game Mode = %d\n", GameMode);

    DisableDebug = 0;
    if (!_strncmp(&argv[i][9], "1", 1)) {
        DisableDebug = 1;
        DPRINTF("Debug Colors disabled\n");
    }

    char *p = _strtok(&argv[i][11], " ");
    if (!_strncmp(p, "Browser", 7))
        ExitPath[0] = '\0';
    else
        _strcpy(ExitPath, p);
    DPRINTF("Exit Path = (%s)\n", ExitPath);

    p = _strtok(NULL, " ");
    HDDSpindown = _strtoui(p);
    DPRINTF("HDD Spindown = %d\n", HDDSpindown);

    p = _strtok(NULL, " ");
    _strcpy(g_ps2_ip, p);
    p = _strtok(NULL, " ");
    _strcpy(g_ps2_netmask, p);
    p = _strtok(NULL, " ");
    _strcpy(g_ps2_gateway, p);
    g_ps2_ETHOpMode = _strtoui(_strtok(NULL, " "));
    DPRINTF("IP=%s NM=%s GW=%s mode: %d\n", g_ps2_ip, g_ps2_netmask, g_ps2_gateway, g_ps2_ETHOpMode);

#ifdef CHEAT
    EnableCheatOp = (gCheatList = (void *)_strtoui(_strtok(NULL, " "))) != NULL;
    DPRINTF("PS2RD Cheat Engine = %s\n", EnableCheatOp == 0 ? "Disabled" : "Enabled");
#endif

#ifdef GSM
    EnableGSMOp = _strtoi(_strtok(NULL, " "));
    DPRINTF("GSM = %s\n", EnableGSMOp == 0 ? "Disabled" : "Enabled");
#endif

#ifdef PADEMU
    EnablePadEmuOp = _strtoi(_strtok(NULL, " "));
    DPRINTF("PADEMU = %s\n", EnablePadEmuOp == 0 ? "Disabled" : "Enabled");

    PadEmuSettings = _strtoi(_strtok(NULL, " "));
#endif

    i++;

    eeloadCopy = (void *)_strtoui(_strtok(argv[i], " "));
    initUserMemory = (void *)_strtoui(_strtok(NULL, " "));
    i++;

    ModStorageStart = (void *)_strtoui(_strtok(argv[i], " "));
    ModStorageEnd = (void *)_strtoui(_strtok(NULL, " "));
    i++;

    strncpy(GameID, argv[i], sizeof(GameID) - 1);
    GameID[sizeof(GameID) - 1] = '\0';
    i++;

    // bitmask of the compat. settings
    g_compat_mask = _strtoui(argv[i]);
    DPRINTF("Compat Mask = 0x%02x\n", g_compat_mask);

    i++;

#ifdef CHEAT
    if (EnableCheatOp) {
        EnableCheats();
    }
#endif

#ifdef GSM
    if (EnableGSMOp) {
        u32 interlace, mode, ffmd, dx_offset, dy_offset;
        u64 display, syncv, smode2;

        interlace = _strtoui(_strtok(argv[i], " "));
        mode = _strtoui(_strtok(NULL, " "));
        ffmd = _strtoui(_strtok(NULL, " "));
        display = _strtoul(_strtok(NULL, " "));
        syncv = _strtoul(_strtok(NULL, " "));
        smode2 = _strtoui(_strtok(NULL, " "));
        dx_offset = _strtoui(_strtok(NULL, " "));
        dy_offset = _strtoui(_strtok(NULL, " "));

        UpdateGSMParams(interlace, mode, ffmd, display, syncv, smode2, dx_offset, dy_offset);
        EnableGSM();
    }
    i++;
#endif

    set_ipconfig();

    /* installing kernel hooks */
    DPRINTF("Installing Kernel Hooks...\n");
    Install_Kernel_Hooks();

    if (!DisableDebug)
        GS_BGCOLOUR = 0xff0000; //Blue

    SifExitRpc();

    return i;
}

int main(int argc, char **argv)
{
    int argOffset;

    if(isInit)
    {   //Ignore argv[0], as it contains the name of this module ("EELOAD"), as passed by the LoadExecPS2 syscall itself (2nd invocation and later will be from LoadExecPS2).
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

