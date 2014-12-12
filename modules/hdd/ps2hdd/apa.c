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
# Main APA related routines
*/

#include "hdd.h"

void apaSaveError(u32 device, void *buffer, u32 lba, u32 err_lba)
{
	memset(buffer, 0, 512);
	*(u32 *)buffer=err_lba;
	ata_device_sector_io(device, buffer, lba, 1, ATA_DIR_WRITE);
	ata_device_flush_cache(device);
}

void setPartErrorSector(u32 device, u32 lba)
{// used to set the lba of a partition that has a error...
	apa_cache *clink;
	clink=cacheGetFree();
	apaSaveError(device, clink->header, APA_SECTOR_PART_ERROR, lba);
	cacheAdd(clink);
}

int getPartErrorSector(u32 device, u32 lba, int *lba_out)
{
	apa_cache	*clink;
	int			rv=0;

	if(!(clink=cacheGetFree()))
		return -ENOMEM;

	if(ata_device_sector_io(device, clink->header, lba, 1, ATA_DIR_READ))
		return -EIO;

	if(lba_out)
		*lba_out=((u32 *)(clink->header))[0];
	if(((u32 *)(clink->header))[0])
		rv=1;// error is set ;)
	cacheAdd(clink);
	return rv;
}

int getPartErrorName(u32 device, char *name)
{
	int lba;
	int rv=0;
	apa_cache *clink;

	if((rv=getPartErrorSector(device, APA_SECTOR_PART_ERROR, &lba)) <= 0)
		return rv;
	if(!(clink=cacheGetHeader(device, 0, THEADER_MODE_READ, &rv)))
		return rv;

	while(clink)
	{
		if(clink->header->type!=APA_TYPE_FREE &&
			!(clink->header->flags & CACHE_FLAG_DIRTY) &&
			clink->header->start==lba) {
				if(name) {
					strncpy(name, clink->header->id, APA_IDMAX - 1);
					name[APA_IDMAX - 1] = '\0';
				}
				cacheAdd(clink);
				return 1;
		}
		clink=apaGetNextHeader(clink, &rv);
	}

	// clear error if no errors and partitions was not found...
	if(rv==0)
		setPartErrorSector(device, 0);
	return rv;
}

int apaCheckPartitionMax(u32 device, s32 size)
{
	return (hddDeviceBuf[device].partitionMaxSize >= size) ? 0 : -EINVAL;
}

apa_cache *apaFillHeader(u32 device, input_param *params, int start, int next,
						 int prev, int length, int *err)
{	// used for makeing a new partition
	apa_cache *clink;

	if(!(clink=cacheGetHeader(device, start, 1, err)))
		return NULL;
	memset(clink->header, 0, sizeof(apa_header));
	clink->header->magic=APA_MAGIC;
	clink->header->start=start;
	clink->header->next=next;
	clink->header->prev=prev;
	clink->header->length=length;
	clink->header->type=params->type;
	clink->header->flags=params->flags;
	clink->header->modver=APA_MODVER;
	memcpy(&clink->header->id, &params->id, APA_IDMAX);
	if(params->flags & APA_FLAG_SUB) {
		clink->header->main=params->main;
		clink->header->number=params->number;
	}
	else
	{
		if(strncmp(clink->header->id, "_tmp", APA_IDMAX)!=0) {
			memcpy(&clink->header->rpwd, &myPassHash, APA_PASSMAX);
			memcpy(&clink->header->fpwd, &myPassHash, APA_PASSMAX);
		}
	}
	getPs2Time(&clink->header->created);
	clink->flags|=CACHE_FLAG_DIRTY;
	return clink;
}

apa_cache *apaInsertPartition(u32 device, input_param *params, u32 sector, int *err)
{	// add's a new partition useing a empty block...
	apa_cache *clink_empty;
	apa_cache *clink_this;
	apa_cache *clink_next;

	if((clink_this=cacheGetHeader(device, sector, 0, err))==0)
		return 0;
	while(clink_this->header->length!=params->size) {
		if((clink_next=cacheGetHeader(device, clink_this->header->next, 0, err))==NULL) { // get next
			cacheAdd(clink_this);
			return 0;
		}
		clink_this->header->length>>=1;
		clink_empty=apaRemovePartition(device, (clink_this->header->start+clink_this->header->length),
			clink_this->header->next, clink_this->header->start, clink_this->header->length);
		clink_this->header->next=clink_empty->header->start;
		clink_this->flags|=CACHE_FLAG_DIRTY;
		clink_next->header->prev=clink_empty->header->start;
		clink_next->flags|=CACHE_FLAG_DIRTY;

		cacheFlushAllDirty(device);
		cacheAdd(clink_empty);
		cacheAdd(clink_next);
	}
	cacheAdd(clink_this);
	clink_this=apaFillHeader(device, params, clink_this->header->start, clink_this->header->next,
		clink_this->header->prev, params->size, err);
	cacheFlushAllDirty(device);
	return clink_this;
}

apa_cache *apaFindPartition(u32 device, char *id, int *err)
{
	apa_cache *clink;

	clink=cacheGetHeader(device, 0, 0, err);
	while(clink)
	{
		if(!(clink->header->flags & APA_FLAG_SUB)) {
			if(memcmp(clink->header->id, id, APA_IDMAX)==0)
				return clink;	// found
		}
		clink=apaGetNextHeader(clink, (int *)err);
	}
	if(*err==0) {
		*err=-ENOENT;
		return NULL;
		//return (apa_cache *)-ENOENT;		// <-- BUG code tests for NULL only
	}
	*err=0;
	return NULL;
}

void addEmptyBlock(apa_header *header, u32 *emptyBlocks)
{	// small helper.... to track empty blocks..
	int i;

	if(header->type==APA_TYPE_FREE) {
		for(i=0;i<32;i++)
		{
			if(header->length==(1 << i)) {
				if(emptyBlocks[i]==APA_TYPE_FREE) {
					emptyBlocks[i]=header->start;
					return;
				}
			}
		}
	}
}


apa_cache *apaAddPartitionHere(u32 device, input_param *params, u32 *emptyBlocks,
				u32 sector, int *err)
{
	apa_cache	*clink_this;
	apa_cache	*clink_next;
	apa_cache	*clink_new;
	apa_header	*header;
	u32			i;
	u32			tmp, some_size, part_end;
	u32			tempSize;

	// walk empty blocks in case can use one :)
	for(i=0;i< 32;i++)
	{
		if((1 << i) >= params->size && emptyBlocks[i]!=0)
			return apaInsertPartition(device, params, emptyBlocks[i], err);
	}
	clink_this=cacheGetHeader(device, sector, 0, err);
	header=clink_this->header;
	part_end=header->start+header->length;
	some_size=(part_end%params->size);
	tmp = some_size ? params->size - some_size : 0;

	if(hddDeviceBuf[device].totalLBA < (part_end+params->size+tmp))
	{
		*err=-ENOSPC;
		cacheAdd(clink_this);
		return NULL;
	}

	if((clink_next=cacheGetHeader(device, 0, 0, err))==NULL){
		cacheAdd(clink_this);
		return NULL;
	}

	tempSize=params->size;
	while(part_end%params->size){
		tempSize=params->size>>1;
		while(0x3FFFF<tempSize){
			if(!(part_end%tempSize)) {
				clink_new=apaRemovePartition(device, part_end, 0,
					clink_this->header->start, tempSize);
				clink_this->header->next=part_end;
				clink_this->flags|=CACHE_FLAG_DIRTY;
				clink_next->header->prev=clink_new->header->start;
				part_end+=tempSize;
				clink_next->flags|=CACHE_FLAG_DIRTY;
				cacheFlushAllDirty(device);
				cacheAdd(clink_this);
				clink_this=clink_new;
				break;
			}
			tempSize>>=1;
		}
	}
	if((clink_new=apaFillHeader(device, params, part_end, 0, clink_this->header->start,
		params->size, err))!=NULL) {
			clink_this->header->next=part_end;
			clink_this->flags|=CACHE_FLAG_DIRTY;
			clink_next->header->prev=clink_new->header->start;
			clink_next->flags|=CACHE_FLAG_DIRTY;
			cacheFlushAllDirty(device);
		}
		cacheAdd(clink_this);
		cacheAdd(clink_next);
		return clink_new;
}


int apaOpen(u32 device, hdd_file_slot_t *fileSlot, input_param *params, int mode)
{
	int				rv=0;
	u32				emptyBlocks[32];
	apa_cache		*clink;
	apa_cache		*clink2;
	u32				sector=0;


	// walk all looking for any empty blocks & look for partition
	clink=cacheGetHeader(device, 0, 0, &rv);
	memset(&emptyBlocks, 0, sizeof(emptyBlocks));
	while(clink)
	{
		sector=clink->sector;
		if(!(clink->header->flags & APA_FLAG_SUB)) {
			if(memcmp(clink->header->id, params->id, APA_IDMAX) == 0)
				break;	// found :)
		}
		addEmptyBlock(clink->header, emptyBlocks);
		clink=apaGetNextHeader(clink, &rv);
	}

	if(rv!=0)
		return rv;
	rv=-ENOENT;

	if(clink==NULL && (mode & O_CREAT))
	{
		if((rv=apaCheckPartitionMax(device, params->size))>=0) {
			if((clink=apaAddPartitionHere(device, params, emptyBlocks, sector, &rv))!=NULL)
			{
				sector=clink->header->start;
				clink2=cacheGetFree();
				memset(clink2->header, 0, sizeof(apa_header));
				ata_device_sector_io(device, clink2->header, sector+8     , 2, ATA_DIR_WRITE);
				ata_device_sector_io(device, clink2->header, sector+0x2000, 2, ATA_DIR_WRITE);
				cacheAdd(clink2);
			}
		}
	}
	if(clink==NULL)
		return rv;
	fileSlot->parts[0].start=clink->header->start;
	fileSlot->parts[0].length=clink->header->length;
	memcpy(&fileSlot->parts[1], &clink->header->subs, APA_MAXSUB*sizeof(apa_subs));
	fileSlot->type=clink->header->type;
	fileSlot->nsub=clink->header->nsub;
	memcpy(&fileSlot->id, &clink->header->id, APA_IDMAX);
	cacheAdd(clink);
	rv=0; if(passcmp(clink->header->fpwd, NULL)!=0)
		rv=-EACCES;
	return rv;
}

int apaRemove(u32 device, char *id)
{
	int			i;
	u32			nsub;
	apa_cache	*clink;
	apa_cache	*clink2;
	int			rv;

	for(i=0;i<maxOpen;i++)	// look to see if open
	{
		if(fileSlots[i].f!=0) {
			if(memcmp(fileSlots[i].id, id, APA_IDMAX)==0)
				return -EBUSY;
		}
	}
	if(id[0]=='_' && id[1]=='_')
		return -EACCES;
	if((clink=apaFindPartition(device, id, &rv))==NULL)
		return rv;
	if(passcmp(clink->header->fpwd, NULL))
	{
		cacheAdd(clink);
		return -EACCES;
	}
	// remove all subs frist...
	nsub=clink->header->nsub;
	clink->header->nsub=0;
	clink->flags|=CACHE_FLAG_DIRTY;
	cacheFlushAllDirty(device);
	for(i=nsub-1;i!=-1;i--)
	{
		if((clink2=cacheGetHeader(device, clink->header->subs[i].start, 0, &rv))){
			if((rv=apaDelete(clink2))){
				cacheAdd(clink);
				return rv;
			}
		}
	}
	if(rv==0)
		return apaDelete(clink);

	cacheAdd(clink);
	return rv;
}

apa_cache *apaRemovePartition(u32 device, u32 start, u32 next, u32 prev,
							  u32 length)
{
	apa_cache *clink;
	u32 err;

	if((clink=cacheGetHeader(device, start, 1, (int *)&err))==NULL)
		return NULL;
	memset(clink->header, 0, sizeof(apa_header));
	clink->header->magic=APA_MAGIC;
	clink->header->start=start;
	clink->header->next=next;
	clink->header->prev=prev;
	clink->header->length=length;
	strcpy(clink->header->id, "__empty");
	getPs2Time(&clink->header->created);
	clink->flags|=CACHE_FLAG_DIRTY;
	return clink;
}

void apaMakeEmpty(apa_cache *clink)
{
	u32		saved_start;
	u32		saved_next;
	u32		saved_prev;
	u32		saved_length;

	saved_start  = clink->header->start;
	saved_next   = clink->header->next;
	saved_prev   = clink->header->prev;
	saved_length = clink->header->length;
	memset(clink->header, 0, sizeof(apa_header));
	clink->header->magic  = APA_MAGIC;
	clink->header->start  = saved_start;
	clink->header->next   = saved_next;
	clink->header->prev   = saved_prev;
	clink->header->length = saved_length;
	getPs2Time(&clink->header->created);
	strcpy(clink->header->id, "__empty");
	clink->flags|=CACHE_FLAG_DIRTY;
}

apa_cache *apaDeleteFixPrev(apa_cache *clink, int *err)
{
	apa_cache		*clink2=clink;
	apa_header		*header=clink2->header;
	u32				device=clink->device;
	u32				length=clink->header->length;
	u32				saved_next=clink->header->next;
	u32				saved_length=clink->header->length;
	u32				tmp;


	while(header->start) {
		if(!(clink2=cacheGetHeader(device, header->prev, 0, err))) {
			cacheAdd(clink);
			return NULL;
		}
		header=clink2->header;
		tmp=header->length+length;
		if(header->type!=0) {
			cacheAdd(clink2);
			break;
		}
		if((header->start%tmp) || (tmp & (tmp-1))) {
			cacheAdd(clink2);
			break;
		}
		length=tmp;
		cacheAdd(clink);
		clink=clink2;
	}
	if(length!=saved_length) {
		if(!(clink2=cacheGetHeader(device, saved_next, 0, err))) {
			cacheAdd(clink);
			return NULL;
		}
		clink->header->length=length;
		clink->header->next=clink->header->start+length;
		clink2->header->prev=clink->header->start;
		clink2->flags|=CACHE_FLAG_DIRTY;
		clink->flags|=CACHE_FLAG_DIRTY;
		cacheFlushAllDirty(device);
		cacheAdd(clink2);
	}
	return clink;
}


apa_cache *apaDeleteFixNext(apa_cache *clink, int *err)
{
	apa_header		*header=clink->header;
	u32				length=header->length;
	u32				saved_length=header->length;
	u32				lnext=header->next;
	apa_cache		*clink1;
	apa_cache		*clink2;
	u32				device=clink->device;
	u32				tmp;

	while(lnext!=0)
	{
		if(!(clink1=cacheGetHeader(device, lnext, 0, err))) {
			cacheAdd(clink);
			return 0;
		}
		header=clink1->header;
		tmp=header->length+length;
		if(header->type!=0) {
			cacheAdd(clink1);
			break;
		}
		if((clink->header->start%tmp)!=0 || ((tmp-1) & tmp)) {
			cacheAdd(clink1);
			break;
		}
		length=tmp;
		cacheAdd(clink1);
		lnext=header->next;
	}
	if(length!=saved_length) {
		if(!(clink2=cacheGetHeader(device, lnext, 0, err))) {
			cacheAdd(clink);
			return NULL;
		}
		clink->header->length=length;
		clink->header->next=lnext;
		apaMakeEmpty(clink);
		clink2->header->prev=clink->header->start;
		clink2->flags|=CACHE_FLAG_DIRTY;
		cacheFlushAllDirty(device);
		cacheAdd(clink2);
	}
	return clink;
}


int apaDelete(apa_cache *clink)
{
	int				rv=0;
	apa_cache		*clink_mbr;
	u32				device=clink->device;
	u32				start=clink->header->start;
	int				i;

	if(!start) {
		cacheAdd(clink);
		return -EACCES;
	}

	if(clink->header->next==0) {
		if((clink_mbr=cacheGetHeader(device, 0, 0, &rv))==NULL)
		{
			cacheAdd(clink);
			return rv;
		}
		do {
			cacheAdd(clink);
			if((clink=cacheGetHeader(clink->device, clink->header->prev, 0, &rv))==NULL)
				return 0;
			clink->header->next=0;
			clink->flags|=CACHE_FLAG_DIRTY;
			clink_mbr->header->prev=clink->header->start;
			clink_mbr->flags|=CACHE_FLAG_DIRTY;
			cacheFlushAllDirty(device);
		} while(clink->header->type==0);
		cacheAdd(clink_mbr);
	} else {
		u32 length=clink->header->length;

		for(i=0;i < 2;i++){
			if((clink=apaDeleteFixPrev(clink, &rv))==NULL)
				return 0;
			if((clink=apaDeleteFixNext(clink, &rv))==NULL)
				return 0;
		}
		if(clink->header->start==start && clink->header->length==length) {
			apaMakeEmpty(clink);
			cacheFlushAllDirty(clink->device);
		}
	}
	cacheAdd(clink);
	return rv;
}

int apaCheckSum(apa_header *header)
{
	u32 *ptr=(u32 *)header;
	u32 sum=0;
	int i;

	for(i=1; i < 256; i++)
		sum+=ptr[i];
	return sum;
}

int apaReadHeader(u32 device, apa_header *header, u32 lba)
{
	if(ata_device_sector_io(device, header, lba, 2, ATA_DIR_READ)!=0)
		return -EIO;
	if(header->magic!=APA_MAGIC)
		return -EIO;
	if(apaCheckSum(header)!=header->checksum)
		return -EIO;
	if(lba==APA_SECTOR_MBR)
	{
		if(strncmp(header->mbr.magic, mbrMagic, 0x20)==0)
			return 0;
		dprintf1("ps2hdd: Error: invalid partition table or version newer than I know.\n");
		return -EIO;
	}
	return 0;
}


int apaWriteHeader(u32 device, apa_header *header, u32 lba)
{
	if(ata_device_sector_io(device, header, lba, 2, ATA_DIR_WRITE))
		return -EIO;
	return 0;
}


int apaGetFormat(u32 device, int *format)
{
	apa_cache *clink;
	int rv=0;// u32 rv=0;
	u32 *pDW;
	int i;

	clink=cacheGetFree();
	*format=0;
	if((rv=apaReadHeader(device, clink->header, 0))==0)
	{
		*format=clink->header->mbr.version;
		if(ata_device_sector_io(device, clink->header, APA_SECTOR_SECTOR_ERROR, 2, ATA_DIR_READ))
			rv=-EIO; // return -EIO;
		if(rv==0){
			pDW=(u32 *)clink->header;
			for(i=0;i < 256; i++)
			{
				if((i & 0x7F) && pDW[i]!=0)
					rv=1;
			}
		}
	}
	cacheAdd(clink);
	return rv==0;
}

int apaGetPartitionMax(int totalLBA)
{
	int i;
	int size;

	totalLBA>>=6; // totalLBA/64
	size=(1<<0x1F);
	for(i=31;i!=0;i--)
	{
		size=1<<i;
		if(size&totalLBA)
			break;
	}
	if(size<totalLBA)
		i++;
	return(1 << i);
}

apa_cache *apaGetNextHeader(apa_cache *clink, int *err)
{
	u32 start=clink->header->start;

	cacheAdd(clink);
	if(!clink->header->next)
		return NULL;

	if(!(clink=cacheGetHeader(clink->device, clink->header->next, 0, err)))
		return NULL;

	if(start!=clink->header->prev) {
		dprintf1("ps2hdd: Warning: Invalid partition information. start != prev\n");
		clink->header->prev=start;
		clink->flags|=CACHE_FLAG_DIRTY;
		cacheFlushAllDirty(clink->device);
	}
	return clink;

}
