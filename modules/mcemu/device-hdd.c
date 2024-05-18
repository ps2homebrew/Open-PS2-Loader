/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
   */

#include "mcemu.h"

/* Return ata sector corresponding to page number in vmc file */
static u32 Mcpage_to_Apasector(int mc_num, u32 mc_page)
{
    register int i;
    register u32 sector_to_read, lbound, ubound;

    ubound = 0;
    sector_to_read = 0;

    for (i = 0; i < 10; i++) {
        lbound = ubound;
        ubound += vmcSpec[mc_num].blocks[i].count;

        if ((mc_page >= (lbound << 4)) && (mc_page < (ubound << 4)))
            sector_to_read = vmcSpec[mc_num].parts[vmcSpec[mc_num].blocks[i].subpart].start + (vmcSpec[mc_num].blocks[i].number << 4) + (mc_page - (lbound << 4));
    }

    return sector_to_read;
}

int DeviceWritePage(int mc_num, void *buf, u32 page_num)
{
    u32 lba;

    lba = Mcpage_to_Apasector(mc_num, page_num);
    DPRINTF("writing page 0x%lx at lba 0x%lx\n", page_num, lba);

    return (sceAtaDmaTransfer(0, buf, lba, 1, ATA_DIR_WRITE) == 0 ? 1 : 0);
}

int DeviceReadPage(int mc_num, void *buf, u32 page_num)
{
    u32 lba;

    lba = Mcpage_to_Apasector(mc_num, page_num);
    DPRINTF("reading page 0x%lx at lba 0x%lx\n", page_num, lba);

    return (sceAtaDmaTransfer(0, buf, lba, 1, ATA_DIR_READ) == 0 ? 1 : 0);
}

void DeviceShutdown(void)
{
}
