/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
   */

#include "mcemu.h"

#define VMC_DIR_CHAIN_SIZE	4

static void usb_open_images(void)
{
	fat_dir_chain_record *pointsBuffer;
	int OldState;
	short int i, parts;

	for (i = 0, parts = 0; i < MCEMU_PORTS; i ++)
	{
		if(vmcSpec[i].active == 1)
			parts++;
	}

	CpuSuspendIntr(&OldState);
	pointsBuffer = AllocSysMemory(ALLOC_FIRST, parts * VMC_DIR_CHAIN_SIZE * sizeof(fat_dir_chain_record), NULL);
	CpuResumeIntr(OldState);

	for (i = 0; i < MCEMU_PORTS; i ++)
	{
		if(vmcSpec[i].active == 1)
			fat_setFatDirChain(&vmcSpec[i].fatDir, vmcSpec[i].file.cluster, vmcSpec[i].file.size, VMC_DIR_CHAIN_SIZE, &pointsBuffer[i * VMC_DIR_CHAIN_SIZE]);
	}
}

int DeviceWritePage(int mc_num, void *buf, u32 page_num)
{
	u32 offset;

	offset = page_num * vmcSpec[mc_num].cspec.PageSize;
	DPRINTF("writing page 0x%lx at offset 0x%lx\n", page_num, offset);

	return(fat_fileIO(&vmcSpec[mc_num].fatDir, FAT_PARTNUM_VMC0 + mc_num, FAT_IO_MODE_WRITE, offset, buf, vmcSpec[mc_num].cspec.PageSize) == vmcSpec[mc_num].cspec.PageSize ? 1 : 0);
}

int DeviceReadPage(int mc_num, void *buf, u32 page_num)
{
	static unsigned char usb_inited = 0;
	u32 offset;

	offset = page_num * vmcSpec[mc_num].cspec.PageSize;
	DPRINTF("reading page 0x%lx at offset 0x%lx\n", page_num, offset);

	if(!usb_inited)
	{
		usb_inited = 1;
		usb_open_images();
	}

	return(fat_fileIO(&vmcSpec[mc_num].fatDir, FAT_PARTNUM_VMC0 + mc_num, FAT_IO_MODE_READ, offset, buf, vmcSpec[mc_num].cspec.PageSize) == vmcSpec[mc_num].cspec.PageSize ? 1 : 0);
}
