/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS block/zone (write) related routines
*/

#include <errno.h>
#include <stdio.h>
#include <sysclib.h>

#include "pfs-opt.h"
#include "libpfs.h"

// Attempt to expand the block segment 'blockpos' by 'count' blocks
int pfsBlockExpandSegment(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 count)
{
	int ret;
	pfs_blockinfo_t *bi;

	if(pfsFixIndex(blockpos->block_segment)==0)
		return 0;

	bi = &blockpos->inode->u.inode->data[pfsFixIndex(blockpos->block_segment)];

	if ((ret = pfsBitmapAllocateAdditionalZones(clink->pfsMount, bi, count)))
	{
		bi->count+=ret;
		clink->u.inode->number_blocks+=ret;
		blockpos->inode->flags |= PFS_CACHE_FLAG_DIRTY;
		clink->flags |= PFS_CACHE_FLAG_DIRTY;
	}

	return ret;
}

// Attempts to allocate 'blocks' new blocks for an inode
int pfsBlockAllocNewSegment(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 blocks)
{
	pfs_blockinfo_t bi, *bi2;
	int result=0;
	pfs_mount_t *pfsMount=clink->pfsMount;
	u32 i, old_blocks = blocks;

	if (pfsCacheIsFull())
		return -ENOMEM;

	// create "indirect segment descriptor" if necessary
	if (pfsFixIndex(clink->u.inode->number_data) == 0)
	{
		pfs_cache_t *clink2;

		bi2 = &blockpos->inode->u.inode->data[pfsFixIndex(blockpos->block_segment)];
		bi.subpart=bi2->subpart;
		bi.count=1;
		bi.number=bi2->number+bi2->count;
		result=pfsBitmapSearchFreeZone(pfsMount, &bi, clink->u.inode->number_blocks);
		if (result<0)
		{
			PFS_PRINTF(PFS_DRV_NAME": Error: Couldnt allocate zone! (1)\n");
			return result;
		}

		clink2=pfsCacheGetData(pfsMount, bi.subpart, bi.number << pfsMount->inode_scale,
								PFS_CACHE_FLAG_SEGI | PFS_CACHE_FLAG_NOLOAD, &result);
		memset(clink2->u.inode, 0, sizeof(pfs_inode_t));
		clink2->u.inode->magic=PFS_SEGI_MAGIC;

		memcpy(&clink2->u.inode->inode_block, &clink->u.inode->inode_block, sizeof(pfs_blockinfo_t));
		memcpy(&clink2->u.inode->last_segment, blockpos->inode->u.inode->data, sizeof(pfs_blockinfo_t));
		memcpy(clink2->u.inode->data, &bi, sizeof(pfs_blockinfo_t));

		clink2->flags |= PFS_CACHE_FLAG_DIRTY;

		clink->u.inode->number_blocks+=bi.count;
		clink->u.inode->number_data++;

		memcpy(&clink->u.inode->last_segment, &bi, sizeof(pfs_blockinfo_t));

		clink->u.inode->number_segdesg++;

		clink->flags |= PFS_CACHE_FLAG_DIRTY;
		blockpos->block_segment++;
		blockpos->block_offset=0;

		memcpy(&blockpos->inode->u.inode->next_segment, &bi, sizeof(pfs_blockinfo_t));

		blockpos->inode->flags |= PFS_CACHE_FLAG_DIRTY;
		pfsCacheFree(blockpos->inode);
		blockpos->inode=clink2;
	}

	bi2 = &blockpos->inode->u.inode->data[pfsFixIndex(blockpos->block_segment)];
	bi.subpart = bi2->subpart;
	bi.count = blocks;
	bi.number = bi2->number + bi2->count;

	result = pfsBitmapSearchFreeZone(pfsMount, &bi, clink->u.inode->number_blocks);
	if(result < 0)
	{
		PFS_PRINTF(PFS_DRV_NAME": Error: Couldnt allocate zone! (2)\n");
		return result;
	}

	clink->u.inode->number_blocks += bi.count;
	clink->u.inode->number_data++;
	clink->flags |= PFS_CACHE_FLAG_DIRTY;
	blockpos->block_offset=0;
	blockpos->block_segment++;

	i = pfsFixIndex(clink->u.inode->number_data-1);
	memcpy(&blockpos->inode->u.inode->data[i], &bi, sizeof(pfs_blockinfo_t));

	blockpos->inode->flags |= PFS_CACHE_FLAG_DIRTY;
	blocks -= bi.count;
	if (blocks)
		blocks -= pfsBlockExpandSegment(clink, blockpos, blocks);

	return old_blocks - blocks;
}

// Returns the block info for the block segment corresponding to the
// files current position.
pfs_blockinfo_t* pfsBlockGetCurrent(pfs_blockpos_t *blockpos)
{
	return &blockpos->inode->u.inode->data[pfsFixIndex(blockpos->block_segment)];
}

pfs_cache_t *pfsBlockGetLastSegmentDescriptorInode(pfs_cache_t *clink, int *result)
{
	return pfsCacheGetData(clink->pfsMount, clink->u.inode->last_segment.subpart,
		clink->u.inode->last_segment.number<<clink->pfsMount->inode_scale,
		(clink->u.inode->last_segment.subpart==
		 clink->u.inode->inode_block.subpart)&&
		(clink->u.inode->last_segment.number==
		 clink->u.inode->inode_block.number) ? 16 : 32, result);
}
