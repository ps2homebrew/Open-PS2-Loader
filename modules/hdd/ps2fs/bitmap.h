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

#ifndef _BITMAP_H
#define _BITMAP_H

#define BITMAP_ALLOC	0
#define BITMAP_FREE		1

typedef struct
{
	u32 chunk;
	u32 index;
	u32 bit;
	u32 partitionChunks;
	u32 partitionRemainder;
} t_bitmapInfo;

void bitmapSetupInfo(pfs_mount_t *pfsMount, t_bitmapInfo *info, u32 subpart, u32 number);
void bitmapAllocFree(pfs_cache_t *clink, u32 operation, u32 subpart, u32 chunk, u32 index, u32 _bit, u32 count);
int bitmapAllocateAdditionalZones(pfs_mount_t *pfsMount, pfs_blockinfo *bi, u32 count);
int bitmapAllocZones(pfs_mount_t *pfsMount, pfs_blockinfo *bi, u32 amount);
int searchFreeZone(pfs_mount_t *pfsMount, pfs_blockinfo *bi, u32 max_count);
void bitmapFreeBlockSegment(pfs_mount_t *pfsMount, pfs_blockinfo *bi);
int calcFreeZones(pfs_mount_t *pfsMount, int sub);
void bitmapShow(pfs_mount_t *pfsMount);
void bitmapFreeInodeBlocks(pfs_cache_t *clink);

#endif /* _BITMAP_H */
