/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
# PFS metadata journal related routines
*/

#include "pfs.h"

///////////////////////////////////////////////////////////////////////////////
//   Globals

pfs_journal_t journalBuf;

///////////////////////////////////////////////////////////////////////////////
//   Function defenitions

int journalCheckSum(void *header)
{
	u32 *ptr=(u32 *)header;
	u32 sum=0;
	int i;
	for(i=2; i < 256; i++)
		sum+=ptr[i];
	return sum & 0xFFFF;
}

void journalWrite(pfs_mount_t *pfsMount, pfs_cache_t *clink, u32 numBuffers)
{
	u32 i=0;
	u32 logSector=2;

	for(i=0; i <numBuffers;i++)
	{
		if((clink[i].flags & CACHE_FLAG_DIRTY) && clink[i].pfsMount == pfsMount) {
			if(clink[i].flags & (CACHE_FLAG_SEGD|CACHE_FLAG_SEGI))
				clink[i].u.inode->checksum=inodeCheckSum(clink[i].u.inode);
			journalBuf.log[journalBuf.num].sector = clink[i].sector << blockSize;
			journalBuf.log[journalBuf.num].sub = clink[i].sub;
			journalBuf.log[journalBuf.num].logSector = logSector;
			journalBuf.num+=1;
		}
		logSector+=2;
	}

	if(pfsMount->blockDev->transfer(pfsMount->fd, clink->u.inode, 0,
		(pfsMount->log.number << pfsMount->sector_scale) + 2, numBuffers*2,
			IOCTL2_TMODE_WRITE)>=0)
				journalFlush(pfsMount);
}

int journalReset(pfs_mount_t *pfsMount)
{
	int rv;

	memset(&journalBuf, 0, sizeof(pfs_journal_t));
	journalBuf.magic=PFS_JOUNRNAL_MAGIC;

	pfsMount->blockDev->flushCache(pfsMount->fd);

	rv = pfsMount->blockDev->transfer(pfsMount->fd, &journalBuf, 0,
		(pfsMount->log.number << pfsMount->sector_scale), 2, IOCTL2_TMODE_WRITE);

	pfsMount->blockDev->flushCache(pfsMount->fd);
	return rv;
}

int journalResetThis(block_device *blockDev, int fd, u32 sector)
{
	memset(&journalBuf, 0, sizeof(pfs_journal_t));
	journalBuf.magic=PFS_JOUNRNAL_MAGIC;
	return blockDev->transfer(fd, &journalBuf, 0, sector, 2, 1);
}

int journalFlush(pfs_mount_t *pfsMount)
{// this write any thing that in are journal buffer :)
	int rv;

	pfsMount->blockDev->flushCache(pfsMount->fd);

	journalBuf.checksum=journalCheckSum(&journalBuf);

	rv=pfsMount->blockDev->transfer(pfsMount->fd, &journalBuf, 0,
		(pfsMount->log.number << pfsMount->sector_scale), 2, IOCTL2_TMODE_WRITE);

	pfsMount->blockDev->flushCache(pfsMount->fd);
	return rv;
}

int journalResetore(pfs_mount_t *pfsMount)
{
	int rv;
	int result;
	pfs_cache_t *clink;
	u32 i;


	// Read journal buffer from disk
	rv = pfsMount->blockDev->transfer(pfsMount->fd, &journalBuf, 0,
		(pfsMount->log.number << pfsMount->sector_scale), 2, IOCTL2_TMODE_READ);

	if(rv || (journalBuf.magic != PFS_JOUNRNAL_MAGIC) ||
		(journalBuf.checksum != (u16)journalCheckSum(&journalBuf)))
		{
			printf("ps2fs: Error: cannot read log/invalid log\n");
			return journalReset(pfsMount);
		}

	if(journalBuf.num == 0)
	{
		dprintf("ps2fs: System cleanly un-mounted, no journal restore needed\n");
		return journalReset(pfsMount);
	}

	clink = cacheAllocClean(&result);
	if(!clink)
		return result;

	for(i = 0; i < journalBuf.num; i++)
	{
		printf("ps2fs: Log overwrite %d:%08lx\n", journalBuf.log[i].sub, journalBuf.log[i].sector);

		// Read data in from log section on disk into cache buffer
		rv = pfsMount->blockDev->transfer(pfsMount->fd, clink->u.data, 0,
			(pfsMount->log.number << pfsMount->sector_scale) + journalBuf.log[i].logSector, 2,
			IOCTL2_TMODE_READ);

		// Write from cache buffer into destination location on disk
		if(!rv)
			pfsMount->blockDev->transfer(pfsMount->fd, clink->u.data, journalBuf.log[i].sub,
				journalBuf.log[i].sector, 2, IOCTL2_TMODE_WRITE);
	}

	cacheAdd(clink);
	return journalReset(pfsMount);
}
