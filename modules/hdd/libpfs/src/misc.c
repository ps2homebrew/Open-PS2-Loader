/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Miscellaneous routines
*/

#include <errno.h>
#include <stdio.h>
#include <iomanX.h>
#include <intrman.h>
#include <sysmem.h>
#include <sysclib.h>
#include <ctype.h>
#include <cdvdman.h>
#include <hdd-ioctl.h>

#include "pfs-opt.h"
#include "libpfs.h"

///////////////////////////////////////////////////////////////////////////////
//   Function definitions

void *pfsAllocMem(int size)
{
    int intrStat;
    void *mem;

    CpuSuspendIntr(&intrStat);
    mem = AllocSysMemory(ALLOC_FIRST, size, NULL);
    CpuResumeIntr(intrStat);

    return mem;
}

void pfsFreeMem(void *buffer)
{
    int OldState;

    CpuSuspendIntr(&OldState);
    FreeSysMemory(buffer);
    CpuResumeIntr(OldState);
}

int pfsGetTime(pfs_datetime_t *tm)
{
    sceCdCLOCK cdtime;
    static pfs_datetime_t timeBuf = {
        0, 0x0D, 0x0E, 0x0A, 0x0D, 1, 2003 // used if can not get time...
    };

    if (sceCdReadClock(&cdtime) != 0 && cdtime.stat == 0) {
        timeBuf.sec = btoi(cdtime.second);
        timeBuf.min = btoi(cdtime.minute);
        timeBuf.hour = btoi(cdtime.hour);
        timeBuf.day = btoi(cdtime.day);
        timeBuf.month = btoi(cdtime.month & 0x7F); //The old CDVDMAN sceCdReadClock() function does not automatically file off the highest bit.
        timeBuf.year = btoi(cdtime.year) + 2000;
    }
    memcpy(tm, &timeBuf, sizeof(pfs_datetime_t));
    return 0;
}

int pfsFsckStat(pfs_mount_t *pfsMount, pfs_super_block_t *superblock,
                u32 stat, int mode)
{ // mode 0=set flag, 1=remove flag, else check stat

    if (pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_SUPER_SECTOR, 1,
                                     PFS_IO_MODE_READ) == 0) {
        switch (mode) {
            case PFS_MODE_SET_FLAG:
                superblock->pfsFsckStat |= stat;
                break;
            case PFS_MODE_REMOVE_FLAG:
                superblock->pfsFsckStat &= ~stat;
                break;
            default /*PFS_MODE_CHECK_FLAG*/:
                return 0 < (superblock->pfsFsckStat & stat);
        }
        pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_SUPER_SECTOR, 1,
                                     PFS_IO_MODE_WRITE);
        pfsMount->blockDev->flushCache(pfsMount->fd);
    }
    return 0;
}

void pfsPrintBitmap(const u32 *bitmap)
{
    u32 i, j;
    char a[48 + 1], b[16 + 1];

    b[16] = 0;
    for (i = 0; i < 32; i++) {
        memset(a, 0, 49);
        for (j = 0; j < 16; j++) {
            const char *c = (const char *)bitmap + j + i * 16;

            sprintf(a + j * 3, "%02x ", *c);
            b[j] = ((*c >= 0) && (isgraph(*c))) ?
                       *c :
                       '.';
        }
        PFS_PRINTF("%s%s\n", a, b);
    }
}

int pfsGetScale(int num, int size)
{
    int scale = 0;

    while ((size << scale) != num)
        scale++;

    return scale;
}

u32 pfsFixIndex(u32 index)
{
    if (index < 114)
        return index;

    index -= 114;
    return index % 123;
}

///////////////////////////////////////////////////////////////////////////////
//   Functions to work with hdd.irx

static int pfsHddTransfer(int fd, void *buffer, u32 sub /*0=main 1+=subs*/, u32 sector,
                          u32 size /* in sectors*/, u32 mode);
static u32 pfsHddGetSubCount(int fd);
static u32 pfsHddGetPartSize(int fd, u32 sub /*0=main 1+=subs*/);
static void pfsHddSetPartError(int fd);
static int pfsHddFlushCache(int fd);

#define NUM_SUPPORTED_DEVICES 1
pfs_block_device_t pfsBlockDeviceCallTable[NUM_SUPPORTED_DEVICES] = {
    {
        "hdd",
        &pfsHddTransfer,
        &pfsHddGetSubCount,
        &pfsHddGetPartSize,
        &pfsHddSetPartError,
        &pfsHddFlushCache,
    }};

pfs_block_device_t *pfsGetBlockDeviceTable(const char *name)
{
    char *end;
    char devname[32];
    char *tmp;
    u32 len;
    int i;

    while (name[0] == ' ')
        name++;

    end = strchr(name, ':');
    if (!end) {
        PFS_PRINTF(PFS_DRV_NAME ": Error: Unknown block device '%s'\n", name);
        return NULL;
    }

    len = (u32)end - (u32)name;
    strncpy(devname, name, len);
    devname[len] = '\0';

    // Loop until digit is found, then terminate string at that digit.
    // Should then have just the device name left, minus any front spaces or trailing digits.
    tmp = devname;
    while (!(isdigit(tmp[0])))
        tmp++;
    tmp[0] = '\0';

    for (i = 0; i < NUM_SUPPORTED_DEVICES; i++)
        if (!strcmp(pfsBlockDeviceCallTable[i].devName, devname))
            return &pfsBlockDeviceCallTable[i];

    return NULL;
}

static int pfsHddTransfer(int fd, void *buffer, u32 sub /*0=main 1+=subs*/, u32 sector,
                          u32 size /* in sectors*/, u32 mode)
{
    hddIoctl2Transfer_t t;

    t.sub = sub;
    t.sector = sector;
    t.size = size;
    t.mode = mode;
    t.buffer = buffer;

    return ioctl2(fd, HIOCTRANSFER, &t, 0, NULL, 0);
}

static u32 pfsHddGetSubCount(int fd)
{
    return ioctl2(fd, HIOCNSUB, NULL, 0, NULL, 0);
}

static u32 pfsHddGetPartSize(int fd, u32 sub /*0=main 1+=subs*/)
{ // of a partition
    return ioctl2(fd, HIOCGETSIZE, &sub, 0, NULL, 0);
}

static void pfsHddSetPartError(int fd)
{
    ioctl2(fd, HIOCSETPARTERROR, NULL, 0, NULL, 0);
}

static int pfsHddFlushCache(int fd)
{
    return ioctl2(fd, HIOCFLUSH, NULL, 0, NULL, 0);
}
