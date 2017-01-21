/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS metadata cache manipulation routines
*/

#include <errno.h>
#include <stdio.h>
#include <sysclib.h>
#include <hdd-ioctl.h>

#include "pfs-opt.h"
#include "libpfs.h"

extern int pfsBlockSize;

pfs_cache_t *pfsCacheBuf;
u32 pfsCacheNumBuffers;

void pfsCacheFree(pfs_cache_t *clink)
{
	if(clink==NULL) {
		PFS_PRINTF(PFS_DRV_NAME": Warning: NULL buffer returned\n");
		return;
	}

	if(clink->nused==0){
		PFS_PRINTF(PFS_DRV_NAME": Error: Unused cache returned\n");
		return;
	}

	clink->nused--;
	if(clink->pfsMount!=NULL) {
		if(clink->nused!=0)
			return;
		pfsCacheLink(pfsCacheBuf->prev, clink);
		return;
	}
	if(clink->nused!=0) {
		PFS_PRINTF(PFS_DRV_NAME": Warning: Invalidated buffer is in use\n");
		return;
	}
	pfsCacheLink(pfsCacheBuf, clink);
}

void pfsCacheLink(pfs_cache_t *clink, pfs_cache_t *cnew)
{
	cnew->prev=clink;
	cnew->next=clink->next;
	clink->next->prev=cnew;
	clink->next=cnew;
}

pfs_cache_t *pfsCacheUnLink(pfs_cache_t *clink)
{
	clink->prev->next=clink->next;
	clink->next->prev=clink->prev;
	return clink;
}

pfs_cache_t *pfsCacheUsedAdd(pfs_cache_t *clink)
{
	clink->nused++;
	return clink;
}

int pfsCacheTransfer(pfs_cache_t* clink, int mode)
{
	pfs_mount_t *pfsMount=clink->pfsMount;
	int err;

	if(pfsMount->lastError == 0) {	// no error
		if((err=pfsMount->blockDev->transfer(pfsMount->fd, clink->u.inode, clink->sub,
			clink->sector << pfsBlockSize, 1 << pfsBlockSize, mode))==0) {
			if(mode==PFS_IO_MODE_READ) {
				if(clink->flags & PFS_CACHE_FLAG_SEGD && ((pfs_inode_t *)clink->u.inode)->magic!=PFS_SEGD_MAGIC)
					err=-EIO;
				if(clink->flags & PFS_CACHE_FLAG_SEGI && ((pfs_inode_t *)clink->u.inode)->magic!=PFS_SEGI_MAGIC)
					err=-EIO;
				if(clink->flags & (PFS_CACHE_FLAG_SEGD|PFS_CACHE_FLAG_SEGI)) {
					if(((pfs_inode_t *)clink->u.inode)->checksum!=pfsInodeCheckSum(clink->u.inode))
						err=-EIO;
				}
			}
		}
		if(err!=0) {
			PFS_PRINTF(PFS_DRV_NAME": Error: Disk error partition %ld, block %ld, err %d\n",
				clink->sub, clink->sector, err);
#ifndef PFS_NO_WRITE_ERROR_STAT
			pfsMount->blockDev->setPartitionError(pfsMount->fd);
			pfsFsckStat(pfsMount, clink->u.superblock, PFS_FSCK_STAT_WRITE_ERROR, PFS_MODE_SET_FLAG);
			pfsMount->lastError=err;
#endif
		}
	}
	clink->flags&=~PFS_CACHE_FLAG_DIRTY; // clear dirty :)
	return pfsMount->lastError;
}

void pfsCacheFlushAllDirty(pfs_mount_t *pfsMount)
{
	u32 i;
	int found=0;

	for(i=1;i<pfsCacheNumBuffers+1;i++){
		if(pfsCacheBuf[i].pfsMount == pfsMount &&
			pfsCacheBuf[i].flags & PFS_CACHE_FLAG_DIRTY)
				found=1;
	}
	if(found) {
		pfsJournalWrite(pfsMount, pfsCacheBuf+1, pfsCacheNumBuffers);
		for(i=1;i<pfsCacheNumBuffers+1;i++){
			if(pfsCacheBuf[i].pfsMount == pfsMount &&
				pfsCacheBuf[i].flags & PFS_CACHE_FLAG_DIRTY)
					pfsCacheTransfer(&pfsCacheBuf[i], 1);
		}
	}

	pfsJournalReset(pfsMount);
}

pfs_cache_t *pfsCacheAlloc(pfs_mount_t *pfsMount, u16 sub, u32 scale,
					int flags, int *result)
{
	pfs_cache_t *allocated;

	if (pfsCacheBuf->prev==pfsCacheBuf && pfsCacheBuf->prev->next==pfsCacheBuf->prev) {
		PFS_PRINTF(PFS_DRV_NAME": Error: Free buffer list is empty\n");
		*result=-ENOMEM;
		return NULL;
	}
	allocated=pfsCacheBuf->next;
	if (pfsCacheBuf->next==NULL)
		PFS_PRINTF(PFS_DRV_NAME": Panic: Null pointer allocated\n");
	if (allocated->pfsMount && (allocated->flags & PFS_CACHE_FLAG_DIRTY))
		pfsCacheFlushAllDirty(allocated->pfsMount);
	allocated->flags 	= flags & PFS_CACHE_FLAG_MASKTYPE;
	allocated->pfsMount	= pfsMount;
	allocated->sub		= sub;
	allocated->sector	= scale;
	allocated->nused	= 1;
	return pfsCacheUnLink(allocated);
}

pfs_cache_t *pfsCacheGetData(pfs_mount_t *pfsMount, u16 sub, u32 sector,
					int flags, int *result)
{
	u32 i;
	pfs_cache_t *clink;

	*result=0;

	for (i=1; i < pfsCacheNumBuffers + 1; i++)
		if ( pfsCacheBuf[i].pfsMount &&
		    (pfsCacheBuf[i].pfsMount==pfsMount) &&
		    (pfsCacheBuf[i].sector  == sector))
			if (pfsCacheBuf[i].sub==sub){
				pfsCacheBuf[i].flags &= PFS_CACHE_FLAG_MASKSTATUS;
				pfsCacheBuf[i].flags |= flags & PFS_CACHE_FLAG_MASKTYPE;
				if (pfsCacheBuf[i].nused == 0)
					pfsCacheUnLink(&pfsCacheBuf[i]);
				pfsCacheBuf[i].nused++;
				return &pfsCacheBuf[i];
			}

	clink=pfsCacheAlloc(pfsMount, sub, sector, flags, result);

	if (clink){
		if (flags & PFS_CACHE_FLAG_NOLOAD)
			return clink;

		if ((*result=pfsCacheTransfer(clink, PFS_IO_MODE_READ))>=0)
			return clink;

		clink->pfsMount=NULL;
		pfsCacheFree(clink);
	}
	return NULL;
}

pfs_cache_t *pfsCacheAllocClean(int *result)
{
	*result = 0;
	return pfsCacheAlloc(NULL, 0, 0, 0, result);
}

// checks if the pfsCacheBuf list has some room
int pfsCacheIsFull(void)
{
	if (pfsCacheBuf->prev != pfsCacheBuf)	return 0;
	return pfsCacheBuf->prev->next == pfsCacheBuf->prev;
}

int pfsCacheInit(u32 numBuf, u32 bufSize)
{
	char *cacheData;
	u32 i;

	if(numBuf > 127) {
		PFS_PRINTF(PFS_DRV_NAME": Error: Number of buffers larger than 127.\n");
		return -EINVAL;
	}

	cacheData = pfsAllocMem(numBuf * bufSize);

	if(!cacheData || !(pfsCacheBuf = pfsAllocMem((numBuf + 1) * sizeof(pfs_cache_t))))
		return -ENOMEM;

	pfsCacheNumBuffers = numBuf;
	memset(pfsCacheBuf, 0, (numBuf + 1) * sizeof(pfs_cache_t));

	pfsCacheBuf->next = pfsCacheBuf;
	pfsCacheBuf->prev = pfsCacheBuf;

	for(i = 1; i < numBuf + 1; i++)
	{
		pfsCacheBuf[i].u.data = cacheData;
		pfsCacheLink(pfsCacheBuf->prev, &pfsCacheBuf[i]);
		cacheData += bufSize;
	}

	return 0;
}

void pfsCacheClose(pfs_mount_t *pfsMount)
{
	unsigned int i;

	pfsCacheFlushAllDirty(pfsMount);
	for(i=1; i < pfsCacheNumBuffers+1;i++){
		if(pfsCacheBuf[i].pfsMount==pfsMount)
			pfsCacheBuf[i].pfsMount=NULL;
	}
}

void pfsCacheMarkClean(pfs_mount_t *pfsMount, u32 subpart, u32 sectorStart, u32 sectorEnd)
{
	u32 i;

	for(i=1; i< pfsCacheNumBuffers+1;i++){
		if(pfsCacheBuf[i].pfsMount==pfsMount && pfsCacheBuf[i].sub==subpart) {
			if(pfsCacheBuf[i].sector >= sectorStart && pfsCacheBuf[i].sector < sectorEnd)
				pfsCacheBuf[i].flags&=~PFS_CACHE_FLAG_DIRTY;
		}
	}
}
