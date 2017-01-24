/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS low-level inode manipulation routines
*/

#include <errno.h>
#include <stdio.h>
#include <sysclib.h>

#include "pfs-opt.h"
#include "libpfs.h"

extern u32 pfsMetaSize;

///////////////////////////////////////////////////////////////////////////////
//	Function definitions

void pfsInodePrint(pfs_inode_t *inode)
{
    PFS_PRINTF(PFS_DRV_NAME ": pfsInodePrint: Checksum = 0x%lX, Magic = 0x%lX\n", inode->checksum, inode->magic);
    PFS_PRINTF(PFS_DRV_NAME ":     Mode = 0x%X, attr = 0x%X\n", inode->mode, inode->attr);
    PFS_PRINTF(PFS_DRV_NAME ":     size = 0x%08lX%08lX\n", (u32)(inode->size >> 32), (u32)(inode->size));
}

int pfsInodeCheckSum(pfs_inode_t *inode)
{
    u32 *ptr = (u32 *)inode;
    u32 sum = 0;
    int i;

    for (i = 1; i < 256; i++)
        sum += ptr[i];
    return sum;
}

pfs_cache_t *pfsInodeGetData(pfs_mount_t *pfsMount, u16 sub, u32 inode, int *result)
{
    return pfsCacheGetData(pfsMount, sub, inode << pfsMount->inode_scale,
                           PFS_CACHE_FLAG_SEGD, result);
}

void pfsInodeSetTime(pfs_cache_t *clink)
{ // set the inode time's in cache
    pfsGetTime(&clink->u.inode->mtime);
    memcpy(&clink->u.inode->ctime, &clink->u.inode->mtime, sizeof(pfs_datetime_t));
    memcpy(&clink->u.inode->atime, &clink->u.inode->mtime, sizeof(pfs_datetime_t));
    clink->flags |= PFS_CACHE_FLAG_DIRTY;
}

int pfsInodeSync(pfs_blockpos_t *blockpos, u64 size, u32 used_segments)
{
    int result = 0;
    u32 i;
    u16 count;

    for (i = pfsBlockSyncPos(blockpos, size); i;) {
        count = blockpos->inode->u.inode->data[pfsFixIndex(blockpos->block_segment)].count;

        i += blockpos->block_offset;

        if (i < count) {
            blockpos->block_offset = i;
            break;
        }

        i -= count;

        if (blockpos->block_segment + 1 == used_segments) {
            blockpos->block_offset = count;
            if (i || blockpos->byte_offset) {
                PFS_PRINTF(PFS_DRV_NAME ": panic: fp exceeds file.\n");
                return -EINVAL;
            }
        } else {
            blockpos->block_offset = 0;
            blockpos->block_segment++;
        }

        if (pfsFixIndex(blockpos->block_segment))
            continue;

        if ((blockpos->inode = pfsBlockGetNextSegment(blockpos->inode, &result)) == 0)
            break;

        i++;
    }

    return result;
}

pfs_cache_t *pfsGetDentriesChunk(pfs_blockpos_t *position, int *result)
{
    pfs_blockinfo_t *bi;
    pfs_mount_t *pfsMount = position->inode->pfsMount;

    bi = &position->inode->u.inode->data[pfsFixIndex(position->block_segment)];

    return pfsCacheGetData(pfsMount, bi->subpart,
                           ((bi->number + position->block_offset) << pfsMount->inode_scale) +
                               position->byte_offset / pfsMetaSize,
                           PFS_CACHE_FLAG_NOTHING, result);
}

pfs_cache_t *pfsGetDentriesAtPos(pfs_cache_t *clink, u64 position, int *offset, int *result)
{
    pfs_blockpos_t blockpos;
    pfs_cache_t *r;
    *result = pfsBlockInitPos(clink, &blockpos, position);
    if (*result)
        return 0;

    *offset = blockpos.byte_offset % pfsMetaSize;

    r = pfsGetDentriesChunk(&blockpos, result);
    pfsCacheFree(blockpos.inode);

    return r;
}
