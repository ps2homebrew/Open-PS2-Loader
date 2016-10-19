/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
   */

#include "mcemu.h"

int mc_configure(MemoryCard *mcds)
{
    register int i;

    DPRINTF("vmcSpec[0].active = %d\n", vmcSpec[0].active);
    DPRINTF("vmcSpec[1].active = %d\n", vmcSpec[1].active);

    if (vmcSpec[0].active == 0 && vmcSpec[1].active == 0)
        return 0;

    for (i = 0; i < MCEMU_PORTS; i++, mcds++) {
        DPRINTF("vmcSpec[%d].flags           = 0x%X\n", i, vmcSpec[i].flags);
        DPRINTF("vmcSpec[%d].cspec.PageSize  = 0x%X\n", i, vmcSpec[i].cspec.PageSize);
        DPRINTF("vmcSpec[%d].cspec.BlockSize = 0x%X\n", i, vmcSpec[i].cspec.BlockSize);
        DPRINTF("vmcSpec[%d].cspec.CardSize  = 0x%X\n", i, (unsigned int)vmcSpec[i].cspec.CardSize);

        if (vmcSpec[i].active == 1) {
            // Set virtual memorycard informations
            mcds->mcnum = i;
            mcds->tcode = 0x5A; /* 'Z' */
            mcds->cbufp = &mceccbuf[i][0];
            mcds->dbufp = &mcdatabuf[0];
            mcds->flags = vmcSpec[i].flags;
            mcds->cspec.PageSize = vmcSpec[i].cspec.PageSize;
            mcds->cspec.BlockSize = vmcSpec[i].cspec.BlockSize;
            mcds->cspec.CardSize = vmcSpec[i].cspec.CardSize;
        } else
            mcds->mcnum = -1;
    }

    return 1;
}
