/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS superblock (write) manipulation routines
*/

#include <errno.h>
#include <stdio.h>
#include <sysclib.h>
#include <hdd-ioctl.h>

#include "pfs-opt.h"
#include "libpfs.h"

extern int pfsBlockSize;
extern u32 pfsMetaSize;

// Formats a partition (main or sub) by filling with fragment pattern and setting the bitmap accordingly
int pfsFormatSub(pfs_block_device_t *blockDev, int fd, u32 sub, u32 reserved, u32 scale, u32 fragment)
{
	pfs_cache_t *cache;
	int i;
	u32 sector, count, size, *b;
	int result = 0;

	size = blockDev->getSize(fd, sub);
	sector = 1 << scale;
	count = pfsGetBitmapSizeSectors(scale, size);
	if (reserved>=2)
		sector+=0x2000;
	reserved += pfsGetBitmapSizeBlocks(scale, size);

	if((cache = pfsCacheAllocClean(&result)))
	{
		// fill with fragment pattern
		for (i=127; i>=0; i--)
			cache->u.bitmap[i]=fragment;

		// set as allocated the sectors up to reserved, for the first part of the bitmap
		// this will mark the area the bitmaps themselves occupy as used
		for (i=0, b=cache->u.bitmap; i<reserved; i++)
		{
			if (i && ((i & 0x1F)==0))
				b++;
			*b |= 1 << (i & 0x1F);
		}

		PFS_PRINTF(PFS_DRV_NAME": Format sub: sub = %ld, sector start = %ld, ", sub, sector);

		// set the rest of the bitmap to the fragment
		while (count-- && ((result=blockDev->transfer(fd, cache->u.bitmap, sub, sector++, 1, 1))>=0))
			for (i=127; i>=0; i--)
				cache->u.bitmap[i]=fragment;

		PFS_PRINTF("sector end = %ld\n", sector - 1);

		pfsCacheFree(cache);
	}
	return result;
}

// Formats a partition and all sub-partitions with PFS
int pfsFormat(pfs_block_device_t *blockDev, int fd, int zonesize, int fragment)
{
	int result, result2;
	pfs_cache_t *clink, *cache;
	pfs_super_block_t *sb;
	int scale;
	u32 i, mainsize, subnumber = blockDev->getSubNumber(fd);

	mainsize=blockDev->getSize(fd, 0);
	if(pfsCheckZoneSize(zonesize) == 0)
		return -EINVAL;

	scale = pfsGetScale(zonesize, 512);

	if((clink=pfsCacheAllocClean(&result)))
	{
		sb = clink->u.superblock;
		memset(sb, 0, pfsMetaSize);
		sb->magic = PFS_SUPER_MAGIC;
		sb->version = 3;
		sb->unknown1 = 0x201;
		sb->zone_size = zonesize;
		sb->num_subs = subnumber;
		sb->log.number = pfsGetBitmapSizeBlocks(scale, mainsize) + (0x2000 >> scale) + 1;
		sb->log.count = 0x20000 / zonesize ? 0x20000 / zonesize : 1;

		PFS_PRINTF(PFS_DRV_NAME": Format: log.number = %ld, log.count = %d\n", sb->log.number << scale, sb->log.count);

		sb->root.count = 1;
		sb->root.number = sb->log.number + sb->log.count;
		if((result = pfsJournalResetThis(blockDev, fd, sb->log.number<<scale)) >= 0)
		{
			if((cache = pfsCacheAllocClean(&result2)))
			{
				pfsFillSelfAndParentDentries(cache, &sb->root, &sb->root);
				result2 = blockDev->transfer(fd, cache->u.dentry, 0, (sb->root.number+1) << scale,
												1 << pfsBlockSize, 1);
				if(result2 == 0)
				{
					// setup root directory
					pfsInodeFill(cache, &sb->root, 0x11FF, 0, 0);
					cache->u.inode->data[1].subpart = 0;
					cache->u.inode->data[1].number = sb->root.number + 1;
					cache->u.inode->data[1].count = 1;
					cache->u.inode->checksum = pfsInodeCheckSum(cache->u.inode);

					result2=blockDev->transfer(fd, cache->u.inode, 0, sb->root.number << scale,
												1 << pfsBlockSize, 1);
				}

				pfsCacheFree(cache);
			}

			if((result = result2) >= 0)
			{
				for (i=0; i < subnumber+1; i++)
					if((result=pfsFormatSub(blockDev, fd, i, i ? 1 : (0x2000 >> scale) +
											sb->log.count + 3, scale, fragment))<0)
						break;

				if((result == 0) && ((result = blockDev->transfer(fd, sb, 0, PFS_SUPER_BACKUP_SECTOR, 1, 1))==0))
					result = blockDev->transfer(fd, sb, 0, PFS_SUPER_SECTOR, 1, 1);
			}
		}

		pfsCacheFree(clink);
		blockDev->flushCache(fd);
	}
	return result;
}

// Formats sub partitions which are added to the main partition after it is initially
// formatted with PFS
int pfsUpdateSuperBlock(pfs_mount_t *pfsMount, pfs_super_block_t *superblock, u32 sub)
{
	u32 scale;
	u32 i;
	u32 count;
	int rv;

	scale = pfsGetScale(superblock->zone_size, 512);
	count = superblock->num_subs + sub + 1;

	for(i = superblock->num_subs + 1; i < count; i++)
	{
		rv = pfsFormatSub(pfsMount->blockDev, pfsMount->fd, i, 1, scale, 0);
		if(rv < 0)
			return rv;
	}

	superblock->num_subs = sub;

	// Write superblock, then write backup
	rv = pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_SUPER_SECTOR, 1, PFS_IO_MODE_WRITE);
	if(!rv)
		rv = pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_SUPER_BACKUP_SECTOR, 1, PFS_IO_MODE_WRITE);

	pfsMount->blockDev->flushCache(pfsMount->fd);

	return rv;
}

int pfsMountSuperBlock(pfs_mount_t *pfsMount)
{
	int result;
	pfs_cache_t *clink;
	pfs_super_block_t *superblock;
	u32 sub;
	u32 i;


	// Get number of sub partitions attached to the main partition
	sub = pfsMount->blockDev->getSubNumber(pfsMount->fd);

	// Allocate a cache entry for the superblock
	clink = pfsCacheAllocClean(&result);
	if(!clink)
		return result;

	superblock = clink->u.superblock;

	// Read the suprerblock from the main partition
	result = pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_SUPER_SECTOR, 1, PFS_IO_MODE_READ);
	if(result) goto error;

	if((superblock->magic != PFS_SUPER_MAGIC) || (superblock->version >= PFS_VERSION))
	{
		PFS_PRINTF(PFS_DRV_NAME": Error: Invalid magic/version\n");
		pfsCacheFree(clink);
		return -EIO;
	}

	if(!pfsCheckZoneSize(superblock->zone_size))
		result = -EIO;

	if((superblock->pfsFsckStat & PFS_FSCK_STAT_WRITE_ERROR) && (pfsMount->flags & PFS_FIO_ATTR_EXECUTABLE))
		result = -EIO;

	if(sub < superblock->num_subs)
	{
		PFS_PRINTF(PFS_DRV_NAME": Error: Filesystem larger than partition\n");
		result = -EIO;
	}

	if(result) goto error;

	// If new subs have been added, update filesystem
	if(superblock->num_subs < sub)
	{
		PFS_PRINTF(PFS_DRV_NAME": New subs added, updating filesystem..\n");
		result = pfsUpdateSuperBlock(pfsMount, superblock, sub);
	}

	if(result) goto error;

	pfsMount->zsize = superblock->zone_size;
	pfsMount->sector_scale = pfsGetScale(pfsMount->zsize, 512);
	pfsMount->inode_scale = pfsGetScale(pfsMount->zsize, pfsMetaSize);
	pfsMount->num_subs = superblock->num_subs;
	memcpy(&pfsMount->root_dir, &superblock->root, sizeof(pfs_blockinfo_t));
	memcpy(&pfsMount->log, &superblock->log, sizeof(pfs_blockinfo_t));
	memcpy(&pfsMount->current_dir, &superblock->root, sizeof(pfs_blockinfo_t));
	pfsMount->total_sector = 0;
	pfsMount->uid = 0;
	pfsMount->gid = 0;

	// Do a journal restore (in case of un-clean unmount)
	pfsJournalRestore(pfsMount);

	// Calculate free space and total size
	for(i = 0; i < (pfsMount->num_subs + 1); i++)
	{
		int free;

		pfsMount->total_sector += pfsMount->blockDev->getSize(pfsMount->fd, i) >> pfsMount->sector_scale;

		free = pfsBitmapCalcFreeZones(pfsMount, i);
		pfsMount->free_zone[i] = free;
		pfsMount->zfree += free;
	}

error:
	pfsCacheFree(clink);
	return result;
}
