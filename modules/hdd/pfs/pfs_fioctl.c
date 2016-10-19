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
# PFS ioctl and devctl related routines
*/

#include <stdio.h>
#include <sysclib.h>
#include <errno.h>
#include <iomanX.h>
#include <thsemap.h>
#include <hdd-ioctl.h>
#include "opl-hdd-ioctl.h"

#include "pfs-opt.h"
#include "libpfs.h"
#include "pfs.h"
#include "pfs_fio.h"
#include "pfs_fioctl.h"

extern pfs_config_t pfsConfig;
extern int pfsFioSema;
extern pfs_file_slot_t *pfsFileSlots;

///////////////////////////////////////////////////////////////////////////////
//	Function declarations

static int devctlFsckStat(pfs_mount_t *pfsMount, int mode);

static int ioctl2Attr(pfs_cache_t *clink, int cmd, void *arg, void *outbuf, u32 *offset);
static pfs_aentry_t *getAentry(pfs_cache_t *clink, char *key, char *value, int mode);
static int ioctl2AttrAdd(pfs_cache_t *clink, pfs_ioctl2attr_t *attr);
static int ioctl2AttrDelete(pfs_cache_t *clink, void *arg);
static int ioctl2AttrLoopUp(pfs_cache_t *clink, char *key, char *value);
static int ioctl2AttrRead(pfs_cache_t *clink, pfs_ioctl2attr_t *attr, u32 *unkbuf);

int pfsFioIoctl(iop_file_t *f, unsigned long arg, void *param)
{
    return -1;
}

int pfsFioDevctl(iop_file_t *f, const char *name, int cmd, void *arg, size_t arglen, void *buf, size_t buflen)
{
    pfs_mount_t *pfsMount;
    int rv = 0;

    if (!(pfsMount = pfsFioGetMountedUnit(f->unit)))
        return -ENODEV;

    switch (cmd) {
        case PFS_DEVCTL_GET_ZONE_SIZE:
            rv = pfsMount->zsize;
            break;

        case PFS_DEVCTL_GET_ZONE_FREE:
            rv = pfsMount->zfree;
            break;

        case PFS_DEVCTL_CLOSE_ALL:
            pfsFioDevctlCloseAll();
            pfsHddFlushCache(pfsMount->fd);
            break;

        case PFS_DEVCTL_CLEAR_STAT:
            rv = devctlFsckStat(pfsMount, PFS_MODE_REMOVE_FLAG);
            break;

        case PFS_DEVCTL_GET_STAT:
            rv = devctlFsckStat(pfsMount, PFS_MODE_CHECK_FLAG);
            break;

        case PFS_DEVCTL_SET_UID:
            pfsMount->uid = *(u16 *)(arg);
            break;

#if PFS_DEVCTL_SET_UID != PFS_DEVCTL_SET_GID
        case PFS_DEVCTL_SET_GID:
            pfsMount->gid = *(u16 *)(arg);
            break;
#endif
        case PFS_DEVCTL_SHOW_BITMAP:
            pfsBitmapShow(pfsMount);
            break;

        default:
            rv = -EINVAL;
            break;
    }
    SignalSema(pfsFioSema);

    return rv;
}

int pfsFioIoctl2(iop_file_t *f, int cmd, void *arg, size_t arglen, void *buf, size_t buflen)
{
    int rv;
    pfs_file_slot_t *fileSlot = (pfs_file_slot_t *)f->privdata;
    pfs_mount_t *pfsMount;

    if (f->mode & O_DIROPEN)
        if (cmd == PFS_IOCTL2_ATTR_READ)
            return -EISDIR;

    if (!(f->mode & O_WRONLY)) {
        if (cmd != PFS_IOCTL2_ATTR_LOOKUP)
            if (cmd != PFS_IOCTL2_ATTR_READ)
                return -EACCES;
    }
    if ((rv = pfsFioCheckFileSlot(fileSlot)) < 0)
        return rv;
    pfsMount = fileSlot->clink->pfsMount;

    switch (cmd) {
        case PFS_IOCTL2_ALLOC:
            rv = pfsFioIoctl2Alloc(fileSlot->clink, *(int *)(arg), 1);
            break;

        case PFS_IOCTL2_FREE:
            pfsFioIoctl2Free(fileSlot->clink);
            break;

        case PFS_IOCTL2_ATTR_ADD:
        case PFS_IOCTL2_ATTR_DEL:
        case PFS_IOCTL2_ATTR_LOOKUP:
        case PFS_IOCTL2_ATTR_READ:
            rv = ioctl2Attr(fileSlot->clink, cmd, arg, buf, &fileSlot->aentryOffset);
            break;

        case PFS_IOCTL2_GET_INODE:
            memcpy(buf, fileSlot->clink->u.inode, sizeof(pfs_inode));
            rv = sizeof(pfs_inode);
            break;

        default:
            rv = -EINVAL;
            break;
    }

    if (pfsMount->flags & PFS_FIO_ATTR_WRITEABLE)
        pfsCacheFlushAllDirty(pfsMount);
    rv = pfsFioCheckForLastError(pfsMount, rv);
    SignalSema(pfsFioSema);

    return rv;
}

static int ioctl2Attr(pfs_cache_t *clink, int cmd, void *arg, void *outbuf, u32 *offset)
{ // attr set, attr delete, attr lookup, attr read cmds
    int rv;
    pfs_cache_t *flink;

    if ((flink = pfsCacheGetData(clink->pfsMount, clink->sub, clink->sector + 1, PFS_CACHE_FLAG_NOTHING, &rv)) == NULL)
        return rv;

    switch (cmd) {
        case PFS_IOCTL2_ATTR_ADD:
            rv = ioctl2AttrAdd(flink, arg);
            break;

        case PFS_IOCTL2_ATTR_DEL:
            rv = ioctl2AttrDelete(flink, arg);
            break;

        case PFS_IOCTL2_ATTR_LOOKUP:
            rv = ioctl2AttrLoopUp(flink, arg, outbuf);
            break;

        case PFS_IOCTL2_ATTR_READ:
            rv = ioctl2AttrRead(flink, outbuf, offset);
            break;
    }
    pfsCacheFree(flink);

    return rv;
}

void pfsFioDevctlCloseAll(void)
{
    u32 i;

    for (i = 0; i < pfsConfig.maxOpen; i++) {
        if (pfsFileSlots[i].fd)
            pfsFioCloseFileSlot(&pfsFileSlots[i]);
    }
    for (i = 0; i < pfsConfig.maxOpen; i++) {
        pfs_mount_t *pfsMount;
        if ((pfsMount = pfsGetMountedUnit(i)) != NULL)
            pfsCacheFlushAllDirty(pfsMount);
    }
}

static int devctlFsckStat(pfs_mount_t *pfsMount, int mode)
{
    int rv;
    pfs_cache_t *clink;

    if ((clink = pfsCacheAllocClean(&rv)) != NULL) {
        rv = pfsFsckStat(pfsMount, clink->u.superblock, PFS_FSCK_STAT_ERROR_0x02, mode);
        pfsCacheFree(clink);
    }
    return rv;
}

static pfs_aentry_t *getAentry(pfs_cache_t *clink, char *key, char *value, int mode)
{ // mode 0=lookup, 1=add, 2=delete
    int kLen, fullsize;
    pfs_aentry_t *aentry = clink->u.aentry;
    pfs_aentry_t *aentryLast = NULL;
    u32 end;

    kLen = strlen(key);
    fullsize = (kLen + strlen(value) + 7) & ~3;
    for (end = (u32)aentry + 1024; (u32)end < (u32)(aentry); (char *)aentry += aentry->aLen) {
        if (aentry->aLen & 3)
            PFS_PRINTF(PFS_DRV_NAME " Error: attrib-entry allocated length/4 != 0\n");
        if (aentry->aLen < ((aentry->kLen + aentry->vLen + 7) & ~3)) {
            PFS_PRINTF(PFS_DRV_NAME " Panic: attrib-entry is too small\n");
            return NULL;
        }
        if ((u32)end < (u32)aentry + aentry->aLen)
            PFS_PRINTF(PFS_DRV_NAME " Error: attrib-emtru too big\n");

        switch (mode) {
            case 0: // lookup
                if (kLen == aentry->kLen)
                    if (memcmp(key, aentry->str, kLen) == 0)
                        return aentry;
                break;

            case 1: // add
                if (aentry->kLen == 0) {
                    if (aentry->aLen >= fullsize)
                        return aentry;
                }
                if (aentry->aLen - ((aentry->kLen + aentry->vLen + 7) & ~3) < fullsize)
                    continue;
                return aentry;

            default: // delete
                if (kLen == aentry->kLen) {
                    if (memcmp(key, aentry->str, kLen) == 0) {
                        if (aentryLast != NULL) {
                            aentryLast->aLen += aentry->aLen;
                            return aentry;
                        }
                        // delete it :P
                        aentry->kLen = 0;
                        aentry->vLen = 0;
                        return aentry;
                    }
                }
                aentryLast = aentry;
                break;
        }
    }
    return NULL;
}

static int ioctl2AttrAdd(pfs_cache_t *clink, pfs_ioctl2attr_t *attr)
{
    u32 kLen, vLen;
    pfs_aentry_t *aentry;
    u32 tmp;

    // input check
    kLen = strlen(attr->key);
    vLen = strlen(attr->value);
    if (kLen >= 256 || vLen >= 256) // max size safe e check
        return -EINVAL;

    if (kLen == 0 || vLen == 0) // no input check
        return -EINVAL;

    if (getAentry(clink, attr->key, NULL, 0))
        return -EEXIST;
    if (!(aentry = getAentry(clink, attr->key, attr->value, 1)))
        return -ENOSPC;

    if (aentry->kLen == 0)
        tmp = aentry->aLen;
    else
        tmp = aentry->aLen - ((aentry->kLen + (aentry->vLen + 7)) & ~3);

    aentry->aLen -= tmp;
    aentry = (pfs_aentry_t *)((u8 *)aentry + aentry->aLen);
    aentry->kLen = kLen;
    aentry->vLen = vLen;
    aentry->aLen = tmp;
    memcpy(&aentry->str[0], attr->key, aentry->kLen);
    memcpy(&aentry->str[aentry->kLen], attr->value, aentry->vLen);
    clink->flags |= PFS_CACHE_FLAG_DIRTY;

    return 0;
}

static int ioctl2AttrDelete(pfs_cache_t *clink, void *arg)
{
    pfs_aentry_t *aentry;

    if ((aentry = getAentry(clink, arg, 0, 2)) == NULL)
        return -ENOENT;
    clink->flags |= PFS_CACHE_FLAG_DIRTY;
    return 0;
}

static int ioctl2AttrLoopUp(pfs_cache_t *clink, char *key, char *value)
{
    pfs_aentry_t *aentry;

    if ((aentry = getAentry(clink, key, 0, 0))) {
        memcpy(value, &aentry->str[aentry->kLen], aentry->vLen);
        value[aentry->vLen] = 0;
        return aentry->vLen;
    }
    return -ENOENT;
}

static int ioctl2AttrRead(pfs_cache_t *clink, pfs_ioctl2attr_t *attr, u32 *offset)
{
    pfs_aentry_t *aentry;

    if (*offset >= 1024)
        return 0;
    do {
        aentry = (pfs_aentry_t *)((u32)(clink->u.inode) + *offset);
        memcpy(attr->key, &aentry->str[0], aentry->kLen);
        attr->key[aentry->kLen] = 0;
        memcpy(attr->value, &aentry->str[aentry->kLen], aentry->vLen);
        attr->value[aentry->vLen] = 0;
        *offset += aentry->aLen; // next
        if (aentry->kLen != 0)
            break;
    } while (*offset < 1024);

    return aentry->kLen;
}

int pfsFioIoctl2Alloc(pfs_cache_t *clink, int msize, int mode)
{
    pfs_blockpos_t blockpos;
    int result = 0;
    u32 val;
    int zsize;

    zsize = clink->pfsMount->zsize;
    val = ((msize - 1 + zsize) & (-zsize)) / zsize;

    if (mode == 0)
        if (((clink->u.inode->number_blocks - clink->u.inode->number_segdesg) * (u64)zsize) >= (clink->u.inode->size + msize))
            return 0;

    if ((blockpos.inode = pfsBlockGetLastSegmentDescriptorInode(clink, &result))) {
        blockpos.block_offset = blockpos.byte_offset = 0;
        blockpos.block_segment = clink->u.inode->number_data - 1;
        val -= pfsBlockExpandSegment(clink, &blockpos, val);
        while (val && ((result = pfsBlockAllocNewSegment(clink, &blockpos, val)) >= 0)) {
            val -= result;
            result = 0;
        }
        pfsCacheFree(blockpos.inode);
    }
    return result;
}

void pfsFioIoctl2Free(pfs_cache_t *pfree)
{
    pfs_blockinfo b;
    int result;
    pfs_mount_t *pfsMount = pfree->pfsMount;
    pfs_inode *inode = pfree->u.inode;
    u32 nextsegdesc = 1, limit = inode->number_data, i, j = 0, zones;
    pfs_blockinfo *bi;
    pfs_cache_t *clink;

    zones = (u32)(inode->size / pfsMount->zsize);
    if (inode->size % pfsMount->zsize)
        zones++;
    if (inode->number_segdesg + zones == inode->number_blocks)
        return;

    j = zones;
    b.number = 0;
    clink = pfsCacheUsedAdd(pfree);

    // iterate through each of the block segments used by the inode
    for (i = 1; i < limit && j; i++) {
        if (pfsFixIndex(i) == 0) {
            if ((clink = pfsBlockGetNextSegment(clink, &result)) == 0)
                return;

            nextsegdesc++;
        } else if (j < clink->u.inode->data[pfsFixIndex(i)].count) {
            clink->u.inode->data[pfsFixIndex(i)].count -= j;
            b.subpart = clink->u.inode->data[pfsFixIndex(i)].subpart;
            b.count = j;
            b.number = clink->u.inode->data[pfsFixIndex(i)].number +
                       clink->u.inode->data[pfsFixIndex(i)].count;
            j = 0;
            clink->flags |= PFS_CACHE_FLAG_DIRTY;
        } else
            j -= clink->u.inode->data[pfsFixIndex(i)].count;
    }

    pfree->u.inode->number_data = i;
    pfree->u.inode->number_blocks = zones + nextsegdesc;
    pfree->u.inode->number_segdesg = nextsegdesc;
    pfree->u.inode->last_segment.number = clink->u.inode->data[0].number;
    pfree->u.inode->last_segment.subpart = clink->u.inode->data[0].subpart;
    pfree->u.inode->last_segment.count = clink->u.inode->data[0].count;
    pfree->flags |= PFS_CACHE_FLAG_DIRTY;

    if (b.number)
        pfsBitmapFreeBlockSegment(pfsMount, &b);

    while (i < limit) {
        if (pfsFixIndex(i) == 0) {
            if ((clink = pfsBlockGetNextSegment(clink, &result)) == 0)
                return;
        }
        bi = &clink->u.inode->data[pfsFixIndex(i++)];
        pfsBitmapFreeBlockSegment(pfsMount, bi);
        pfsCacheMarkClean(pfsMount, bi->subpart, bi->number << pfsMount->inode_scale,
                          (bi->number + bi->count) << pfsMount->inode_scale);
    }

    pfsCacheFree(clink);
}
