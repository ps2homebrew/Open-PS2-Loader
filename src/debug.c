/*
  Copyright 2010, jimmikaelakel
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/ethsupport.h"
#include "include/system.h"
#include "include/ioman.h"
#include "include/extern_irx.h"


int debugSetActive(void)
{
#ifndef _DTL_T10000
    int ret;

    if ((ret = ethLoadInitModules()) != 0)
        return -1;

    ret = sysLoadModuleBuffer(&udptty_irx, size_udptty_irx, 0, NULL);
    if (ret < 0)
        return -8;

    ret = sysLoadModuleBuffer(&ioptrap_irx, size_ioptrap_irx, 0, NULL);
    if (ret < 0)
        return -9;

    ret = sysLoadModuleBuffer(&ps2link_irx, size_ps2link_irx, 0, NULL);
    if (ret < 0)
        return -10;
#endif

    return 0;
}
