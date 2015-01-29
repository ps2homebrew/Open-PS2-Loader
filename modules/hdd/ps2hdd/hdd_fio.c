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
# APA File System routines
*/

#include "hdd.h"

hdd_file_slot_t	*fileSlots;
int				fioSema;

static const char *formatPartList[]={
	"__net", "__system", "__sysconf", "__common", NULL
};

#define APA_NUMBER_OF_SIZES 9
static const char *sizeList[APA_NUMBER_OF_SIZES]={
	"128M", "256M", "512M", "1G", "2G", "4G", "8G", "16G", "32G"
};

int fioPartitionSizeLookUp(char *str)
{
	int	i;

	for(i=0;i<APA_NUMBER_OF_SIZES;i++){
		if(strcmp(str, sizeList[i])==0)
			return (256*1024) << i;
	}
	dprintf1("ps2hdd: Error: Invalid partition size, %s.\n", str);
	return -EINVAL;
}

int fioInputBreaker(char const **arg, char *outBuf, int maxout)
{
	u32 len;
	char *p;

	if((p=strchr(arg[0], ','))) {
		if(maxout < (len=p-arg[0]))
			return -EINVAL;
		memcpy(outBuf, arg[0], len);
		arg[0]=p+1;
		while(arg[0][0]==' ') arg[0]+=1;
		return 0;
	}// else
	if(maxout < (len=strlen(arg[0])))
		return -EINVAL;
	memcpy(outBuf, arg[0], len); arg[0]+=len;
	return 0;
}

// NOTE: Changed so format = partitionID,size (used to be partitionID,fpswd,rpswd,size,filesystem)
int fioGetInput(const char *arg, input_param *params)
{
	char	szBuf[32];
	int		rv=0;

	if(params==NULL)
		return -EINVAL;
	memset(params, 0, sizeof(input_param));

	while(arg[0]==' ') arg++;

	if(arg[0]==0 || arg[0]==',')
		return -EINVAL;
	if((rv=fioInputBreaker(&arg, params->id, APA_IDMAX))!=0)
		return rv;
	if((params->id[0]==0) || (arg[0]==0))
		return 0;

	memset(szBuf, 0, sizeof(szBuf));
	if((rv=fioInputBreaker(&arg, szBuf, sizeof(szBuf)))!=0)
		return rv;

	if((rv=fioPartitionSizeLookUp(szBuf))<0)
		return rv;
	params->size=rv;

	// Filesystem type is fixed to PFS!
	params->type = APA_TYPE_PFS;
	return rv;
}

int getFileSlot(input_param *params, hdd_file_slot_t **fileSlot)
{
	int i;

	for(i=0;i<maxOpen;i++)
	{
		if(fileSlots[i].f)
			if(memcmp(fileSlots[i].id, &params->id, APA_IDMAX)==0)
				return -EBUSY;// file is open
	}
	for(i=0;i<maxOpen;i++)
	{
		if(!fileSlots[i].f){
			*fileSlot=&fileSlots[i];
			return 0;
		}
	}
	return -EMFILE;// no file slots free :(
}

int fioDataTransfer(iop_file_t *f, void *buf, int size, int mode)
{
	hdd_file_slot_t *fileSlot=(hdd_file_slot_t *)f->privdata;

	if((size & 0x1FF))
		return -EINVAL;
	size>>=9;	// size/512

	if(fileSlot->post+size>=0x1FF9)// no over reading
		size=0x1FF8-fileSlot->post;

	if(size!=0) {
		int rv=0;

		WaitSema(fioSema);
		if(ata_device_sector_io(f->unit, buf, fileSlot->post+fileSlot->parts[0].start+8, size, mode))
			rv=-EIO;
		SignalSema(fioSema);
		if(rv==0)
		{
			fileSlot->post+=size;
			return size<<9;
		}
	}
	return 0;
}

int ioctl2Transfer(u32 device, hdd_file_slot_t *fileSlot, hddIoctl2Transfer_t *arg)
{
	if(fileSlot->nsub<arg->sub)
		return -ENODEV;

	// main partitions can only be read starting from the 4MB offset.
	if(arg->sub==0 && (arg->sector < 0x2000))
		return -EINVAL;
	 // sub-partitions can only be read starting from after the header.
	if(arg->sub!=0 && (arg->sector < 2))
		return -EINVAL;

	if(fileSlot->parts[arg->sub].length<arg->sector+arg->size)
		return -ENXIO;

	if(ata_device_sector_io(device, arg->buffer,
		fileSlot->parts[arg->sub].start+arg->sector, arg->size, arg->mode))
		return -EIO;

	return 0;
}

void hddPowerOffHandler(void* data)
{
	printf("hdd flush cache\n");
	ata_device_flush_cache(0);
}

int hddInit(iop_device_t *f)
{
	iop_sema_t sema;

	sema.attr=1;
	sema.initial=1;
	sema.max=1;
	sema.option=0;
	fioSema=CreateSema(&sema);

	AddPowerOffHandler(hddPowerOffHandler, NULL);
	return 0;
}

int hddDeinit(iop_device_t *f)
{
	DeleteSema(fioSema);
	return 0;
}

int hddFormat(iop_file_t *f, const char *dev, const char *blockdev, void *arg, size_t arglen)
{
	int				rv=0;
	apa_cache		*clink;
	int				i;
	input_param		params;
	u32				emptyBlocks[32];

	if(f->unit >= 2)
		return -ENXIO;

	// clear all errors on hdd
	clink=cacheGetFree();
	memset(clink->header, 0, sizeof(apa_header));
	if(ata_device_sector_io(f->unit, clink->header, APA_SECTOR_SECTOR_ERROR, 1, ATA_DIR_WRITE)){
		cacheAdd(clink);
		return -EIO;
	}
	if(ata_device_sector_io(f->unit, clink->header, APA_SECTOR_PART_ERROR, 1, ATA_DIR_WRITE)){
		cacheAdd(clink);
		return -EIO;
	}
	// clear apa headers
	for(i=1024*8;i<hddDeviceBuf[f->unit].totalLBA;i+=(1024*256))
	{
		ata_device_sector_io(f->unit, clink->header, i, sizeof(apa_header)/512,
			ATA_DIR_WRITE);
	}
	cacheAdd(clink);
	if((rv=journalReset(f->unit))!=0)
		return rv;

	// set up mbr :)
	if((clink=cacheGetHeader(f->unit, 0, 1, &rv))){
		apa_header *header=clink->header;
		memset(header, 0, sizeof(apa_header));
		header->magic=APA_MAGIC;
		header->length=(1024*256);	// 128MB
		header->type=APA_TYPE_MBR;
		strcpy(header->id,"__mbr");
		memcpy(header->mbr.magic, mbrMagic, 32);

		header->mbr.version=APA_MBR_VERSION;
		header->mbr.nsector=0;
		getPs2Time(&header->created);
		getPs2Time(&header->mbr.created);
		header->checksum=apaCheckSum(header);
		clink->flags|=CACHE_FLAG_DIRTY;
		cacheFlushDirty(clink);
		ata_device_flush_cache(f->unit);
		cacheAdd(clink);
		hddDeviceBuf[f->unit].status=0;
		hddDeviceBuf[f->unit].format=APA_MBR_VERSION;
	}
	memset(&emptyBlocks, 0, sizeof(emptyBlocks));
	memset(&params, 0, sizeof(input_param));
	params.size=(1024*256);
	params.type=APA_TYPE_PFS;

	// add __net, __system....
	for(i=0;formatPartList[i];i++)
	{
		memset(params.id, 0, APA_IDMAX);
		strcpy(params.id, formatPartList[i]);
		if(!(clink=apaAddPartitionHere(f->unit, &params, emptyBlocks, i ? clink->sector : 0, &rv)))
			return rv;
		cacheAdd(clink);

		params.size<<=1;
		if(hddDeviceBuf[f->unit].partitionMaxSize < params.size)
			params.size=hddDeviceBuf[f->unit].partitionMaxSize;
	}
	return rv;
}

int hddRemove(iop_file_t *f, const char *name)
{
	int			rv;
	input_param	params;

	if((rv=fioGetInput(name, &params)) < 0)
		return rv;

	WaitSema(fioSema);
	apaRemove(f->unit, params.id);
	return SignalSema(fioSema);
}

int hddOpen(iop_file_t *f, const char *name, int mode, int other_mode)
{
	int			rv;
	input_param	params;
	hdd_file_slot_t *fileSlot;

	if(f->unit >= 2 || hddDeviceBuf[f->unit].status!=0)
		return -ENODEV;

	if(!(f->mode & O_DIROPEN))
		if((rv=fioGetInput(name, &params)) < 0)
			return rv;

	WaitSema(fioSema);
	if((rv=getFileSlot(&params, &fileSlot))==0) {
		if(!(f->mode & O_DIROPEN)) {
			if((rv=apaOpen(f->unit, fileSlot, &params, mode))==0){
				fileSlot->f=f;
				f->privdata=fileSlot;
			}
		}
		else
		{
			fileSlot->f=f;
			f->privdata=fileSlot;
		}
	}
	SignalSema(fioSema);
	return rv;
}

int hddClose(iop_file_t *f)
{
	WaitSema(fioSema);
	memset(f->privdata, 0, sizeof(hdd_file_slot_t));
	SignalSema(fioSema);
	return 0;
}

int hddRead(iop_file_t *f, void *buf, int size)
{
	return fioDataTransfer(f, buf, size, ATA_DIR_READ);
}

int hddWrite(iop_file_t *f, void *buf, int size)
{
	if(!(f->mode & O_WRONLY))
		return -EACCES;
	return fioDataTransfer(f, buf, size, ATA_DIR_WRITE);
}

int hddLseek(iop_file_t *f, int post, int whence)
{
	int 	rv=0;
	hdd_file_slot_t *fileSlot;

	// test input( no seeking to end point less :P )
	if(whence==SEEK_END)
		return -EINVAL;
	if((post & 0x1FF))
		return -EINVAL;

	post>>=9;// post/512

	WaitSema(fioSema);
	fileSlot=f->privdata;
	if(whence==SEEK_CUR)
	{
		if((fileSlot->post+post) < 0 || (fileSlot->post+post)>=0x1FF9)
			rv=-EINVAL;
		else
		{
			fileSlot->post+=post;
			rv=fileSlot->post<<9;
		}
	}
	else if(whence==SEEK_SET)
	{
		if(post < 0 || post>=0x1FF9)
			rv=-EINVAL;
		else
		{
			fileSlot->post=post;
			rv=fileSlot->post<<9;
		}

	}
	SignalSema(fioSema);
	return rv;
}

void fioGetStatFiller(apa_cache *clink, iox_stat_t *stat)
{
	apa_header *header;
	u64 size;

	stat->mode=clink->header->type;
	stat->attr=clink->header->flags;
	stat->hisize=0;
	size = clink->header->length;
	size *= 512;
	stat->size=size & 0xFFFFFFFF;
	size >>= 32;
	stat->hisize=size & 0xFFFFFFFF;
	header=clink->header;
	memcpy(&stat->ctime, &clink->header->created, sizeof(ps2time));
	memcpy(&stat->atime, &clink->header->created, sizeof(ps2time));
	memcpy(&stat->mtime, &clink->header->created, sizeof(ps2time));
	if(clink->header->flags & APA_FLAG_SUB)
		stat->private_0=clink->header->number;
	else
		stat->private_0=clink->header->nsub;
	stat->private_1=0;
	stat->private_2=0;
	stat->private_3=0;
	stat->private_4=0;
	//stat->private_5=0;// game ver
	stat->private_5=clink->header->start;// sony ver
}

int hddGetStat(iop_file_t *f, const char *name, iox_stat_t *stat)
{
	apa_cache	*clink;
	input_param	params;
	int			rv;

	if((rv=fioGetInput(name, &params))<0)
		return rv;

	WaitSema(fioSema);
	if((clink=apaFindPartition(f->unit, params.id, &rv))){
		if((rv=passcmp(clink->header->rpwd, NULL))==0)
				fioGetStatFiller(clink, stat);
		cacheAdd(clink);
	}
	SignalSema(fioSema);
	return rv;
}

int hddDopen(iop_file_t *f, const char *name)
{
    return hddOpen(f, name, 0, 0);
}

int hddDread(iop_file_t *f, iox_dirent_t *dirent)
{
	int				rv;
	hdd_file_slot_t *fileSlot=f->privdata;
	apa_cache 		*clink;

	if(!(f->mode & O_DIROPEN))
		return -ENOTDIR;

	if(fileSlot->parts[0].start==-1)
		return 0;// end :)

	WaitSema(fioSema);
	if((clink=cacheGetHeader(f->unit, fileSlot->parts[0].start, 0, &rv)) &&
		clink->header->length)
	{
		if(clink->header->flags & APA_FLAG_SUB) {
			// if sub get id from main header...
			apa_cache *cmain=cacheGetHeader(f->unit, clink->header->main, 0, &rv);
			if(cmain!=NULL){
				rv=strlen(cmain->header->id);
				strcpy(dirent->name, cmain->header->id);
				cacheAdd(cmain);
			}
		}
		else {
			rv=strlen(clink->header->id);
			strcpy(dirent->name, clink->header->id);
		}
		fioGetStatFiller(clink, &dirent->stat);
		if(clink->header->next==0)
			fileSlot->parts[0].start=-1;		// mark end
		else
			fileSlot->parts[0].start=clink->header->next;// set next
		cacheAdd(clink);
	}
	SignalSema(fioSema);
	return rv;
}

int hddReName(iop_file_t *f, const char *oldname, const char *newname)
{
	int i, rv;
	apa_cache *clink;
	char tmpBuf[APA_IDMAX];

	if(f->unit >= 2 || hddDeviceBuf[f->unit].status!=0)
		return -ENODEV;// No such device

	WaitSema(fioSema);
	// look to see if can make(newname) or not...
	memset(tmpBuf, 0, APA_IDMAX);
	strncpy(tmpBuf, newname, APA_IDMAX - 1);
	tmpBuf[APA_IDMAX - 1] = '\0';
	if((clink=apaFindPartition(f->unit, tmpBuf, &rv))){
		cacheAdd(clink);
		SignalSema(fioSema);
		return -EEXIST;	// File exists
	}

	// look to see if open(oldname)
	memset(tmpBuf, 0, APA_IDMAX);
	strncpy(tmpBuf, oldname, APA_IDMAX - 1);
	tmpBuf[APA_IDMAX - 1] = '\0';
	for(i=0;i<maxOpen;i++)
	{
		if(fileSlots[i].f!=0)
			if(fileSlots[i].f->unit==f->unit)
				if(memcmp(fileSlots[i].id, oldname, APA_IDMAX)==0)
				{
					SignalSema(fioSema);
					return -EBUSY;
				}
	}

	// find :)
	if(!(clink=apaFindPartition(f->unit, tmpBuf, &rv)))
	{
		SignalSema(fioSema);
		return -ENOENT;
	}

	// do the renaming :) note: subs have no names!!
	memset(clink->header->id, 0, APA_IDMAX);		// all cmp are done with memcmp!
	strncpy(clink->header->id, newname, APA_IDMAX - 1);
	clink->header->id[APA_IDMAX - 1] = '\0';
	clink->flags|=CACHE_FLAG_DIRTY;

	cacheFlushAllDirty(f->unit);
	cacheAdd(clink);
	SignalSema(fioSema);
	return 0;
}

int ioctl2AddSub(hdd_file_slot_t *fileSlot, char *argp)
{
	int			rv;
	u32 		device=fileSlot->f->unit;
	input_param	params;
	u32			emptyBlocks[32];
	apa_cache	*clink;
	u32			sector=0;
	u32			length;

	if(!(fileSlot->f->mode & O_WRONLY))
		return -EACCES;

	if(!(fileSlot->nsub < APA_MAXSUB))
		return -EFBIG;

	memset(&params, 0, sizeof(input_param));

	if((rv=fioPartitionSizeLookUp(argp)) < 0)
		return rv;

	params.size=rv;
	params.flags=APA_FLAG_SUB;
	params.type=fileSlot->type;
	params.main=fileSlot->parts[0].start;
	params.number=fileSlot->nsub+1;
	if((rv=apaCheckPartitionMax(device, params.size)) < 0)
		return rv;

	// walk all looking for any empty blocks
	memset(&emptyBlocks, 0, sizeof(emptyBlocks));
	clink=cacheGetHeader(device, 0, 0, &rv);
	while(clink){
		sector=clink->sector;
		addEmptyBlock(clink->header, emptyBlocks);
		clink=apaGetNextHeader(clink, &rv);
	}
	if(rv!=0)
		return rv;

	if(!(clink=apaAddPartitionHere(device, &params, emptyBlocks, sector, &rv)))
		return rv;

	sector=clink->header->start;
	length=clink->header->length;
	cacheAdd(clink);
	if(!(clink=cacheGetHeader(device, fileSlot->parts[0].start, 0, &rv)))
		return rv;

	clink->header->subs[clink->header->nsub].start=sector;
	clink->header->subs[clink->header->nsub].length=length;
	clink->header->nsub++;
	fileSlot->nsub++;
	fileSlot->parts[fileSlot->nsub].start=sector;
	fileSlot->parts[fileSlot->nsub].length=length;
	clink->flags|=CACHE_FLAG_DIRTY;
	cacheFlushAllDirty(device);
	cacheAdd(clink);
	return rv;
}

int ioctl2DeleteLastSub(hdd_file_slot_t *fileSlot)
{
	int			rv;
	u32 		device=fileSlot->f->unit;
	apa_cache	*mainPart;
	apa_cache	*subPart;

	if(!(fileSlot->f->mode & O_WRONLY))
		return -EACCES;

	if(fileSlot->nsub==0)
		return -ENOENT;

	if(!(mainPart=cacheGetHeader(device, fileSlot->parts[0].start, 0, &rv)))
		return rv;

	if((subPart=cacheGetHeader(device,
		mainPart->header->subs[mainPart->header->nsub-1].start, 0, &rv))) {
		fileSlot->nsub--;
		mainPart->header->nsub--;
		mainPart->flags|=CACHE_FLAG_DIRTY;
		cacheFlushAllDirty(device);
		rv=apaDelete(subPart);
	}
	cacheAdd(mainPart);
	return rv;
}

int hddIoctl2(iop_file_t *f, int req, void *argp, unsigned int arglen,
			  void *bufp, unsigned int buflen)
{
	u32 rv=0;
	hdd_file_slot_t *fileSlot=f->privdata;

	WaitSema(fioSema);
	switch(req)
	{
	// cmd set 1
	case APA_IOCTL2_ADD_SUB:
		rv=ioctl2AddSub(fileSlot, (char *)argp);
		break;

	case APA_IOCTL2_DELETE_LAST_SUB:
		rv=ioctl2DeleteLastSub(fileSlot);
		break;

	case APA_IOCTL2_NUMBER_OF_SUBS:
		rv=fileSlot->nsub;
		break;

	case APA_IOCTL2_FLUSH_CACHE:
		ata_device_flush_cache(f->unit);
		break;

	// cmd set 2
	case APA_IOCTL2_TRANSFER_DATA:
		rv=ioctl2Transfer(f->unit, fileSlot, argp);
		break;

	case APA_IOCTL2_GETSIZE:
		rv=fileSlot->parts[*(u32 *)argp].length;
		break;

	case APA_IOCTL2_SET_PART_ERROR:
		setPartErrorSector(f->unit, fileSlot->parts[0].start); rv=0;
		break;

	case APA_IOCTL2_GET_PART_ERROR:
		if((rv=getPartErrorSector(f->unit, APA_SECTOR_PART_ERROR, bufp)) > 0) {
			if(*(u32 *)bufp==fileSlot->parts[0].start) {
				rv=0; setPartErrorSector(f->unit, 0);// clear last error :)
			}
		}
		break;

	case APA_IOCTL2_GETHEADER:
		if(ata_device_sector_io(f->unit, bufp, fileSlot->parts[0].start, sizeof(apa_header)/512, ATA_DIR_READ))
			rv=-EIO;
		rv=sizeof(apa_header);
		break;

	default:
		rv=-EINVAL;
		break;
	}
	SignalSema(fioSema);
	return rv;
}

int devctlSwapTemp(u32 device, char *argp)
{
	int			rv;
	input_param	params;
	char		szBuf[APA_IDMAX];
	apa_cache	*partTemp;
	apa_cache	*partNew;


	if((rv=fioGetInput(argp, &params)) < 0)
		return rv;

	if(*(u16 *)(params.id)==(u16)0x5F5F)// test for '__' system partition
		return -EINVAL;

	memset(szBuf, 0, APA_IDMAX);
	strcpy(szBuf, "_tmp");
	if(!(partTemp=apaFindPartition(device, szBuf, &rv)))
		return rv;

	if((partNew=apaFindPartition(device, params.id, &rv))) {
		if((rv=passcmp(partNew->header->fpwd, NULL))==0) {
			memcpy(partTemp->header->id, partNew->header->id, APA_IDMAX);
			memcpy(partTemp->header->rpwd, partNew->header->rpwd, APA_PASSMAX);
			memcpy(partTemp->header->fpwd, partNew->header->fpwd, APA_PASSMAX);
			//memset(partNew->header->id, 0, 8);// BUG! can make it so can not open!!
			memset(partNew->header->id, 0, APA_IDMAX);
			strcpy(partNew->header->id, "_tmp");
			memset(partNew->header->rpwd, 0, APA_PASSMAX);
			memset(partNew->header->fpwd, 0, APA_PASSMAX);
			partTemp->flags|=CACHE_FLAG_DIRTY;
			partNew->flags|=CACHE_FLAG_DIRTY;
			cacheFlushAllDirty(device);
		}
		cacheAdd(partNew);
	}
	cacheAdd(partTemp);
	return rv;
}

int devctlSetOsdMBR(u32 device, hddSetOsdMBR_t *mbrInfo)
{
	int rv;
	apa_cache *clink;

	if(!(clink=cacheGetHeader(device, APA_SECTOR_MBR, 0, &rv)))
		return rv;

	dprintf1("ps2hdd: mbr start: %ld\n"
			 "ps2hdd: mbr size : %ld\n", mbrInfo->start, mbrInfo->size);
	clink->header->mbr.osdStart=mbrInfo->start;
	clink->header->mbr.osdSize=mbrInfo->size;
	clink->flags|=CACHE_FLAG_DIRTY;
	cacheFlushAllDirty(device);
	cacheAdd(clink);
	return rv;
}

int hddDevctl(iop_file_t *f, const char *devname, int cmd, void *arg,
			  unsigned int arglen, void *bufp, unsigned int buflen)
{
	int	rv=0;

	WaitSema(fioSema);
	switch(cmd)
	{
	// cmd set 1
	case APA_DEVCTL_DEV9_SHUTDOWN:
		ata_device_smart_save_attr(f->unit);
		dev9Shutdown();
		break;

	case APA_DEVCTL_IDLE:
		rv=ata_device_idle(f->unit, *(char *)arg);
		break;

	case APA_DEVCTL_MAX_SECTORS:
		rv=hddDeviceBuf[f->unit].partitionMaxSize;
		break;

	case APA_DEVCTL_TOTAL_SECTORS:
		rv=hddDeviceBuf[f->unit].totalLBA;
		break;

	case APA_DEVCTL_FLUSH_CACHE:
		if(ata_device_flush_cache(f->unit))
			rv=-EIO;
		break;

	case APA_DEVCTL_SWAP_TMP:
		rv=devctlSwapTemp(f->unit, (char *)arg);
		break;

	case APA_DEVCTL_SMART_STAT:
		rv=ata_device_smart_get_status(f->unit);
		break;

	case APA_DEVCTL_STATUS:
		rv=hddDeviceBuf[f->unit].status;
		break;

	case APA_DEVCTL_FORMAT:
		rv=hddDeviceBuf[f->unit].format;
		break;

	// removed dos not work the way you like... use hddlib ;)
	//case APA_DEVCTL_FREE_SECTORS:
	//case APA_DEVCTL_FREE_SECTORS2:
	//	rv=apaGetFreeSectors(f->unit, bufp);
	//	break;

	// cmd set 2 :)
	case APA_DEVCTL_GETTIME:
		rv=getPs2Time((ps2time *)bufp);
		break;

	case APA_DEVCTL_SET_OSDMBR:
		rv=devctlSetOsdMBR(f->unit, (hddSetOsdMBR_t *)arg);
		break;

	case APA_DEVCTL_GET_SECTOR_ERROR:
		rv=getPartErrorSector(f->unit, APA_SECTOR_SECTOR_ERROR, 0);
		break;

	case APA_DEVCTL_GET_ERROR_PART_NAME:
		rv=getPartErrorName(f->unit, (char *)bufp);
		break;

	case APA_DEVCTL_ATA_READ:
		rv=ata_device_sector_io(f->unit, (void *)bufp, ((hddAtaTransfer_t *)arg)->lba,
			((hddAtaTransfer_t *)arg)->size, ATA_DIR_READ);
		break;

	case APA_DEVCTL_ATA_WRITE:
		rv=ata_device_sector_io(f->unit, ((hddAtaTransfer_t *)arg)->data,
			((hddAtaTransfer_t *)arg)->lba, ((hddAtaTransfer_t *)arg)->size,
				ATA_DIR_WRITE);
		break;

	case APA_DEVCTL_SCE_IDENTIFY_DRIVE:
		rv=ata_device_sce_identify_drive(f->unit, (u16 *)bufp);
		break;

	case APA_DEVCTL_IS_48BIT:
		rv=ata_device_is_48bit(f->unit);
		break;

	case APA_DEVCTL_SET_TRANSFER_MODE:
		rv=ata_device_set_transfer_mode(f->unit, ((hddAtaSetMode_t *)arg)->type, ((hddAtaSetMode_t *)arg)->mode);
		break;

	case APA_DEVCTL_ATA_IOP_WRITE:
		rv=ata_device_sector_io(f->unit, ((hddAtaIOPTransfer_t *)arg)->data,
			((hddAtaIOPTransfer_t *)arg)->lba, ((hddAtaIOPTransfer_t *)arg)->size,
				ATA_DIR_WRITE);
		break;

	default:
		rv=-EINVAL;
		break;
	}
	SignalSema(fioSema);

	return rv;
}

int fioUnsupported(iop_file_t *f){return -1;}
