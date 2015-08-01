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
# PFS block/zone related routines
*/

#include "pfs.h"

// Sets 'blockpos' to point to the next block segment for the inode (and moves onto the
// next block descriptor inode if necessary)
int blockSeekNextSegment(pfs_cache_t *clink, pfs_blockpos_t *blockpos)
{
	pfs_cache_t *nextSegment;
	int result=0;

	if (blockpos->byte_offset)
	{
		printf("ps2fs: Panic: This is a bug!\n");
		return -1;
	}

	// If we're at the end of the block descriptor array for the current
	// inode, then move onto the next inode
	if (fixIndex(blockpos->block_segment+1)==0)
	{
		if ((nextSegment=blockGetNextSegment(cacheUsedAdd(blockpos->inode), &result)) == NULL)
			return result;
		cacheAdd(blockpos->inode);
		blockpos->inode=nextSegment;
		if (clink->u.inode->number_data-1 == ++blockpos->block_segment)
			return -EINVAL;
	}

	blockpos->block_offset=0;
	blockpos->block_segment++;
	return result;
}

u32 blockSyncPos(pfs_blockpos_t *blockpos, u64 size)
{
	u32 i;

	i = size / blockpos->inode->pfsMount->zsize;
	blockpos->byte_offset += size % blockpos->inode->pfsMount->zsize;

	if (blockpos->byte_offset >= blockpos->inode->pfsMount->zsize)
	{
		blockpos->byte_offset -= blockpos->inode->pfsMount->zsize;
		i++;
	}
	return i;
}

int blockInitPos(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u64 position)
{
	blockpos->inode=cacheUsedAdd(clink);
	blockpos->byte_offset=0;

	if (clink->u.inode->size)
	{
		blockpos->block_segment=1;
		blockpos->block_offset=0;
	}else{
		blockpos->block_segment=0;
		blockpos->block_offset=1;
	}
	return inodeSync(blockpos, position, clink->u.inode->number_data);
}

// Attempt to expand the block segment 'blockpos' by 'count' blocks
int blockExpandSegment(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 count)
{
	int ret;
	pfs_blockinfo *bi;

	if(fixIndex(blockpos->block_segment)==0)
		return 0;

	bi = &blockpos->inode->u.inode->data[fixIndex(blockpos->block_segment)];

	if ((ret = bitmapAllocateAdditionalZones(clink->pfsMount, bi, count)))
	{
		bi->count+=ret;
		clink->u.inode->number_blocks+=ret;
		blockpos->inode->flags |= CACHE_FLAG_DIRTY;
		clink->flags |= CACHE_FLAG_DIRTY;
	}

	return ret;
}

// Attempts to allocate 'blocks' new blocks for an inode
int blockAllocNewSegment(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 blocks)
{
	pfs_blockinfo bi, *bi2;
	int result=0;
	pfs_mount_t *pfsMount=clink->pfsMount;
	u32 i, old_blocks = blocks;

	dprintf2("ps2fs CALL: allocNewBlockSegment(, , %ld)\n", blocks);

	if (cacheIsFull())
		return -ENOMEM;

	// create "indirect segment descriptor" if necessary
	if (fixIndex(clink->u.inode->number_data) == 0)
	{
		pfs_cache_t *clink2;

		bi2 = &blockpos->inode->u.inode->data[fixIndex(blockpos->block_segment)];
		bi.subpart=bi2->subpart;
		bi.count=1;
		bi.number=bi2->number+bi2->count;
		result=searchFreeZone(pfsMount, &bi, clink->u.inode->number_blocks);
		if (result<0)
		{
			dprintf("ps2fs: Error: Couldnt allocate zone! (1)\n");
			return result;
		}

		clink2=cacheGetData(pfsMount, bi.subpart, bi.number << pfsMount->inode_scale,
								CACHE_FLAG_SEGI | CACHE_FLAG_NOLOAD, &result);
		memset(clink2->u.inode, 0, sizeof(pfs_inode));
		clink2->u.inode->magic=PFS_SEGI_MAGIC;

		memcpy(&clink2->u.inode->inode_block, &clink->u.inode->inode_block, sizeof(pfs_blockinfo));
		memcpy(&clink2->u.inode->last_segment, &blockpos->inode->u.inode->data[0], sizeof(pfs_blockinfo));
		memcpy(&clink2->u.inode->data[0], &bi, sizeof(pfs_blockinfo));

		clink2->flags |= CACHE_FLAG_DIRTY;

		clink->u.inode->number_blocks+=bi.count;
		clink->u.inode->number_data++;

		memcpy(&clink->u.inode->last_segment, &bi, sizeof(pfs_blockinfo));

		clink->u.inode->number_segdesg++;

		clink->flags |= CACHE_FLAG_DIRTY;
		blockpos->block_segment++;
		blockpos->block_offset=0;

		memcpy(&blockpos->inode->u.inode->next_segment, &bi, sizeof(pfs_blockinfo));

		blockpos->inode->flags |= CACHE_FLAG_DIRTY;
		cacheAdd(blockpos->inode);
		blockpos->inode=clink2;
	}

	bi2 = &blockpos->inode->u.inode->data[fixIndex(blockpos->block_segment)];
	bi.subpart = bi2->subpart;
	bi.count = blocks;
	bi.number = bi2->number + bi2->count;

	result = searchFreeZone(pfsMount, &bi, clink->u.inode->number_blocks);
	if(result < 0)
	{
		dprintf("ps2fs: Error: Couldnt allocate zone! (2)\n");
		return result;
	}

	clink->u.inode->number_blocks += bi.count;
	clink->u.inode->number_data++;
	clink->flags |= CACHE_FLAG_DIRTY;
	blockpos->block_offset=0;
	blockpos->block_segment++;

	i = fixIndex(clink->u.inode->number_data-1);
	memcpy(&blockpos->inode->u.inode->data[i], &bi, sizeof(pfs_blockinfo));

	blockpos->inode->flags |= CACHE_FLAG_DIRTY;
	blocks -= bi.count;
	if (blocks)
		blocks -= blockExpandSegment(clink, blockpos, blocks);

	return old_blocks - blocks;
}

// Returns the block info for the block segment corresponding to the
// files current position.
pfs_blockinfo* blockGetCurrent(pfs_blockpos_t *blockpos)
{
	return &blockpos->inode->u.inode->data[fixIndex(blockpos->block_segment)];
}

// Returns the next block descriptor inode
pfs_cache_t *blockGetNextSegment(pfs_cache_t *clink, int *result)
{
	cacheAdd(clink);

	if (clink->u.inode->next_segment.number)
		return cacheGetData(clink->pfsMount,
		clink->u.inode->next_segment.subpart,
		clink->u.inode->next_segment.number << clink->pfsMount->inode_scale,
		CACHE_FLAG_SEGI, result);

	printf( "ps2fs: Error: There is no next segment descriptor\n");
	*result=-EINVAL;
	return NULL;
}

pfs_cache_t *blockGetLastSegmentDescriptorInode(pfs_cache_t *clink, int *result)
{
	return cacheGetData(clink->pfsMount, clink->u.inode->last_segment.subpart,
		clink->u.inode->last_segment.number<<clink->pfsMount->inode_scale,
		(clink->u.inode->last_segment.subpart==
		 clink->u.inode->inode_block.subpart)&&
		(clink->u.inode->last_segment.number==
		 clink->u.inode->inode_block.number) ? 16 : 32, result);
}
