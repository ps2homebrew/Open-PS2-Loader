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
# APA cache manipulation routines
*/

#include "hdd.h"

//  Globals
apa_cache		*cacheBuf;
int				cacheSize;

int cacheInit(u32 size)
{
	apa_header *header;
	int i;

	cacheSize=size;	// save size ;)
	if((header=(apa_header *)allocMem(size*sizeof(apa_header)))){
		cacheBuf=allocMem((size+1)*sizeof(apa_cache));
		if(cacheBuf==NULL)
			return -ENOMEM;
	}
	else
		return -ENOMEM;
	// setup cache header...
	memset(cacheBuf, 0, (size+1)*sizeof(apa_cache));
	cacheBuf->next=cacheBuf;
	cacheBuf->tail=cacheBuf;
	for(i=1; i<size+1;i++, header++){
		cacheBuf[i].header=header;
		cacheBuf[i].device=-1;
		cacheLink(cacheBuf->tail, &cacheBuf[i]);
	}
	return 0;
}

void cacheLink(apa_cache *clink, apa_cache *cnew)
{
	cnew->tail=clink;
	cnew->next=clink->next;
	clink->next->tail=cnew;
	clink->next=cnew;
}

apa_cache *cacheUnLink(apa_cache *clink)
{
	clink->tail->next=clink->next;
	clink->next->tail=clink->tail;
	return clink;
}

int cacheTransfer(apa_cache *clink, int type)
{
	int err;
	if(type)
		err=apaWriteHeader(clink->device, clink->header, clink->sector);
	else// 0
		err=apaReadHeader(clink->device, clink->header, clink->sector);

	if(err)
	{
		dprintf1("ps2hdd: Error: disk err %d on device %ld, sector %ld, type %d\n",
			err, clink->device, clink->sector, type);
		if(type==0)// save any read error's..
			apaSaveError(clink->device, clink->header, APA_SECTOR_SECTOR_ERROR, clink->sector);
	}
	clink->flags&=~CACHE_FLAG_DIRTY;
	return err;
}

void cacheFlushDirty(apa_cache *clink)
{
	if(clink->flags&CACHE_FLAG_DIRTY)
		cacheTransfer(clink, THEADER_MODE_WRITE);
}

int cacheFlushAllDirty(u32 device)
{
	u32 i;
	// flush apal
	for(i=1;i<cacheSize+1;i++){
		if((cacheBuf[i].flags & CACHE_FLAG_DIRTY) && cacheBuf[i].device==device)
			journalWrite(&cacheBuf[i]);
	}
	journalFlush(device);
	// flush apa
	for(i=1;i<cacheSize+1;i++){
		if((cacheBuf[i].flags & CACHE_FLAG_DIRTY) && cacheBuf[i].device==device)
			cacheTransfer(&cacheBuf[i], THEADER_MODE_WRITE);
	}
	return journalReset(device);
}

apa_cache *cacheGetHeader(u32 device, u32 sector, u32 mode, int *result)
{
	apa_cache *clink=NULL;
	int i;

	*result=0;
	for(i=1;i<cacheSize+1;i++){
		if(cacheBuf[i].sector==sector &&
			cacheBuf[i].device==device){
				clink=&cacheBuf[i];
				break;
			}
	}
	if(clink!=NULL) {
		// cached ver was found :)
		if(clink->nused==0)
			clink=cacheUnLink(clink);
		clink->nused++;
		return clink;
	}
	if((cacheBuf->tail==cacheBuf) &&
		(cacheBuf->tail==cacheBuf->tail->next)){
			dprintf1("ps2hdd: error: free buffer empty\n");
		}
	else
	{
		clink=cacheBuf->next;
		if(clink->flags & CACHE_FLAG_DIRTY)
			dprintf1("ps2hdd: error: dirty buffer allocated\n");
		clink->flags=0;
		clink->nused=1;
		clink->device=device;
		clink->sector=sector;
		clink=cacheUnLink(clink);
	}
	if(clink==NULL)
	{
		*result=-ENOMEM;
		return NULL;
	}
	if(!mode)
	{
		if((*result=cacheTransfer(clink, THEADER_MODE_READ))<0){
			clink->nused=0;
			clink->device=-1;
			cacheLink(cacheBuf, clink);
			clink=NULL;
		}
	}
	return clink;
}

void cacheAdd(apa_cache *clink)
{
	if(clink==NULL){
		dprintf1("ps2hdd: Error: null buffer returned\n");
		return;
	}
	if(clink->nused==0){
		dprintf1("ps2hdd: Error: unused cache returned\n");
		return;
	}
	if(clink->flags & CACHE_FLAG_DIRTY)
		dprintf1("ps2hdd: Error: dirty buffer returned\n");
	clink->nused--;
	if(clink->nused==0)
		cacheLink(cacheBuf->tail, clink);
	return;
}

apa_cache *cacheGetFree()
{
	apa_cache *cnext;

	if((cacheBuf->tail==cacheBuf) &&
		(cacheBuf->tail==cacheBuf->tail->next)){
			dprintf1("ps2hdd: Error: free buffer empty\n");
			return NULL;
		}
	cnext=cacheBuf->next;
	if(cnext->flags & CACHE_FLAG_DIRTY)
		dprintf1("ps2hdd: Error: dirty buffer allocated\n");
	cnext->nused=1;
	cnext->flags=0;
	cnext->device=(u32)-1;
	cnext->sector=-1;
	return cacheUnLink(cnext);
}
