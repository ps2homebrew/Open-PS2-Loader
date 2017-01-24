/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS metadata journal related routines
*/

#include <stdio.h>
#include <sysclib.h>
#include <hdd-ioctl.h>

#include "pfs-opt.h"
#include "libpfs.h"

extern int pfsBlockSize;

///////////////////////////////////////////////////////////////////////////////
//	Globals

pfs_journal_t pfsJournalBuf;

///////////////////////////////////////////////////////////////////////////////
//	Function defenitions

int pfsJournalChecksum(void *header)
{
    u32 *ptr = (u32 *)header;
    u32 sum = 0;
    int i;
    for (i = 2; i < 256; i++)
        sum += ptr[i];
    return sum & 0xFFFF;
}

void pfsJournalWrite(pfs_mount_t *pfsMount, pfs_cache_t *clink, u32 pfsCacheNumBuffers)
{
    u32 i = 0;
    u32 logSector = 2;

    for (i = 0; i < pfsCacheNumBuffers; i++) {
        if ((clink[i].flags & PFS_CACHE_FLAG_DIRTY) && clink[i].pfsMount == pfsMount) {
            if (clink[i].flags & (PFS_CACHE_FLAG_SEGD | PFS_CACHE_FLAG_SEGI))
                clink[i].u.inode->checksum = pfsInodeCheckSum(clink[i].u.inode);
            pfsJournalBuf.log[pfsJournalBuf.num].sector = clink[i].sector << pfsBlockSize;
            pfsJournalBuf.log[pfsJournalBuf.num].sub = clink[i].sub;
            pfsJournalBuf.log[pfsJournalBuf.num].logSector = logSector;
            pfsJournalBuf.num += 1;
        }
        logSector += 2;
    }

    if (pfsMount->blockDev->transfer(pfsMount->fd, clink->u.inode, 0,
                                     (pfsMount->log.number << pfsMount->sector_scale) + 2, pfsCacheNumBuffers * 2,
                                     PFS_IO_MODE_WRITE) >= 0)
        pfsJournalFlush(pfsMount);
}

int pfsJournalReset(pfs_mount_t *pfsMount)
{
    int rv;

    memset(&pfsJournalBuf, 0, sizeof(pfs_journal_t));
    pfsJournalBuf.magic = PFS_JOUNRNAL_MAGIC;

    pfsMount->blockDev->flushCache(pfsMount->fd);

    rv = pfsMount->blockDev->transfer(pfsMount->fd, &pfsJournalBuf, 0,
                                      (pfsMount->log.number << pfsMount->sector_scale), 2, PFS_IO_MODE_WRITE);

    pfsMount->blockDev->flushCache(pfsMount->fd);
    return rv;
}

int pfsJournalResetThis(pfs_block_device_t *blockDev, int fd, u32 sector)
{
    memset(&pfsJournalBuf, 0, sizeof(pfs_journal_t));
    pfsJournalBuf.magic = PFS_JOUNRNAL_MAGIC;
    return blockDev->transfer(fd, &pfsJournalBuf, 0, sector, 2, 1);
}

int pfsJournalFlush(pfs_mount_t *pfsMount)
{ // this write any thing that in are journal buffer :)
    int rv;

    pfsMount->blockDev->flushCache(pfsMount->fd);

    pfsJournalBuf.checksum = pfsJournalChecksum(&pfsJournalBuf);

    rv = pfsMount->blockDev->transfer(pfsMount->fd, &pfsJournalBuf, 0,
                                      (pfsMount->log.number << pfsMount->sector_scale), 2, PFS_IO_MODE_WRITE);

    pfsMount->blockDev->flushCache(pfsMount->fd);
    return rv;
}

int pfsJournalRestore(pfs_mount_t *pfsMount)
{
    int rv;
    int result;
    pfs_cache_t *clink;
    u32 i;

    // Read journal buffer from disk
    rv = pfsMount->blockDev->transfer(pfsMount->fd, &pfsJournalBuf, 0,
                                      (pfsMount->log.number << pfsMount->sector_scale), 2, PFS_IO_MODE_READ);

    if (rv || (pfsJournalBuf.magic != PFS_JOUNRNAL_MAGIC) ||
        (pfsJournalBuf.checksum != (u16)pfsJournalChecksum(&pfsJournalBuf))) {
        PFS_PRINTF(PFS_DRV_NAME ": Error: cannot read log/invalid log\n");
        return pfsJournalReset(pfsMount);
    }

    if (pfsJournalBuf.num == 0) {
        return pfsJournalReset(pfsMount);
    }

    clink = pfsCacheAllocClean(&result);
    if (!clink)
        return result;

    for (i = 0; i < pfsJournalBuf.num; i++) {
        PFS_PRINTF(PFS_DRV_NAME ": Log overwrite %d:%08lx\n", pfsJournalBuf.log[i].sub, pfsJournalBuf.log[i].sector);

        // Read data in from log section on disk into cache buffer
        rv = pfsMount->blockDev->transfer(pfsMount->fd, clink->u.data, 0,
                                          (pfsMount->log.number << pfsMount->sector_scale) + pfsJournalBuf.log[i].logSector, 2,
                                          PFS_IO_MODE_READ);

        // Write from cache buffer into destination location on disk
        if (!rv)
            pfsMount->blockDev->transfer(pfsMount->fd, clink->u.data, pfsJournalBuf.log[i].sub,
                                         pfsJournalBuf.log[i].sector, 2, PFS_IO_MODE_WRITE);
    }

    pfsCacheFree(clink);
    return pfsJournalReset(pfsMount);
}
