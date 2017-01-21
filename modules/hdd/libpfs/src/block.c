/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS block/zone related routines
*/

#include <errno.h>
#include <stdio.h>
#include <sysclib.h>

#include "pfs-opt.h"
#include "libpfs.h"

// Returns the next block descriptor inode
pfs_cache_t *pfsBlockGetNextSegment(pfs_cache_t *clink, int *result)
{
    pfsCacheFree(clink);

    if (clink->u.inode->next_segment.number)
        return pfsCacheGetData(clink->pfsMount,
                               clink->u.inode->next_segment.subpart,
                               clink->u.inode->next_segment.number << clink->pfsMount->inode_scale,
                               PFS_CACHE_FLAG_SEGI, result);

    PFS_PRINTF(PFS_DRV_NAME ": Error: There is no next segment descriptor\n");
    *result = -EINVAL;
    return NULL;
}

// Sets 'blockpos' to point to the next block segment for the inode (and moves onto the
// next block descriptor inode if necessary)
int pfsBlockSeekNextSegment(pfs_cache_t *clink, pfs_blockpos_t *blockpos)
{
    pfs_cache_t *nextSegment;
    int result = 0;

    if (blockpos->byte_offset) {
        PFS_PRINTF(PFS_DRV_NAME ": Panic: This is a bug!\n");
        return -1;
    }

    // If we're at the end of the block descriptor array for the current
    // inode, then move onto the next inode
    if (pfsFixIndex(blockpos->block_segment + 1) == 0) {
        if ((nextSegment = pfsBlockGetNextSegment(pfsCacheUsedAdd(blockpos->inode), &result)) == NULL)
            return result;
        pfsCacheFree(blockpos->inode);
        blockpos->inode = nextSegment;
        if (clink->u.inode->number_data - 1 == ++blockpos->block_segment)
            return -EINVAL;
    }

    blockpos->block_offset = 0;
    blockpos->block_segment++;
    return result;
}

u32 pfsBlockSyncPos(pfs_blockpos_t *blockpos, u64 size)
{
    u32 i;

    i = (u32)(size / blockpos->inode->pfsMount->zsize);
    blockpos->byte_offset += size % blockpos->inode->pfsMount->zsize;

    if (blockpos->byte_offset >= blockpos->inode->pfsMount->zsize) {
        blockpos->byte_offset -= blockpos->inode->pfsMount->zsize;
        i++;
    }
    return i;
}

int pfsBlockInitPos(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u64 position)
{
    blockpos->inode = pfsCacheUsedAdd(clink);
    blockpos->byte_offset = 0;

    if (clink->u.inode->size) {
        blockpos->block_segment = 1;
        blockpos->block_offset = 0;
    } else {
        blockpos->block_segment = 0;
        blockpos->block_offset = 1;
    }
    return pfsInodeSync(blockpos, position, clink->u.inode->number_data);
}
