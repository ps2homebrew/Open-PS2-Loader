/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# APA journal related routines
*/

#include <errno.h>
#include <iomanX.h>
#include <atad.h>
#include <sysclib.h>
#include <stdio.h>
#include <hdd-ioctl.h>

#include "apa-opt.h"
#include "libapa.h"

//  Globals
static apa_journal_t journalBuf;

int apaJournalFlush(s32 device)
{// this write any thing that in are journal buffer :)
	if(ata_device_flush_cache(device))
		return -EIO;
	if(ata_device_sector_io(device, &journalBuf, APA_SECTOR_APAL, 1, ATA_DIR_WRITE))
		return -EIO;
	if(ata_device_flush_cache(device))
		return -EIO;
	return 0;
}

int apaJournalReset(s32 device)
{
	memset(&journalBuf, 0, sizeof(apa_journal_t));
	journalBuf.magic=APAL_MAGIC;
	return apaJournalFlush(device);
}

int apaJournalWrite(apa_cache_t *clink)
{
	clink->header->checksum=journalCheckSum(clink->header);
	if(ata_device_sector_io(clink->device, clink->header,
		(journalBuf.num << 1)+APA_SECTOR_APAL_HEADERS, 2, ATA_DIR_WRITE))
			return -EIO;
	journalBuf.sectors[journalBuf.num]=clink->sector;
	journalBuf.num++;
	return 0;
}

int apaJournalRestore(s32 device)
{	// copys apa headers from apal to apa system
	int i;
	u32 sector;
	apa_cache_t *clink;

	APA_PRINTF(APA_DRV_NAME": checking log...\n");
	if(ata_device_sector_io(device, &journalBuf, APA_SECTOR_APAL, sizeof(apa_journal_t)/512, ATA_DIR_READ)){
		apaJournalReset(device);
		return -EIO;
	}
	if(journalBuf.magic==APAL_MAGIC)
	{
		if(journalBuf.num==0)
			return 0;
		clink=apaCacheAlloc();
		for(i=0, sector=APA_SECTOR_APAL_HEADERS;i<journalBuf.num;i++, sector+=2)
		{
			if(ata_device_sector_io(device, clink->header, sector, 2, ATA_DIR_READ))
				break;
			if(ata_device_sector_io(device, clink->header, journalBuf.sectors[i], 2, ATA_DIR_WRITE))
				break;
		}
		apaCacheFree(clink);
		return apaJournalReset(device);// only do if journal..
	}
	memset(&journalBuf, 0, sizeof(apa_journal_t));// safe e
	journalBuf.magic=APAL_MAGIC;
	return 0;//-EINVAL;
}
