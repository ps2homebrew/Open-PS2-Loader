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
# PFS superblock manipulation routines
*/

#include "pfs.h"

int checkZoneSize(u32 zone_size)
{
	if((zone_size & (zone_size - 1)) || (zone_size < (2 * 1024)) || (zone_size > (128 * 1024)))
	{
		printf("ps2fs: Error: Invalid zone size\n");
		return 0;
	}

	return 1;
}

// Returns the number of sectors (512 byte units) which will be used
// for bitmaps, given the zone size and partition size
u32 getBitmapSectors(int zoneScale, u32 partSize)
{
	int w, zones = partSize / (1 << zoneScale);

	w = (zones & 7);
	zones = zones / 8 + w;

	w = (zones & 511);
	return zones / 512 + w;
}

// Returns the number of blocks/zones which will be used for bitmaps
u32 getBitmapBocks(int scale, u32 mainsize)
{
	u32 a=getBitmapSectors(scale, mainsize);
	return a / (1<<scale) + ((a % (1<<scale))>0);
}

// Formats a partition (main or sub) by filling with fragment pattern and setting the bitmap accordingly
int formatSub(block_device *blockDev, int fd, u32 sub, u32 reserved, u32 scale, u32 fragment)
{
	pfs_cache_t *cache;
	int i;
	u32 sector, count, size, *b;
	int result = 0;

	size = blockDev->getSize(fd, sub);
	sector = 1 << scale;
	count = getBitmapSectors(scale, size);
	if (reserved>=2)
		sector+=0x2000;
	reserved += getBitmapBocks(scale, size);

	if((cache = cacheAllocClean(&result)))
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

		dprintf("ps2fs: Format sub: sub = %ld, sector start = %ld, ", sub, sector);

		// set the rest of the bitmap to the fragment
		while (count-- && ((result=blockDev->transfer(fd, cache->u.bitmap, sub, sector++, 1, 1))>=0))
			for (i=127; i>=0; i--)
				cache->u.bitmap[i]=fragment;

		dprintf("sector end = %ld\n", sector - 1);

		cacheAdd(cache);
	}
	return result;
}

// Formats a partition and all sub-partitions with PFS
int _format(block_device *blockDev, int fd, int zonesize, int fragment)
{
	int result, result2;
	pfs_cache_t *clink, *cache;
	pfs_super_block *sb;
	int scale;
	u32 i, mainsize, subnumber = blockDev->getSubNumber(fd);

	mainsize=blockDev->getSize(fd, 0);
	if(checkZoneSize(zonesize) == 0)
		return -EINVAL;

	scale = getScale(zonesize, 512);

	if((clink=cacheAllocClean(&result)))
	{
		sb = clink->u.superblock;
		memset(sb, 0, metaSize);
		sb->magic = PFS_SUPER_MAGIC;
		sb->version = 3;
		sb->unknown1 = 0x201;
		sb->zone_size = zonesize;
		sb->num_subs = subnumber;
		sb->log.number = getBitmapBocks(scale, mainsize) + (0x2000 >> scale) + 1;
		sb->log.count = 0x20000 / zonesize ? 0x20000 / zonesize : 1;

		dprintf("ps2fs: Format: log.number = %ld, log.count = %d\n", sb->log.number << scale, sb->log.count);

		sb->root.count = 1;
		sb->root.number = sb->log.number + sb->log.count;
		if((result = journalResetThis(blockDev, fd, sb->log.number<<scale)) >= 0)
		{
			if((cache = cacheAllocClean(&result2)))
			{
				fillSelfAndParentDentries(cache, &sb->root, &sb->root);
				result2 = blockDev->transfer(fd, cache->u.dentry, 0, (sb->root.number+1) << scale,
												1 << blockSize, 1);
				if(result2 == 0)
				{
					// setup root directory
					inodeFill(cache, &sb->root, 0x11FF, 0, 0);
					cache->u.inode->data[1].subpart = 0;
					cache->u.inode->data[1].number = sb->root.number + 1;
					cache->u.inode->data[1].count = 1;
					cache->u.inode->checksum = inodeCheckSum(cache->u.inode);

					result2=blockDev->transfer(fd, cache->u.inode, 0, sb->root.number << scale,
												1 << blockSize, 1);
				}

				cacheAdd(cache);
			}

			if((result = result2) >= 0)
			{
				for (i=0; i < subnumber+1; i++)
					if((result=formatSub(blockDev, fd, i, i ? 1 : (0x2000 >> scale) +
											sb->log.count + 3, scale, fragment))<0)
						break;

				if((result == 0) && ((result = blockDev->transfer(fd, sb, 0, PFS_SUPER_BACKUP_SECTOR, 1, 1))==0))
					result = blockDev->transfer(fd, sb, 0, PFS_SUPER_SECTOR, 1, 1);
			}
		}

		cacheAdd(clink);
		blockDev->flushCache(fd);
	}
	return result;
}

// Formats sub partitions which are added to the main partition after it is initially
// formatted with PFS
int updateSuperBlock(pfs_mount_t *pfsMount, pfs_super_block *superblock, u32 sub)
{
	u32 scale;
	u32 i;
	u32 count;
	int rv;

	scale = getScale(superblock->zone_size, 512);
	count = superblock->num_subs + sub + 1;

	for(i = superblock->num_subs + 1; i < count; i++)
	{
		rv = formatSub(pfsMount->blockDev, pfsMount->fd, i, 1, scale, 0);
		if(rv < 0)
			return rv;
	}

	superblock->num_subs = sub;

	// Write superblock, then write backup
	rv = pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_SUPER_SECTOR, 1, IOCTL2_TMODE_WRITE);
	if(!rv)
		rv = pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_SUPER_BACKUP_SECTOR, 1, IOCTL2_TMODE_WRITE);

	pfsMount->blockDev->flushCache(pfsMount->fd);

	return rv;
}

int mountSuperBlock(pfs_mount_t *pfsMount)
{
	int result;
	pfs_cache_t *clink;
	pfs_super_block *superblock;
	u32 sub;
	u32 i;


	// Get number of sub partitions attached to the main partition
	sub = pfsMount->blockDev->getSubNumber(pfsMount->fd);

	// Allocate a cache entry for the superblock
	clink = cacheAllocClean(&result);
	if(!clink)
		return result;

	superblock = clink->u.superblock;

	// Read the suprerblock from the main partition
	result = pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_SUPER_SECTOR, 1, IOCTL2_TMODE_READ);
	if(result) goto error;

	if((superblock->magic != PFS_SUPER_MAGIC) || (superblock->version >= PFS_VERSION))
	{
		printf("ps2fs: Error: Invalid magic/version\n");
		cacheAdd(clink);
		return -EIO;
	}

	if(!checkZoneSize(superblock->zone_size))
		result = -EIO;

	if((superblock->fsckStat & FSCK_STAT_WRITE_ERROR) && (pfsMount->flags & FIO_ATTR_EXECUTABLE))
		result = -EIO;

	if(sub < superblock->num_subs)
	{
		printf("ps2fs: Error: Filesystem larger than partition\n");
		result = -EIO;
	}

	if(result) goto error;

	// If new subs have been added, update filesystem
	if(superblock->num_subs < sub)
	{
		dprintf("ps2fs: New subs added, updating filesystem..\n");
		result = updateSuperBlock(pfsMount, superblock, sub);
	}

	if(result) goto error;

	pfsMount->zsize = superblock->zone_size;
	pfsMount->sector_scale = getScale(pfsMount->zsize, 512);
	pfsMount->inode_scale = getScale(pfsMount->zsize, metaSize);
	pfsMount->num_subs = superblock->num_subs;
	memcpy(&pfsMount->root_dir, &superblock->root, sizeof(pfs_blockinfo));
	memcpy(&pfsMount->log, &superblock->log, sizeof(pfs_blockinfo));
	memcpy(&pfsMount->current_dir, &superblock->root, sizeof(pfs_blockinfo));
	pfsMount->total_sector = 0;
	pfsMount->uid = 0;
	pfsMount->gid = 0;

	// Do a journal restore (in case of un-clean unmount)
	journalResetore(pfsMount);

	// Calculate free space and total size
	for(i = 0; i < (pfsMount->num_subs + 1); i++)
	{
		int free;

		pfsMount->total_sector += pfsMount->blockDev->getSize(pfsMount->fd, i) >> pfsMount->sector_scale;

		free = calcFreeZones(pfsMount, i);
		pfsMount->free_zone[i] = free;
		pfsMount->zfree += free;
	}

error:
	cacheAdd(clink);
	return result;
}

