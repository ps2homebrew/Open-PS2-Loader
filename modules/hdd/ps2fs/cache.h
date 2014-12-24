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
*/

#ifndef _CACHE_H
#define _CACHE_H

void cacheAdd(pfs_cache_t *clink);
void cacheLink(pfs_cache_t *clink, pfs_cache_t *cnew);
pfs_cache_t *cacheUnLink(pfs_cache_t *clink);
pfs_cache_t *cacheUsedAdd(pfs_cache_t *clink);
int cacheTransfer(pfs_cache_t* clink, int mode);
void cacheFlushAllDirty(pfs_mount_t *pfsMount);
pfs_cache_t *cacheAlloc(pfs_mount_t *pfsMount, u16 sub, u32 scale, int flags, int *result);
pfs_cache_t *cacheGetData(pfs_mount_t *pfsMount, u16 sub, u32 scale, int flags, int *result);
pfs_cache_t *cacheAllocClean(int *result);
int cacheIsFull();
int cacheInit(u32 numBuf, u32 bufSize);
void cacheMarkClean(pfs_mount_t *pfsMount, u32 subpart, u32 sectorStart, u32 sectorEnd);

#endif /* _CACHE_H */
