/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS bitmap manipulation routines
*/

#include <errno.h>
#include <stdio.h>

#include "pfs-opt.h"
#include "libpfs.h"

extern u32 pfsMetaSize;
extern int pfsBlockSize;

u32 pfsBitsPerBitmapChunk = 8192; // number of bitmap bits in each bitmap data chunk (1024 bytes)

void pfsBitmapSetupInfo(pfs_mount_t *pfsMount, pfs_bitmapInfo_t *info, u32 subpart, u32 number)
{
    u32 size;

    size = pfsMount->blockDev->getSize(pfsMount->fd, subpart) >> pfsMount->sector_scale;

    info->chunk = number / pfsBitsPerBitmapChunk;
    info->bit = number & 31;
    info->index = (number % pfsBitsPerBitmapChunk) / 32;
    info->partitionChunks = size / pfsBitsPerBitmapChunk;
    info->partitionRemainder = size % pfsBitsPerBitmapChunk;
}

// Allocates or frees (depending on operation) the bitmap area starting at chunk/index/bit, of size count
void pfsBitmapAllocFree(pfs_cache_t *clink, u32 operation, u32 subpart, u32 chunk, u32 index, u32 _bit, u32 count)
{
    int result;
    u32 sector, bit;

    while (clink) {
        for (; index < (pfsMetaSize / 4) && count; index++, _bit = 0) {
            for (bit = _bit; bit < 32 && count; bit++, count--) {
                if (operation == PFS_BITMAP_ALLOC) {
                    if (clink->u.bitmap[index] & (1 << bit))
                        PFS_PRINTF(PFS_DRV_NAME ": Error: Tried to allocate used block!\n");

                    clink->u.bitmap[index] |= (1 << bit);
                } else {
                    if ((clink->u.bitmap[index] & (1 << bit)) == 0)
                        PFS_PRINTF(PFS_DRV_NAME ": Error: Tried to free unused block!\n");

                    clink->u.bitmap[index] &= ~(1 << bit);
                }
            }
        }

        index = 0;
        clink->flags |= PFS_CACHE_FLAG_DIRTY;
        pfsCacheFree(clink);

        if (count == 0)
            break;

        chunk++;

        sector = (1 << clink->pfsMount->inode_scale) + chunk;
        if (subpart == 0)
            sector += 0x2000 >> pfsBlockSize;

        clink = pfsCacheGetData(clink->pfsMount, subpart, sector, PFS_CACHE_FLAG_BITMAP, &result);
    }
}

// Attempts to allocate 'count' more continuous zones for 'bi'
int pfsBitmapAllocateAdditionalZones(pfs_mount_t *pfsMount, pfs_blockinfo_t *bi, u32 count)
{
    int result;
    pfs_bitmapInfo_t info;
    pfs_cache_t *c;
    int res = 0;

    pfsBitmapSetupInfo(pfsMount, &info, bi->subpart, bi->number + bi->count);

    // Make sure we're not trying to allocate more than is possible
    if (65535 - bi->count < count)
        count = 65535 - bi->count;

    // Loop over each bitmap chunk (each is 1024 bytes in size) until either we have allocated
    // the requested amount of blocks, or until we have run out of space on the current partition
    while ((((info.partitionRemainder == 0) && (info.chunk < info.partitionChunks)) ||
            ((info.partitionRemainder != 0) && (info.chunk < info.partitionChunks + 1))) &&
           count) {
        u32 sector = (1 << pfsMount->inode_scale) + info.chunk;

        // if main partition, add offset (in units of blocks)
        if (bi->subpart == 0)
            sector += 0x2000 >> pfsBlockSize;

        // Read the bitmap chunk from the hdd
        c = pfsCacheGetData(pfsMount, bi->subpart, sector, PFS_CACHE_FLAG_BITMAP, &result);
        if (c == 0)
            break;

        // Loop over each 32-bit word in the current bitmap chunk until
        // we find a used zone or we've allocated all the zones we need
        while ((info.index < (info.chunk == info.partitionChunks ? info.partitionRemainder / 32 : pfsMetaSize / 4)) && count) {
            // Loop over each of the 32 bits in the current word from the current bitmap chunk,
            // trying to allocate the requested number of zones
            for (; (info.bit < 32) && count; count--) {
                // We only want to allocate a continuous area, so if we come
                // accross a used zone bail
                if (c->u.bitmap[info.index] & (1 << info.bit)) {
                    pfsCacheFree(c);
                    goto exit;
                }

                // If the current bit in the bitmap is marked as free, mark it was used
                res++;
                c->u.bitmap[info.index] |= 1 << info.bit;
                info.bit++;
                c->flags |= PFS_CACHE_FLAG_DIRTY;
            }

            info.index++;
            info.bit = 0;
        }
        pfsCacheFree(c);
        info.index = 0;
        info.chunk++;
    }
exit:

    // Adjust global free zone counts
    pfsMount->free_zone[bi->subpart] -= res;
    pfsMount->zfree -= res;
    return res;
}

// Searches for 'amount' free zones, starting from the position specified in 'bi'.
// Returns 1 if allocation was successful, otherwise 0.
int pfsBitmapAllocZones(pfs_mount_t *pfsMount, pfs_blockinfo_t *bi, u32 amount)
{
    pfs_bitmapInfo_t info;
    int result;
    u32 startBit = 0, startPos = 0, startChunk = 0, count = 0;
    u32 sector;
    pfs_cache_t *bitmap;
    u32 *bitmapEnd, *bitmapWord;
    u32 i;

    pfsBitmapSetupInfo(pfsMount, &info, bi->subpart, bi->number);

    for (; ((info.partitionRemainder == 0) && (info.chunk < info.partitionChunks)) ||
           ((info.partitionRemainder != 0) && (info.chunk < info.partitionChunks + 1));
         info.chunk++) {

        sector = info.chunk + (1 << pfsMount->inode_scale);
        if (bi->subpart == 0)
            sector += 0x2000 >> pfsBlockSize;

        // read in the bitmap chunk
        bitmap = pfsCacheGetData(pfsMount, bi->subpart, sector, PFS_CACHE_FLAG_BITMAP, &result);
        if (bitmap == 0)
            return 0;

        bitmapEnd = bitmap->u.bitmap +
                    (info.chunk == info.partitionChunks ? info.partitionRemainder / 32 : pfsMetaSize / 4);

        for (bitmapWord = bitmap->u.bitmap + info.index; bitmapWord < bitmapEnd; info.bit = 0, bitmapWord++) {
            for (i = info.bit; i < 32; i++) {
                // if this bit is marked as free..
                if (((*bitmapWord >> i) & 1) == 0) {
                    if (count == 0) {
                        startBit = i;
                        startChunk = info.chunk;
                        startPos = bitmapWord - bitmap->u.bitmap;
                    }
                    if (++count == amount) {
                        bi->number = (startPos * 32) + (startChunk * pfsBitsPerBitmapChunk) + startBit;
                        if (count < bi->count)
                            bi->count = count;

                        if (bitmap->sector != (startChunk + (1 << pfsMount->inode_scale))) {
                            pfsCacheFree(bitmap);
                            sector = (1 << pfsMount->inode_scale) + startChunk;
                            if (bi->subpart == 0)
                                sector += 0x2000 >> pfsBlockSize;

                            bitmap = pfsCacheGetData(pfsMount, bi->subpart, sector, PFS_CACHE_FLAG_BITMAP, &result);
                        }

                        pfsBitmapAllocFree(bitmap, PFS_BITMAP_ALLOC, bi->subpart, startChunk, startPos, startBit, bi->count);
                        return 1;
                    }
                } else
                    count = 0;
            }
        }
        pfsCacheFree(bitmap);
        info.index = 0;
    }
    return 0;
}

// Searches for 'max_count' free zones over all the partitions, and
// allocates them. Returns 0 on success, -ENOSPC if the zones could
// not be allocated.
int pfsBitmapSearchFreeZone(pfs_mount_t *pfsMount, pfs_blockinfo_t *bi, u32 max_count)
{
    u32 num, count, n;

    num = pfsMount->num_subs + 1;

    if (bi->subpart > pfsMount->num_subs)
        bi->subpart = 0;
    if (bi->number)
        num = pfsMount->num_subs + 2;

    count = max_count < 33 ? max_count : 32;       //min(max_count, 32)
    count = count < bi->count ? bi->count : count; //max(count, bi->count)
                                                   // => count = bound(bi->count, 32);
    for (; num >= 0; num--) {
        for (n = count; n; n /= 2) {
            if ((pfsMount->free_zone[bi->subpart] >= n) &&
                pfsBitmapAllocZones(pfsMount, bi, n)) {
                pfsMount->free_zone[bi->subpart] -= bi->count;
                pfsMount->zfree -= bi->count;
                return 0; // the good exit ;)
            }
        }

        bi->number = 0;
        bi->subpart++;

        if (bi->subpart == pfsMount->num_subs + 1)
            bi->subpart = 0;
    }
    return -ENOSPC;
}

// De-allocates the block segment 'bi' in the bitmaps
void pfsBitmapFreeBlockSegment(pfs_mount_t *pfsMount, pfs_blockinfo_t *bi)
{
    pfs_bitmapInfo_t info;
    pfs_cache_t *clink;
    u32 sector;
    int rv;

    pfsBitmapSetupInfo(pfsMount, &info, bi->subpart, bi->number);

    sector = (1 << pfsMount->inode_scale) + info.chunk;
    if (bi->subpart == 0)
        sector += 0x2000 >> pfsBlockSize;

    if ((clink = pfsCacheGetData(pfsMount, (u16)bi->subpart, sector, PFS_CACHE_FLAG_BITMAP, &rv)) != NULL) {
        pfsBitmapAllocFree(clink, PFS_BITMAP_FREE, bi->subpart, info.chunk, info.index, info.bit, bi->count);
        pfsMount->free_zone[(u16)bi->subpart] += bi->count;
        pfsMount->zfree += bi->count;
    }
}

// Returns the number of free zones for the partition 'sub'
int pfsBitmapCalcFreeZones(pfs_mount_t *pfsMount, int sub)
{
    // "Free zone" map. Used to get number of free zone in bitmap, 4-bits at a time
    u32 pfsFreeZoneBitmap[16] = {4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0};
    int result;
    pfs_bitmapInfo_t info;
    pfs_cache_t *clink;
    u32 i, bitmapSize, zoneFree = 0, sector;

    pfsBitmapSetupInfo(pfsMount, &info, sub, 0);

    while (((info.partitionRemainder != 0) && (info.chunk < info.partitionChunks + 1)) ||
           ((info.partitionRemainder == 0) && (info.chunk < info.partitionChunks))) {
        bitmapSize = info.chunk == info.partitionChunks ? info.partitionRemainder >> 3 : pfsMetaSize;

        sector = (1 << pfsMount->inode_scale) + info.chunk;
        if (sub == 0)
            sector += 0x2000 >> pfsBlockSize;

        if ((clink = pfsCacheGetData(pfsMount, sub, sector, PFS_CACHE_FLAG_BITMAP, &result))) {
            for (i = 0; i < bitmapSize; i++) {
                zoneFree += pfsFreeZoneBitmap[((u8 *)clink->u.bitmap)[i] & 0xF] + pfsFreeZoneBitmap[((u8 *)clink->u.bitmap)[i] >> 4];
            }

            pfsCacheFree(clink);
        }
        info.chunk++;
    }

    return zoneFree;
}

// Debugging function, prints bitmap information
void pfsBitmapShow(pfs_mount_t *pfsMount)
{
    int result;
    pfs_bitmapInfo_t info;
    u32 pn, bitcnt;

    for (pn = 0; pn < pfsMount->num_subs + 1; pn++) {
        bitcnt = pfsBitsPerBitmapChunk;
        pfsBitmapSetupInfo(pfsMount, &info, pn, 0);

        while (((info.partitionRemainder != 0) && (info.chunk < info.partitionChunks + 1)) ||
               ((info.partitionRemainder == 0) && (info.chunk < info.partitionChunks))) {
            pfs_cache_t *clink;
            u32 sector = (1 << pfsMount->inode_scale) + info.chunk;
            u32 i;

            if (pn == 0)
                sector += 0x2000 >> pfsBlockSize;
            clink = pfsCacheGetData(pfsMount, pn, sector, PFS_CACHE_FLAG_BITMAP, &result);

            if (info.chunk == info.partitionChunks)
                bitcnt = info.partitionRemainder;

            PFS_PRINTF(PFS_DRV_NAME ": Zone show: pn %ld, bn %ld, bitcnt %ld\n", pn, info.chunk, bitcnt);

            for (i = 0; (i < (1 << pfsBlockSize)) && ((i * 512) < (bitcnt / 8)); i++)
                pfsPrintBitmap(clink->u.bitmap + 128 * i);

            pfsCacheFree(clink);
            info.chunk++;
        }
    }
}

// Free's all blocks allocated to an inode
void pfsBitmapFreeInodeBlocks(pfs_cache_t *clink)
{
    pfs_mount_t *pfsMount = clink->pfsMount;
    u32 i;

    pfsCacheUsedAdd(clink);
    for (i = 0; i < clink->u.inode->number_data; i++) {
        u32 index = pfsFixIndex(i);
        pfs_blockinfo_t *bi = &clink->u.inode->data[index];
        int err;

        if (i != 0) {
            if (index == 0)
                if ((clink = pfsBlockGetNextSegment(clink, &err)) == NULL)
                    return;
        }

        pfsBitmapFreeBlockSegment(pfsMount, bi);
        pfsCacheMarkClean(pfsMount, (u16)bi->subpart, bi->number << pfsMount->inode_scale,
                          (bi->number + bi->count) << pfsMount->inode_scale);
    }
    pfsCacheFree(clink);
}
