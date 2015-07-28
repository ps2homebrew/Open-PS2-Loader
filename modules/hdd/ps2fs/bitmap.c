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
# PFS bitmap manipulation routines
*/

#include "pfs.h"

u32 bitsPerBitmapChunk = 8192; // number of bitmap bits in each bitmap data chunk (1024 bytes)

// "Free zone" map. Used to get number of free zone in bitmap, 4-bits at a time
u32 free_bitmap[16]={4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0};

void bitmapSetupInfo(pfs_mount_t *pfsMount, t_bitmapInfo *info, u32 subpart, u32 number)
{
	u32 size;

	size = pfsMount->blockDev->getSize(pfsMount->fd, subpart) >> pfsMount->sector_scale;

	info->chunk = number / bitsPerBitmapChunk;
	info->bit = number & 31;
	info->index = (number % bitsPerBitmapChunk) / 32;
	info->partitionChunks = size / bitsPerBitmapChunk;
	info->partitionRemainder = size % bitsPerBitmapChunk;
}

// Allocates or frees (depending on operation) the bitmap area starting at chunk/index/bit, of size count
void bitmapAllocFree(pfs_cache_t *clink, u32 operation, u32 subpart, u32 chunk, u32 index, u32 _bit, u32 count)
{
	int result;
	u32 sector, bit;

	while (clink)
	{
		for ( ; index < (metaSize / 4) && count; index++, _bit = 0)
		{
			for (bit = _bit; bit < 32 && count; bit++, count--)
			{
				if(operation == BITMAP_ALLOC)
				{
					if (clink->u.bitmap[index] & (1 << bit))
						printf("ps2fs: Error: Tried to allocate used block!\n");

					clink->u.bitmap[index] |= (1 << bit);
				}
				else
				{
					if ((clink->u.bitmap[index] & (1 << bit))==0)
						printf("ps2fs: Error: Tried to free unused block!\n");

					clink->u.bitmap[index] &= ~(1 << bit);
				}
			}
		}

		index = 0;
		clink->flags |= CACHE_FLAG_DIRTY;
		cacheAdd(clink);

		if (count==0)
			break;

		chunk++;

		sector = (1 << clink->pfsMount->inode_scale) + chunk;
		if(subpart==0)
			sector += 0x2000 >> blockSize;

		clink = cacheGetData(clink->pfsMount, subpart, sector, CACHE_FLAG_BITMAP, &result);
	}
}

// Attempts to allocate 'count' more continuous zones for 'bi'
int bitmapAllocateAdditionalZones(pfs_mount_t *pfsMount, pfs_blockinfo *bi, u32 count)
{
	int result;
	t_bitmapInfo info;
	pfs_cache_t *c;
	int res=0;

	bitmapSetupInfo(pfsMount, &info, bi->subpart, bi->number+bi->count);

	// Make sure we're not trying to allocate more than is possible
	if (65535-bi->count < count)
		count=65535-bi->count;

	// Loop over each bitmap chunk (each is 1024 bytes in size) until either we have allocated
	// the requested amount of blocks, or until we have run out of space on the current partition
	while ((((info.partitionRemainder==0) && (info.chunk < info.partitionChunks  )) ||
	        ((info.partitionRemainder!=0) && (info.chunk < info.partitionChunks+1))) && count)
	{
		u32 sector=(1<<pfsMount->inode_scale) + info.chunk;

		// if main partition, add offset (in units of blocks)
		if (bi->subpart==0)
			sector += 0x2000 >> blockSize;

		// Read the bitmap chunk from the hdd
		c=cacheGetData(pfsMount, bi->subpart, sector, CACHE_FLAG_BITMAP, &result);
		if (c==0)break;

		// Loop over each 32-bit word in the current bitmap chunk until
		// we find a used zone or we've allocated all the zones we need
		while ((info.index < (info.chunk==info.partitionChunks ? info.partitionRemainder / 32 : metaSize / 4)) && count)
		{
			// Loop over each of the 32 bits in the current word from the current bitmap chunk,
			// trying to allocate the requested number of zones
			for ( ; (info.bit < 32) && count; count--)
			{
				// We only want to allocate a continuous area, so if we come
				// accross a used zone bail
				if (c->u.bitmap[info.index] & (1<<info.bit))
				{
					cacheAdd(c);
					goto exit;
				}

				// If the current bit in the bitmap is marked as free, mark it was used
				res++;
				c->u.bitmap[info.index] |= 1<<info.bit;
				info.bit++;
				c->flags |= CACHE_FLAG_DIRTY;
			}

			info.index++;
			info.bit=0;
		}
		cacheAdd(c);
		info.index=0;
		info.chunk++;
	}
exit:

	// Adjust global free zone counts
	pfsMount->free_zone[bi->subpart]-=res;
	pfsMount->zfree-=res;
	return res;
}

// Searches for 'amount' free zones, starting from the position specified in 'bi'.
// Returns 1 if allocation was successful, otherwise 0.
int bitmapAllocZones(pfs_mount_t *pfsMount, pfs_blockinfo *bi, u32 amount)
{
	t_bitmapInfo info;
	int result;
	u32 startBit = 0, startPos = 0, startChunk = 0, count = 0;
	u32 sector;
	pfs_cache_t *bitmap;
	u32 *bitmapEnd, *bitmapWord;
	u32 i;

	bitmapSetupInfo(pfsMount, &info, bi->subpart, bi->number);

	for ( ; ((info.partitionRemainder==0) && (info.chunk < info.partitionChunks))||
	        ((info.partitionRemainder!=0) && (info.chunk < info.partitionChunks+1)); info.chunk++){

		sector = info.chunk + (1 << pfsMount->inode_scale);
		if(bi->subpart==0)
			sector += 0x2000 >> blockSize;

		// read in the bitmap chunk
		bitmap = cacheGetData(pfsMount, bi->subpart, sector, CACHE_FLAG_BITMAP, &result);
		if(bitmap==0)
			return 0;

		bitmapEnd=bitmap->u.bitmap +
			(info.chunk == info.partitionChunks ? info.partitionRemainder / 32 : metaSize / 4);

		for (bitmapWord=bitmap->u.bitmap + info.index; bitmapWord < bitmapEnd; info.bit=0, bitmapWord++)
		{
			for (i=info.bit; i < 32; i++)
			{
				// if this bit is marked as free..
				if (((*bitmapWord >> i) & 1)==0)
				{
					if (count==0)
					{
						startBit = i;
						startChunk = info.chunk;
						startPos = bitmapWord - bitmap->u.bitmap;
					}
					if (++count == amount)
					{
						bi->number = (startPos * 32) + (startChunk * bitsPerBitmapChunk) + startBit;
						if (count < bi->count)
							bi->count=count;

						if (bitmap->sector != (startChunk + (1 << pfsMount->inode_scale)))
						{
							cacheAdd(bitmap);
							sector = (1 << pfsMount->inode_scale) + startChunk;
							if(bi->subpart==0)
								sector += 0x2000 >> blockSize;

							bitmap = cacheGetData(pfsMount, bi->subpart, sector, CACHE_FLAG_BITMAP, &result);
						}

						bitmapAllocFree(bitmap, BITMAP_ALLOC, bi->subpart, startChunk, startPos, startBit, bi->count);
						return 1;
					}
				}
				else
					count=0;
			}
		}
		cacheAdd(bitmap);
		info.index=0;
	}
	return 0;
}

// Searches for 'max_count' free zones over all the partitions, and
// allocates them. Returns 0 on success, -ENOSPC if the zones could
// not be allocated.
int searchFreeZone(pfs_mount_t *pfsMount, pfs_blockinfo *bi, u32 max_count)
{
	u32 num, count, n;

	num = pfsMount->num_subs + 1;

	if (bi->subpart > pfsMount->num_subs)
		bi->subpart = 0;
	if (bi->number)
		num = pfsMount->num_subs + 2;

	count = max_count < 33 ? max_count : 32;		//min(max_count, 32)
	count = count < bi->count ? bi->count : count;	//max(count, bi->count)
													// => count = bound(bi->count, 32);
	for(; num >= 0; num--)
	{
		for (n = count; n; n /= 2)
		{
			if ((pfsMount->free_zone[bi->subpart] >= n) &&
			    bitmapAllocZones(pfsMount, bi, n))
			{
				pfsMount->free_zone[bi->subpart] -= bi->count;
				pfsMount->zfree -= bi->count;
				return 0;	// the good exit ;)
			}
		}

		bi->number=0;
		bi->subpart++;

		if(bi->subpart == pfsMount->num_subs + 1)
			bi->subpart=0;
	}
	return -ENOSPC;
}

// De-allocates the block segment 'bi' in the bitmaps
void bitmapFreeBlockSegment(pfs_mount_t *pfsMount, pfs_blockinfo *bi)
{
	t_bitmapInfo info;
	pfs_cache_t *clink;
	u32 sector;
	int rv;

	bitmapSetupInfo(pfsMount, &info, bi->subpart, bi->number);

	sector = (1 << pfsMount->inode_scale) + info.chunk;
	if(bi->subpart==0)
		sector += 0x2000 >> blockSize;

	if((clink=cacheGetData(pfsMount, (u16)bi->subpart, sector, CACHE_FLAG_BITMAP, &rv)) != NULL)
	{
		bitmapAllocFree(clink, BITMAP_FREE, bi->subpart, info.chunk, info.index, info.bit, bi->count);
		pfsMount->free_zone[(u16)bi->subpart]+=bi->count;
		pfsMount->zfree+=bi->count;
	}
}

// Returns the number of free zones for the partition 'sub'
int calcFreeZones(pfs_mount_t *pfsMount, int sub)
{
	int result;
	t_bitmapInfo info;
	pfs_cache_t *clink;
	u32 i, bitmapSize, zoneFree=0, sector;

	bitmapSetupInfo(pfsMount, &info, sub, 0);

	while (((info.partitionRemainder!=0) && (info.chunk<info.partitionChunks+1)) ||
	       ((info.partitionRemainder==0) && (info.chunk<info.partitionChunks)))
	{
		bitmapSize = info.chunk==info.partitionChunks ? info.partitionRemainder>>3 : metaSize;

		sector = (1<<pfsMount->inode_scale) + info.chunk;
		if (sub==0)
			sector +=0x2000>>blockSize;

		if ((clink=cacheGetData(pfsMount, sub, sector, CACHE_FLAG_BITMAP, &result)))
		{
			for (i=0; i<bitmapSize; i++)
			{
				zoneFree+=free_bitmap[((u8*)clink->u.bitmap)[i] & 0xF]
				   +free_bitmap[((u8*)clink->u.bitmap)[i] >> 4];
			}

			cacheAdd(clink);
		}
		info.chunk++;
	}

	return zoneFree;
}

// Debugging function, prints bitmap information
void bitmapShow(pfs_mount_t *pfsMount)
{
	int result;
	t_bitmapInfo info;
	u32 pn, bitcnt;

	for (pn=0; pn < pfsMount->num_subs+1; pn++)
	{
		bitcnt=bitsPerBitmapChunk;
		bitmapSetupInfo(pfsMount, &info, pn, 0);

		while (((info.partitionRemainder!=0) && (info.chunk<info.partitionChunks+1)) ||
		       ((info.partitionRemainder==0) && (info.chunk<info.partitionChunks)))
		{
			pfs_cache_t *clink;
			u32 sector = (1<<pfsMount->inode_scale) + info.chunk;
			u32 i;

			if(pn==0)
				sector += 0x2000 >> blockSize;
			clink=cacheGetData(pfsMount, pn, sector, CACHE_FLAG_BITMAP, &result);

			if (info.chunk == info.partitionChunks)
				bitcnt=info.partitionRemainder;

			printf("ps2fs: Zone show: pn %ld, bn %ld, bitcnt %ld\n", pn, info.chunk, bitcnt);

			for(i=0; (i < (1<<blockSize)) && ((i * 512) < (bitcnt / 8)); i++)
				printBitmap(clink->u.bitmap+128*i);

			cacheAdd(clink);
			info.chunk++;
		}
	}
}

// Free's all blocks allocated to an inode
void bitmapFreeInodeBlocks(pfs_cache_t *clink)
{
	pfs_mount_t *pfsMount=clink->pfsMount;
	u32 i;

	cacheUsedAdd(clink);
	for(i=0;i < clink->u.inode->number_data; i++)
	{
		u32 index = fixIndex(i);
		pfs_blockinfo *bi=&clink->u.inode->data[index];
		int err;

		if(i!=0) {
			if(index==0)
				if((clink = blockGetNextSegment(clink, &err))==NULL)
					return;
		}

		bitmapFreeBlockSegment(pfsMount, bi);
		cacheMarkClean(pfsMount, (u16)bi->subpart,  bi->number << pfsMount->inode_scale,
			(bi->number + bi->count) << pfsMount->inode_scale);
	}
	cacheAdd(clink);
}
