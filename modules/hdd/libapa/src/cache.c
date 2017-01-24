/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# APA cache manipulation routines
*/

#include <errno.h>
#include <iomanX.h>
#include <sysclib.h>
#include <stdio.h>
#include <hdd-ioctl.h>

#include "apa-opt.h"
#include "libapa.h"

//  Globals
static apa_cache_t *cacheBuf;
static int cacheSize;

int apaCacheInit(u32 size)
{
    apa_header_t *header;
    int i;

    cacheSize = size; // save size ;)
    if ((header = (apa_header_t *)apaAllocMem(size * sizeof(apa_header_t)))) {
        cacheBuf = apaAllocMem((size + 1) * sizeof(apa_cache_t));
        if (cacheBuf == NULL)
            return -ENOMEM;
    } else
        return -ENOMEM;
    // setup cache header...
    memset(cacheBuf, 0, (size + 1) * sizeof(apa_cache_t));
    cacheBuf->next = cacheBuf;
    cacheBuf->tail = cacheBuf;
    for (i = 1; i < size + 1; i++, header++) {
        cacheBuf[i].header = header;
        cacheBuf[i].device = -1;
        apaCacheLink(cacheBuf->tail, &cacheBuf[i]);
    }
    return 0;
}

void apaCacheLink(apa_cache_t *clink, apa_cache_t *cnew)
{
    cnew->tail = clink;
    cnew->next = clink->next;
    clink->next->tail = cnew;
    clink->next = cnew;
}

apa_cache_t *apaCacheUnLink(apa_cache_t *clink)
{
    clink->tail->next = clink->next;
    clink->next->tail = clink->tail;
    return clink;
}

int apaCacheTransfer(apa_cache_t *clink, int type)
{
    int err;
    if (type)
        err = apaWriteHeader(clink->device, clink->header, clink->sector);
    else // 0
        err = apaReadHeader(clink->device, clink->header, clink->sector);

    if (err) {
        APA_PRINTF(APA_DRV_NAME ": error: disk err %d on device %ld, sector %ld, type %d\n",
                   err, clink->device, clink->sector, type);
        if (type == 0) // save any read error's..
            apaSaveError(clink->device, clink->header, APA_SECTOR_SECTOR_ERROR, clink->sector);
    }
    clink->flags &= ~APA_CACHE_FLAG_DIRTY;
    return err;
}

void apaCacheFlushDirty(apa_cache_t *clink)
{
    if (clink->flags & APA_CACHE_FLAG_DIRTY)
        apaCacheTransfer(clink, APA_IO_MODE_WRITE);
}

int apaCacheFlushAllDirty(s32 device)
{
    u32 i;
    // flush apal
    for (i = 1; i < cacheSize + 1; i++) {
        if ((cacheBuf[i].flags & APA_CACHE_FLAG_DIRTY) && cacheBuf[i].device == device)
            apaJournalWrite(&cacheBuf[i]);
    }
    apaJournalFlush(device);
    // flush apa
    for (i = 1; i < cacheSize + 1; i++) {
        if ((cacheBuf[i].flags & APA_CACHE_FLAG_DIRTY) && cacheBuf[i].device == device)
            apaCacheTransfer(&cacheBuf[i], APA_IO_MODE_WRITE);
    }
    return apaJournalReset(device);
}

apa_cache_t *apaCacheGetHeader(s32 device, u32 sector, u32 mode, int *result)
{
    apa_cache_t *clink = NULL;
    int i;

    *result = 0;
    for (i = 1; i < cacheSize + 1; i++) {
        if (cacheBuf[i].sector == sector &&
            cacheBuf[i].device == device) {
            clink = &cacheBuf[i];
            break;
        }
    }
    if (clink != NULL) {
        // cached ver was found :)
        if (clink->nused == 0)
            clink = apaCacheUnLink(clink);
        clink->nused++;
        return clink;
    }
    if ((cacheBuf->tail == cacheBuf) &&
        (cacheBuf->tail == cacheBuf->tail->next)) {
        APA_PRINTF(APA_DRV_NAME ": error: free buffer empty\n");
    } else {
        clink = cacheBuf->next;
        if (clink->flags & APA_CACHE_FLAG_DIRTY)
            APA_PRINTF(APA_DRV_NAME ": error: dirty buffer allocated\n");
        clink->flags = 0;
        clink->nused = 1;
        clink->device = device;
        clink->sector = sector;
        clink = apaCacheUnLink(clink);
    }
    if (clink == NULL) {
        *result = -ENOMEM;
        return NULL;
    }
    if (!mode) {
        if ((*result = apaCacheTransfer(clink, APA_IO_MODE_READ)) < 0) {
            clink->nused = 0;
            clink->device = -1;
            apaCacheLink(cacheBuf, clink);
            clink = NULL;
        }
    }
    return clink;
}

void apaCacheFree(apa_cache_t *clink)
{
    if (clink == NULL) {
        APA_PRINTF(APA_DRV_NAME ": error: null buffer returned\n");
        return;
    }
    if (clink->nused == 0) {
        APA_PRINTF(APA_DRV_NAME ": error: unused cache returned\n");
        return;
    }
    if (clink->flags & APA_CACHE_FLAG_DIRTY)
        APA_PRINTF(APA_DRV_NAME ": error: dirty buffer returned\n");
    clink->nused--;
    if (clink->nused == 0)
        apaCacheLink(cacheBuf->tail, clink);
    return;
}

apa_cache_t *apaCacheAlloc(void)
{
    apa_cache_t *cnext;

    if ((cacheBuf->tail == cacheBuf) &&
        (cacheBuf->tail == cacheBuf->tail->next)) {
        APA_PRINTF(APA_DRV_NAME ": error: free buffer empty\n");
        return NULL;
    }
    cnext = cacheBuf->next;
    if (cnext->flags & APA_CACHE_FLAG_DIRTY)
        APA_PRINTF(APA_DRV_NAME ": error: dirty buffer allocated\n");
    cnext->nused = 1;
    cnext->flags = 0;
    cnext->device = -1;
    cnext->sector = -1;
    return apaCacheUnLink(cnext);
}
