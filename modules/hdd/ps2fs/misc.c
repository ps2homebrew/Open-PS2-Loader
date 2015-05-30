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
# Miscellaneous routines
*/

#include "pfs.h"

///////////////////////////////////////////////////////////////////////////////
//   Function defenitions

void *allocMem(int size)
{
	int intrStat;
	void *mem;

	CpuSuspendIntr(&intrStat);
	mem = AllocSysMemory(ALLOC_FIRST, size, NULL);
	CpuResumeIntr(intrStat);

	return mem;
}

int getPs2Time(pfs_datetime *tm)
{
	sceCdCLOCK	cdtime;
	static pfs_datetime timeBuf={
		0, 0x0D, 0x0E, 0x0A, 0x0D, 1, 2003	// used if can not get time...
	};

	if(sceCdReadClock(&cdtime)!=0 && cdtime.stat==0)
	{
		timeBuf.sec=btoi(cdtime.second);
		timeBuf.min=btoi(cdtime.minute);
		timeBuf.hour=btoi(cdtime.hour);
		timeBuf.day=btoi(cdtime.day);
		timeBuf.month=btoi(cdtime.month & 0x7F);	//The old CDVDMAN sceCdReadClock() function does not automatically file off the highest bit.
		timeBuf.year=btoi(cdtime.year) + 2000;
	}
	memcpy(tm, &timeBuf, sizeof(pfs_datetime));
	return 0;
}

int fsckStat(pfs_mount_t *pfsMount, pfs_super_block *superblock,
	u32 stat, int mode)
{	// mode 0=set flag, 1=remove flag, else check stat

	if(pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_BLOCKSIZE, 1,
		IOCTL2_TMODE_READ)==0)
	{
		switch(mode)
		{
			case MODE_SET_FLAG:	// set flag
				superblock->fsckStat|=stat;
				break;
			case MODE_REMOVE_FLAG:	// remove flag
				superblock->fsckStat&=~stat;
				break;
			default/*MODE_CHECK_FLAG*/:// check flag
				return 0 < (superblock->fsckStat & stat);
		}
		pfsMount->blockDev->transfer(pfsMount->fd, superblock, 0, PFS_BLOCKSIZE, 1,
			IOCTL2_TMODE_WRITE);
		pfsMount->blockDev->flushCache(pfsMount->fd);
	}
	return 0;
}

void printBitmap(u32 *bitmap) {
	u32 i, j;
	char a[48+1], b[16+1];

	b[16]=0;
	for (i=0; i < 32; i++){
		memset(a, 0, 49);
		for (j=0; j < 16; j++){
			char *c=(char*)bitmap+j+i*16;

			sprintf(a+j*3, "%02x ", *c);
			b[j] = ((*c>=0) && (look_ctype_table(*c) & 0x17)) ?
				*c : '.';
		}
		printf("%s%s\n", a, b);
	}
}

int getScale(int num, int size)
{
	int scale = 0;

	while((size << scale) != num)
		scale++;

	return scale;
}

u32 fixIndex(u32 index)
{
	if(index < 114)
		return index;

	index -= 114;
	return index % 123;
}


int checkAccess(pfs_cache_t *clink, int flags)
{
	int mode;

	// Bail if trying to write to read-only mount
	if ((clink->pfsMount->flags & FIO_MT_RDONLY) && (flags & O_WRONLY))
		return -EROFS;

	if (((clink->u.inode->mode & FIO_S_IFMT) != FIO_S_IFDIR) &&
		((clink->u.inode->mode & 0111) == 0))
		mode=6;
	else
		mode=7;
	return ((mode & flags) & 7) == flags ? 0 : -EACCES;
}

char* splitPath(char *filename, char *path, int *result)
{
	int i=0;
	int j=0;

	for (i=0; filename[i] == '/'; i++)
		;

	for (; i<1024 && filename[i] != '/'; i++){
		if (filename[i] == 0)	break;
		if (j < 255)
			path[j++] = filename[i];
	}

	if (j<256)
		path[j]=0;

	while (filename[i] == '/')
		if (i<1024)
			i++;
		else
			break;
	if (i<1024)
		return &filename[i];

	*result=-ENAMETOOLONG;
	return 0;
}

u16 getMaxthIndex(pfs_mount_t *pfsMount)
{
	u32 max=0, maxI=0, i, v;

	for (i=0; i < pfsMount->num_subs + 1; i++)  //enumerate all subs
	{
		v =	(pfsMount->free_zone[i] * (u64)100) /
			(pfsMount->blockDev->getSize(pfsMount->fd, i) >> pfsMount->sector_scale);
		if (max < v)
		{
			max=v;
			maxI=i;
		}
	}
	return maxI;
}

///////////////////////////////////////////////////////////////////////////////
//   Functions to work with hdd.irx

#define NUM_SUPPORTED_DEVICES	1
block_device deviceCallTable[NUM_SUPPORTED_DEVICES] = {
	{
		"hdd",
		hddTransfer,
		hddGetSubNumber,
		hddGetSize,
		hddSetPartError,
		hddFlushCache,
	}
};

block_device *getDeviceTable(const char *name)
{
	char *end;
	char devname[32];
	char *tmp;
	u32 len;
	int i;

	while(name[0] == ' ')
		name++;

	end = strchr(name, ':');
	if(!end) {
		printf("ps2fs: Error: Unknown block device '%s'\n", name);
		return NULL;
	}

	len = (u32)end - (u32)name;
	strncpy(devname, name, len);
	devname[len] = '\0';

	// Loop until digit is found, then terminate string at that digit.
	// Should then have just the device name left, minus any front spaces or trailing digits.
	tmp = devname;
	while(!(look_ctype_table(tmp[0]) & 0x04))
		tmp++;
	tmp[0] = '\0';

	for(i = 0; i < NUM_SUPPORTED_DEVICES; i++)
		if(!strcmp(deviceCallTable[i].devName, devname))
			return &deviceCallTable[i];

	return NULL;
}

int hddTransfer(int fd, void *buffer, u32 sub/*0=main 1+=subs*/, u32 sector,
				u32 size/* in sectors*/, u32 mode)
{
	hdd_ioctl2_transfer_t t;

	t.sub=sub;
	t.sector=sector;
	t.size=size;
	t.mode=mode;
	t.buffer=buffer;

	return ioctl2(fd, APA_IOCTL2_TRANSFER_DATA, &t, 0, NULL, 0);
}

u32 hddGetSubNumber(int fd)
{
	return ioctl2(fd, APA_IOCTL2_NUMBER_OF_SUBS, NULL, 0, NULL, 0);
}

u32 hddGetSize(int fd, u32 sub/*0=main 1+=subs*/)
{	// of a partition
	return ioctl2(fd, APA_IOCTL2_GETSIZE, &sub, 0, NULL, 0);
}

void hddSetPartError(int fd)
{
	ioctl2(fd, APA_IOCTL2_SET_PART_ERROR, NULL, 0, NULL, 0);
}

int hddFlushCache(int fd)
{
	return ioctl2(fd, APA_IOCTL2_FLUSH_CACHE, NULL, 0, NULL, 0);
}
