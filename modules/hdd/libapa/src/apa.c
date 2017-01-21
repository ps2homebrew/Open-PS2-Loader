/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Main APA related routines
*/

#include <errno.h>
#include <iomanX.h>
#include <sysclib.h>
#include <stdio.h>
#include <atad.h>
#include <hdd-ioctl.h>

#include "apa-opt.h"
#include "libapa.h"

const char apaMBRMagic[]="Sony Computer Entertainment Inc.";

void apaSaveError(s32 device, void *buffer, u32 lba, u32 err_lba)
{
	memset(buffer, 0, 512);
	*(u32 *)buffer=err_lba;
	ata_device_sector_io(device, buffer, lba, 1, ATA_DIR_WRITE);
	ata_device_flush_cache(device);
}

void apaSetPartErrorSector(s32 device, u32 lba)
{// used to set the lba of a partition that has a error...
	apa_cache_t *clink;
	clink=apaCacheAlloc();
	apaSaveError(device, clink->header, APA_SECTOR_PART_ERROR, lba);
	apaCacheFree(clink);
}

int apaGetPartErrorSector(s32 device, u32 lba, u32 *lba_out)
{
	apa_cache_t	*clink;
	int			rv=0;

	if(!(clink=apaCacheAlloc()))
		return -ENOMEM;

	if(ata_device_sector_io(device, clink->header, lba, 1, ATA_DIR_READ))
		return -EIO;

	if(lba_out)
		*lba_out=*clink->error_lba;
	if(*clink->error_lba)
		rv=1;// error is set ;)
	apaCacheFree(clink);
	return rv;
}

int apaGetPartErrorName(s32 device, char *name)
{
	u32 lba;
	int rv=0;
	apa_cache_t *clink;

	if((rv=apaGetPartErrorSector(device, APA_SECTOR_PART_ERROR, &lba)) <= 0)
		return rv;
	if(!(clink=apaCacheGetHeader(device, 0, APA_IO_MODE_READ, &rv)))
		return rv;

	while(clink)
	{
		if(clink->header->type!=APA_TYPE_FREE &&
			!(clink->header->flags & APA_CACHE_FLAG_DIRTY) &&
			clink->header->start==lba)
		{
				if(name)
				{
					strncpy(name, clink->header->id, APA_IDMAX - 1);
					name[APA_IDMAX - 1] = '\0';
				}
				apaCacheFree(clink);
				return 1;
		}
		clink=apaGetNextHeader(clink, &rv);
	}

	// clear error if no errors and partitions was not found...
	if(rv==0)
		apaSetPartErrorSector(device, 0);
	return rv;
}

apa_cache_t *apaFillHeader(s32 device, const apa_params_t *params, u32 start, u32 next,
						 u32 prev, u32 length, int *err)
{	// used for making a new partition
	apa_cache_t *clink;

	if(!(clink=apaCacheGetHeader(device, start, APA_IO_MODE_WRITE, err)))
		return NULL;
	memset(clink->header, 0, sizeof(apa_header_t));
	clink->header->magic=APA_MAGIC;
	clink->header->start=start;
	clink->header->next=next;
	clink->header->prev=prev;
	clink->header->length=length;
	clink->header->type=params->type;
	clink->header->flags=params->flags;
	clink->header->modver=APA_MODVER;
	memcpy(clink->header->id, params->id, APA_IDMAX);
	if(params->flags & APA_FLAG_SUB)
	{
		clink->header->main=params->main;
		clink->header->number=params->number;
	}
	else
	{
		if(strncmp(clink->header->id, "_tmp", APA_IDMAX)!=0)
		{
			memcpy(clink->header->rpwd, params->rpwd, APA_PASSMAX);
			memcpy(clink->header->fpwd, params->fpwd, APA_PASSMAX);
		}
	}
	apaGetTime(&clink->header->created);
	clink->flags|=APA_CACHE_FLAG_DIRTY;
	return clink;
}

apa_cache_t *apaInsertPartition(s32 device, const apa_params_t *params, u32 sector, int *err)
{	// Adds a new partition using an empty block.
	apa_cache_t *clink_empty;
	apa_cache_t *clink_this;
	apa_cache_t *clink_next;

	if((clink_this=apaCacheGetHeader(device, sector, APA_IO_MODE_READ, err))==0)
		return 0;

	while(clink_this->header->length!=params->size)
	{
		if((clink_next=apaCacheGetHeader(device, clink_this->header->next, APA_IO_MODE_READ, err))==NULL)
		{	// Get next partition
			apaCacheFree(clink_this);
			return 0;
		}
		clink_this->header->length>>=1;
		clink_empty=apaRemovePartition(device, (clink_this->header->start+clink_this->header->length),
			clink_this->header->next, clink_this->header->start, clink_this->header->length);
		clink_this->header->next=clink_empty->header->start;
		clink_this->flags|=APA_CACHE_FLAG_DIRTY;
		clink_next->header->prev=clink_empty->header->start;
		clink_next->flags|=APA_CACHE_FLAG_DIRTY;

		apaCacheFlushAllDirty(device);
		apaCacheFree(clink_empty);
		apaCacheFree(clink_next);
	}
	apaCacheFree(clink_this);
	clink_this=apaFillHeader(device, params, clink_this->header->start, clink_this->header->next,
		clink_this->header->prev, params->size, err);
	apaCacheFlushAllDirty(device);
	return clink_this;
}

apa_cache_t *apaFindPartition(s32 device, const char *id, int *err)
{
	apa_cache_t *clink;

	clink=apaCacheGetHeader(device, 0, APA_IO_MODE_READ, err);
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
		//return (apa_cache_t *)-ENOENT;		// <-- BUG code tests for NULL only
	}
	*err=0;
	return NULL;
}

void apaAddEmptyBlock(apa_header_t *header, u32 *emptyBlocks)
{	// small helper.... to track empty blocks..
	u32 i;

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

apa_cache_t *apaRemovePartition(s32 device, u32 start, u32 next, u32 prev,
							  u32 length)
{
	apa_cache_t *clink;
	int err;

	if((clink=apaCacheGetHeader(device, start, APA_IO_MODE_WRITE, &err))==NULL)
		return NULL;
	memset(clink->header, 0, sizeof(apa_header_t));
	clink->header->magic=APA_MAGIC;
	clink->header->start=start;
	clink->header->next=next;
	clink->header->prev=prev;
	clink->header->length=length;
	strcpy(clink->header->id, "__empty");
	apaGetTime(&clink->header->created);
	clink->flags|=APA_CACHE_FLAG_DIRTY;
	return clink;
}

void apaMakeEmpty(apa_cache_t *clink)
{
	u32		saved_start;
	u32		saved_next;
	u32		saved_prev;
	u32		saved_length;

	saved_start  = clink->header->start;
	saved_next   = clink->header->next;
	saved_prev   = clink->header->prev;
	saved_length = clink->header->length;
	memset(clink->header, 0, sizeof(apa_header_t));
	clink->header->magic  = APA_MAGIC;
	clink->header->start  = saved_start;
	clink->header->next   = saved_next;
	clink->header->prev   = saved_prev;
	clink->header->length = saved_length;
	apaGetTime(&clink->header->created);
	strcpy(clink->header->id, "__empty");
	clink->flags|=APA_CACHE_FLAG_DIRTY;
}

apa_cache_t *apaDeleteFixPrev(apa_cache_t *clink, int *err)
{
	apa_cache_t		*clink2=clink;
	apa_header_t		*header=clink2->header;
	u32				device=clink->device;
	u32				length=clink->header->length;
	u32				saved_next=clink->header->next;
	u32				saved_length=clink->header->length;
	u32				tmp;

	while(header->start)
	{
		if(!(clink2=apaCacheGetHeader(device, header->prev, APA_IO_MODE_READ, err)))
		{
			apaCacheFree(clink);
			return NULL;
		}
		header=clink2->header;
		tmp=header->length+length;
		if(header->type!=0) {
			apaCacheFree(clink2);
			break;
		}
		if((header->start%tmp) || (tmp & (tmp-1))) {
			apaCacheFree(clink2);
			break;
		}
		length=tmp;
		apaCacheFree(clink);
		clink=clink2;
	}
	if(length!=saved_length)
	{
		if(!(clink2=apaCacheGetHeader(device, saved_next, APA_IO_MODE_READ, err)))
		{
			apaCacheFree(clink);
			return NULL;
		}
		clink->header->length=length;
		clink->header->next=clink->header->start+length;
		clink2->header->prev=clink->header->start;
		clink2->flags|=APA_CACHE_FLAG_DIRTY;
		clink->flags|=APA_CACHE_FLAG_DIRTY;
		apaCacheFlushAllDirty(device);
		apaCacheFree(clink2);
	}
	return clink;
}


apa_cache_t *apaDeleteFixNext(apa_cache_t *clink, int *err)
{
	apa_header_t		*header=clink->header;
	u32				length=header->length;
	u32				saved_length=header->length;
	u32				lnext=header->next;
	apa_cache_t		*clink1;
	apa_cache_t		*clink2;
	u32				device=clink->device;
	u32				tmp;

	while(lnext!=0)
	{
		if(!(clink1=apaCacheGetHeader(device, lnext, APA_IO_MODE_READ, err)))
		{
			apaCacheFree(clink);
			return 0;
		}
		header=clink1->header;
		tmp=header->length+length;
		if(header->type!=0)
		{
			apaCacheFree(clink1);
			break;
		}
		if((clink->header->start%tmp)!=0 || ((tmp-1) & tmp))
		{
			apaCacheFree(clink1);
			break;
		}
		length=tmp;
		apaCacheFree(clink1);
		lnext=header->next;
	}
	if(length!=saved_length)
	{
		if(!(clink2=apaCacheGetHeader(device, lnext, APA_IO_MODE_READ, err)))
		{
			apaCacheFree(clink);
			return NULL;
		}
		clink->header->length=length;
		clink->header->next=lnext;
		apaMakeEmpty(clink);
		clink2->header->prev=clink->header->start;
		clink2->flags|=APA_CACHE_FLAG_DIRTY;
		apaCacheFlushAllDirty(device);
		apaCacheFree(clink2);
	}
	return clink;
}


int apaDelete(apa_cache_t *clink)
{
	int				rv=0;
	apa_cache_t		*clink_mbr;
	u32				device=clink->device;
	u32				start=clink->header->start;
	int				i;

	if(!start) {
		apaCacheFree(clink);
		return -EACCES;
	}

	if(clink->header->next==0)
	{
		if((clink_mbr=apaCacheGetHeader(device, 0, APA_IO_MODE_READ, &rv))==NULL)
		{
			apaCacheFree(clink);
			return rv;
		}
		do {
			apaCacheFree(clink);
			if((clink=apaCacheGetHeader(clink->device, clink->header->prev, APA_IO_MODE_READ, &rv))==NULL)
				return 0;
			clink->header->next=0;
			clink->flags|=APA_CACHE_FLAG_DIRTY;
			clink_mbr->header->prev=clink->header->start;
			clink_mbr->flags|=APA_CACHE_FLAG_DIRTY;
			apaCacheFlushAllDirty(device);
		} while(clink->header->type==0);
		apaCacheFree(clink_mbr);
	} else {
		u32 length=clink->header->length;

		for(i=0;i < 2;i++){
			if((clink=apaDeleteFixPrev(clink, &rv))==NULL)
				return 0;
			if((clink=apaDeleteFixNext(clink, &rv))==NULL)
				return 0;
		}
		if(clink->header->start==start && clink->header->length==length)
		{
			apaMakeEmpty(clink);
			apaCacheFlushAllDirty(clink->device);
		}
	}
	apaCacheFree(clink);
	return rv;
}

int apaCheckSum(apa_header_t *header)
{
	u32 *ptr=(u32 *)header;
	u32 sum, i;

	for(sum=0,i=1; i < 256; i++)	//sizeof(header)/4 = 256, start at offset +4 to omit the checksum field.
		sum+=ptr[i];
	return sum;
}

int apaReadHeader(s32 device, apa_header_t *header, u32 lba)
{
	if(ata_device_sector_io(device, header, lba, 2, ATA_DIR_READ)!=0)
		return -EIO;
	if(header->magic!=APA_MAGIC)
		return -EIO;
	if(apaCheckSum(header)!=header->checksum)
		return -EIO;
	if(lba==APA_SECTOR_MBR)
	{
		if(strncmp(header->mbr.magic, apaMBRMagic, sizeof(header->mbr.magic))==0)
			return 0;
		APA_PRINTF(APA_DRV_NAME": error: invalid partition table or version newer than I know.\n");
		return -EIO;
	}
	return 0;
}

int apaWriteHeader(s32 device, apa_header_t *header, u32 lba)
{
	if(ata_device_sector_io(device, header, lba, 2, ATA_DIR_WRITE))
		return -EIO;
	return 0;
}

int apaGetFormat(s32 device, int *format)
{
	apa_cache_t *clink;
	int rv=0;
	u32 *pDW, i;

	clink=apaCacheAlloc();
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
	apaCacheFree(clink);
	return rv==0;
}

u32 apaGetPartitionMax(u32 totalLBA)
{
	u32 i, size;

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

apa_cache_t *apaGetNextHeader(apa_cache_t *clink, int *err)
{
	u32 start=clink->header->start;

	apaCacheFree(clink);
	if(!clink->header->next)
		return NULL;

	if(!(clink=apaCacheGetHeader(clink->device, clink->header->next, APA_IO_MODE_READ, err)))
		return NULL;

	if(start!=clink->header->prev) {
		APA_PRINTF(APA_DRV_NAME": Warning: Invalid partition information. start != prev\n");
		clink->header->prev=start;
		clink->flags|=APA_CACHE_FLAG_DIRTY;
		apaCacheFlushAllDirty(clink->device);
	}
	return clink;

}
