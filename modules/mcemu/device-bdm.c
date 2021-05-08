/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
   */

#include "mcemu.h"

int DeviceWritePage(int mc_num, void *buf, u32 page_num)
{
    u32 lba;

    lba = vmcSpec[mc_num].stsec + page_num;
    DPRINTF("writing page 0x%lx at lba 0x%lx\n", page_num, lba);

    bdm_writeSector(lba, 1, buf);

    return 1;
}

int DeviceReadPage(int mc_num, void *buf, u32 page_num)
{
    u32 lba;

    lba = vmcSpec[mc_num].stsec + page_num;
    DPRINTF("reading page 0x%lx at lba 0x%lx\n", page_num, lba);

    bdm_readSector(lba, 1, buf);

    return 1;
}

void DeviceShutdown(void)
{
}
