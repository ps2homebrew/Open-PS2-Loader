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
# PFS metadata cache manipulation routines
*/

#include "pfs.h"

pfs_cache_t *cacheBuf;
u32 numBuffers;


void cacheAdd(pfs_cache_t *clink)
{
	if(clink==NULL) {
		printf("ps2fs: Warning: NULL buffer returned\n");
		return;
	}

	if(clink->nused==0){
		printf("ps2fs: Error: Unused cache returned\n");
		return;
	}

	clink->nused--;
	if(clink->pfsMount!=NULL) {
		if(clink->nused!=0)
			return;
		cacheLink(cacheBuf->prev, clink);
		return;
	}
	if(clink->nused!=0) {
		printf("ps2fs: Warning: Invalidated buffer is in use\n");
		return;
	}
	cacheLink(cacheBuf, clink);
}

void cacheLink(pfs_cache_t *clink, pfs_cache_t *cnew)
{
	cnew->prev=clink;
	cnew->next=clink->next;
	clink->next->prev=cnew;
	clink->next=cnew;
}

pfs_cache_t *cacheUnLink(pfs_cache_t *clink)
{
	clink->prev->next=clink->next;
	clink->next->prev=clink->prev;
	return clink;
}

pfs_cache_t *cacheUsedAdd(pfs_cache_t *clink)
{
	clink->nused++;
	return clink;
}

int cacheTransfer(pfs_cache_t* clink, int mode)
{
	pfs_mount_t *pfsMount=clink->pfsMount;
	int err;

	if(pfsMount->lastError == 0) {	// no error
		if((err=pfsMount->blockDev->transfer(pfsMount->fd, clink->u.inode, clink->sub,
			clink->sector << blockSize, 1 << blockSize, mode))==0) {
			if(mode==IOCTL2_TMODE_READ) {
				if(clink->flags & CACHE_FLAG_SEGD && ((pfs_inode *)clink->u.inode)->magic!=PFS_SEGD_MAGIC)
					err=-EIO;
				if(clink->flags & CACHE_FLAG_SEGI && ((pfs_inode *)clink->u.inode)->magic!=PFS_SEGI_MAGIC)
					err=-EIO;
				if(clink->flags & (CACHE_FLAG_SEGD|CACHE_FLAG_SEGI)) {
					if(((pfs_inode *)clink->u.inode)->checksum!=inodeCheckSum(clink->u.inode))
						err=-EIO;
				}
			}
		}
		if(err!=0) {
			printf("ps2fs: Error: Disk error partition %ld, block %ld, err %d\n",
				clink->sub, clink->sector, err);
			pfsMount->blockDev->setPartitionError(pfsMount->fd);
			fsckStat(pfsMount, clink->u.superblock, FSCK_STAT_WRITE_ERROR, MODE_SET_FLAG);
			pfsMount->lastError=err;
		}
	}
	clink->flags&=~CACHE_FLAG_DIRTY; // clear dirty :)
	return pfsMount->lastError;
}

void cacheFlushAllDirty(pfs_mount_t *pfsMount)
{
	u32 i;
	int found=0;

	for(i=1;i<numBuffers+1;i++){
		if(cacheBuf[i].pfsMount == pfsMount &&
			cacheBuf[i].flags & CACHE_FLAG_DIRTY)
				found=1;
	}
	if(found) {
		journalWrite(pfsMount, cacheBuf+1, numBuffers);
		for(i=1;i<numBuffers+1;i++){
			if(cacheBuf[i].pfsMount == pfsMount &&
				cacheBuf[i].flags & CACHE_FLAG_DIRTY)
					cacheTransfer(&cacheBuf[i], 1);
		}
		journalReset(pfsMount);
	}
}

pfs_cache_t *cacheAlloc(pfs_mount_t *pfsMount, u16 sub, u32 scale,
					int flags, int *result)
{
	pfs_cache_t *allocated;

	if (cacheBuf->prev==cacheBuf && cacheBuf->prev->next==cacheBuf->prev) {
		printf("ps2fs: Error: Free buffer list is empty\n");
		*result=-ENOMEM;
		return NULL;
	}
	allocated=cacheBuf->next;
	if (cacheBuf->next==NULL)
		printf("ps2fs: Panic: Null pointer allocated\n");
	if (allocated->pfsMount && (allocated->flags & CACHE_FLAG_DIRTY))
		cacheFlushAllDirty(allocated->pfsMount);
	allocated->flags 	= flags & CACHE_FLAG_MASKTYPE;
	allocated->pfsMount	= pfsMount;
	allocated->sub		= sub;
	allocated->sector	= scale;
	allocated->nused	= 1;
	return cacheUnLink(allocated);
}


pfs_cache_t *cacheGetData(pfs_mount_t *pfsMount, u16 sub, u32 sector,
					int flags, int *result)
{
	u32 i;
	pfs_cache_t *clink;

	*result=0;

	for (i=1; i < numBuffers + 1; i++)
		if ( cacheBuf[i].pfsMount &&
		    (cacheBuf[i].pfsMount==pfsMount) &&
		    (cacheBuf[i].sector  == sector))
			if (cacheBuf[i].sub==sub){
				cacheBuf[i].flags &= CACHE_FLAG_MASKSTATUS;
				cacheBuf[i].flags |= flags & CACHE_FLAG_MASKTYPE;
				if (cacheBuf[i].nused == 0)
					cacheUnLink(&cacheBuf[i]);
				cacheBuf[i].nused++;
				return &cacheBuf[i];
			}

	clink=cacheAlloc(pfsMount, sub, sector, flags, result);

	if (clink){
		if (flags & CACHE_FLAG_NOLOAD)
			return clink;

		if ((*result=cacheTransfer(clink, IOCTL2_TMODE_READ))>=0)
			return clink;

		clink->pfsMount=NULL;
		cacheAdd(clink);
	}
	return NULL;
}

pfs_cache_t *cacheAllocClean(int *result)
{
	*result = 0;
	return cacheAlloc(NULL, 0, 0, 0, result);
}

// checks if the cacheBuf list has some room
int cacheIsFull()
{
	if (cacheBuf->prev != cacheBuf)	return 0;
	return cacheBuf->prev->next == cacheBuf->prev;
}

int cacheInit(u32 numBuf, u32 bufSize)
{
	char *cacheData;
	u32 i;

	if(numBuf > 127) {
		printf("ps2fs: Error: Number of buffers larger than 127.\n");
		return -EINVAL;
	}

	cacheData = allocMem(numBuf * bufSize);

	if(!cacheData || !(cacheBuf = allocMem((numBuf + 1) * sizeof(pfs_cache_t))))
		return -ENOMEM;

	numBuffers = numBuf;
	memset(cacheBuf, 0, (numBuf + 1) * sizeof(pfs_cache_t));

	cacheBuf->next = cacheBuf;
	cacheBuf->prev = cacheBuf;

	for(i = 1; i < numBuf + 1; i++)
	{
		cacheBuf[i].u.data = cacheData;
		cacheLink(cacheBuf->prev, &cacheBuf[i]);
		cacheData += bufSize;
	}

	return 0;
}

void cacheMarkClean(pfs_mount_t *pfsMount, u32 subpart, u32 sectorStart, u32 sectorEnd)
{
	u32 i;

	for(i=1; i< numBuffers+1;i++){
		if(cacheBuf[i].pfsMount==pfsMount && cacheBuf[i].sub==subpart) {
			if(cacheBuf[i].sector >= sectorStart && cacheBuf[i].sector < sectorEnd)
				cacheBuf[i].flags&=~CACHE_FLAG_DIRTY;
		}
	}
}
