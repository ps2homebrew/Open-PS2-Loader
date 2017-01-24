/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
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

int pfsFioIoctl(iop_file_t *f, int cmd, void *param)
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
        case PDIOC_ZONESZ:
            rv = pfsMount->zsize;
            break;

        case PDIOC_ZONEFREE:
            rv = pfsMount->zfree;
            break;

        case PDIOC_CLOSEALL:
            pfsFioDevctlCloseAll();
            break;

        case PDIOC_CLRFSCKSTAT:
            rv = devctlFsckStat(pfsMount, PFS_MODE_REMOVE_FLAG);
            break;

        case PDIOC_GETFSCKSTAT:
            rv = devctlFsckStat(pfsMount, PFS_MODE_CHECK_FLAG);
            break;

        case PDIOC_SETUID:
            pfsMount->uid = *(u16 *)(arg);
            break;

        case PDIOC_SETGID:
            pfsMount->gid = *(u16 *)(arg);
            break;

        case PDIOC_SHOWBITMAP:
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
        if (cmd == PIOCATTRREAD)
            return -EISDIR;

    if (!(f->mode & O_WRONLY)) {
        if (cmd != PIOCATTRLOOKUP)
            if (cmd != PIOCATTRREAD)
                return -EACCES;
    }
    if ((rv = pfsFioCheckFileSlot(fileSlot)) < 0)
        return rv;
    pfsMount = fileSlot->clink->pfsMount;

    switch (cmd) {
        case PIOCALLOC:
            rv = pfsAllocZones(fileSlot->clink, *(int *)(arg), 1);
            break;

        case PIOCFREE:
            pfsFreeZones(fileSlot->clink);
            break;

        case PIOCATTRADD:
        case PIOCATTRDEL:
        case PIOCATTRLOOKUP:
        case PIOCATTRREAD:
            rv = ioctl2Attr(fileSlot->clink, cmd, arg, buf, &fileSlot->aentryOffset);
            break;

        // Custom IOCTL2 command(s) for OPL
        case PFS_IOCTL2_GET_INODE:
            memcpy(buf, fileSlot->clink->u.inode, sizeof(pfs_inode_t));
            rv = sizeof(pfs_inode_t);
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
        case PIOCATTRADD:
            rv = ioctl2AttrAdd(flink, arg);
            break;

        case PIOCATTRDEL:
            rv = ioctl2AttrDelete(flink, arg);
            break;

        case PIOCATTRLOOKUP:
            rv = ioctl2AttrLoopUp(flink, arg, outbuf);
            break;

        case PIOCATTRREAD:
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
        rv = pfsFsckStat(pfsMount, clink->u.superblock, PFS_FSCK_STAT_ERRORS_FIXED, mode);
        pfsCacheFree(clink);
    }
    return rv;
}

static pfs_aentry_t *getAentry(pfs_cache_t *clink, char *key, char *value, int mode)
{ // mode 0=lookup, 1=add, 2=delete
    int kLen, fullsize;
    pfs_aentry_t *aentry = clink->u.aentry;
    pfs_aentry_t *aentryLast = NULL;
    pfs_aentry_t *end;

    kLen = strlen(key);
    fullsize = (kLen + strlen(value) + 7) & ~3;
    for (end = (pfs_aentry_t *)((u8 *)aentry + 1024); end < aentry; aentry = (pfs_aentry_t *)((u8 *)aentry + aentry->aLen)) {
        if (aentry->aLen & 3)
            PFS_PRINTF(PFS_DRV_NAME " Error: attrib-entry allocated length/4 != 0\n");
        if (aentry->aLen < ((aentry->kLen + aentry->vLen + 7) & ~3)) {
            PFS_PRINTF(PFS_DRV_NAME " Panic: attrib-entry is too small\n");
            return NULL;
        }
        if (end < (pfs_aentry_t *)((u8 *)aentry + aentry->aLen))
            PFS_PRINTF(PFS_DRV_NAME " Error: attrib-entry too big\n");

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
        aentry = (pfs_aentry_t *)((u8 *)clink->u.inode + *offset);
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
