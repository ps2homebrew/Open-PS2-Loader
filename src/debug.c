/*
  Copyright 2010, jimmikaelakel
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/ethsupport.h"
#include "include/system.h"
#include "include/ioman.h"
#include "include/imports.h"

static u8 modulesLoaded = 0;

int debugSetActive(void)
{
#ifndef _DTL_T10000
    int ret;

#if defined(TTY_UDP) || defined(__DECI2_DEBUG)
    if ((ret = ethLoadInitModules()) != 0)
        return -1;
#endif

#ifdef __DECI2_DEBUG
    LOG("[DRVTIF]:\n");
    ret = sysLoadModuleBuffer(&drvtif_irx, size_drvtif_irx, 0, NULL);
    if (ret < 0)
        return -8;

    LOG("[TIFNET]:\n");
    ret = sysLoadModuleBuffer(&tifinet_irx, size_tifinet_irx, 0, NULL);
    if (ret < 0)
        return -9;
#elif defined(TTY_UDP)
    LOG("[UDPTTY]:\n");
    ret = sysLoadModuleBuffer(&udptty_irx, size_udptty_irx, 0, NULL);
    if (ret < 0)
        return -8;

    LOG("[IOPTRAP]:\n");
    ret = sysLoadModuleBuffer(&ioptrap_irx, size_ioptrap_irx, 0, NULL);
    if (ret < 0)
        return -9;

    LOG("[PS2LINK]:\n");
    ret = sysLoadModuleBuffer(&ps2link_irx, size_ps2link_irx, 0, NULL);
    if (ret < 0)
        return -10;
#elif defined(TTY_PPC_UART)
    LOG("[PPCTTY]:\n");
    ret = sysLoadModuleBuffer(&ppctty_irx, size_ppctty_irx, 0, NULL);
    if (ret < 0)
        return -8;

    LOG("[IOPTRAP]:\n");
    ret = sysLoadModuleBuffer(&ioptrap_irx, size_ioptrap_irx, 0, NULL);
    if (ret < 0)
        return -9;
#endif
#endif

    modulesLoaded = 1;

    return 0;
}

void debugApplyConfig(void)
{
#ifndef _DTL_T10000
    if (modulesLoaded)
        ethApplyConfig();
#endif
}
