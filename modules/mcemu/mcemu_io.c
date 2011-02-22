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

	if(vmcSpec[0].active == 0 && vmcSpec[1].active == 0)
		return 0;

	for (i = 0; i < MCEMU_PORTS; i ++, mcds ++)
	{
		DPRINTF("vmcSpec[%d].flags           = 0x%X\n", i, vmcSpec[i].flags                        );
		DPRINTF("vmcSpec[%d].cspec.PageSize  = 0x%X\n", i, vmcSpec[i].cspec.PageSize               );
		DPRINTF("vmcSpec[%d].cspec.BlockSize = 0x%X\n", i, vmcSpec[i].cspec.BlockSize              );
		DPRINTF("vmcSpec[%d].cspec.CardSize  = 0x%X\n", i, (unsigned int)vmcSpec[i].cspec.CardSize );

		if(vmcSpec[i].active == 1)
		{
			// Set virtual memorycard informations
			mcds->mcnum           = i;
			mcds->tcode           = 0x5A; /* 'Z' */
			mcds->cbufp           = &mceccbuf[i][0];
			mcds->dbufp           = &mcdatabuf[0];
			mcds->flags           = vmcSpec[i].flags;
			mcds->cspec.PageSize  = vmcSpec[i].cspec.PageSize;
			mcds->cspec.BlockSize = vmcSpec[i].cspec.BlockSize;
			mcds->cspec.CardSize  = vmcSpec[i].cspec.CardSize;
		}
		else
			mcds->mcnum           = -1;
	}

	return 1;
}

//---------------------------------------------------------------------------
int mc_read(int mc_num, void *buf, u32 page_num)
{
	register u32 lba = 0;

#ifdef HDD_DRIVER
	lba = Mcpage_to_Apasector(mc_num, page_num);
#endif

#ifdef USB_DRIVER
	lba = vmcSpec[mc_num].stsec + page_num;
#endif

#ifdef SMB_DRIVER
	lba = page_num * vmcSpec[mc_num].cspec.PageSize;
#endif

	DPRINTF("reading page 0x%X at lba 0x%X\n", (unsigned int)page_num, (unsigned int)lba);

#ifdef HDD_DRIVER
	ata_device_dma_transfer(0, buf, lba, 1, ATA_DIR_READ);
#endif

#ifdef USB_DRIVER
	mass_stor_readSector(lba, 1, buf);
#endif

#ifdef SMB_DRIVER
	if(vmcSpec[mc_num].fid == 0xFFFF)
	{
		if(!smb_OpenAndX(vmcSpec[mc_num].fname, &vmcSpec[mc_num].fid, 1))
			return 0;
	}

	smb_ReadFile(vmcSpec[mc_num].fid, lba, 0, buf, vmcSpec[mc_num].cspec.PageSize);
#endif

	return 1;
}

//---------------------------------------------------------------------------
int mc_write(int mc_num, void *buf, u32 page_num)
{
	register u32 lba = 0;

#ifdef HDD_DRIVER
	lba = Mcpage_to_Apasector(mc_num, page_num);
#endif

#ifdef USB_DRIVER
	lba = vmcSpec[mc_num].stsec + page_num;
#endif

#ifdef SMB_DRIVER
	lba = page_num * vmcSpec[mc_num].cspec.PageSize;
#endif

	DPRINTF("writing page 0x%X at lba 0x%X\n", (unsigned int)page_num, (unsigned int)lba);

#ifdef HDD_DRIVER
	ata_device_dma_transfer(0, buf, lba, 1, ATA_DIR_WRITE);
#endif

#ifdef USB_DRIVER
	mass_stor_writeSector(lba, 1, buf);
#endif

#ifdef SMB_DRIVER
	smb_WriteFile(vmcSpec[mc_num].fid, lba, 0, buf, vmcSpec[mc_num].cspec.PageSize);
#endif

	return 1;
}
