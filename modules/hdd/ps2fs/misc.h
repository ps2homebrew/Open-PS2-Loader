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

#ifndef _MISC_H
#define _MISC_H

///////////////////////////////////////////////////////////////////////////////
// APA Commands defines

// APA IOCTL2 commands
#define APA_IOCTL2_ADD_SUB			0x00006801
#define APA_IOCTL2_DELETE_LAST_SUB	0x00006802
#define APA_IOCTL2_NUMBER_OF_SUBS	0x00006803
#define APA_IOCTL2_FLUSH_CACHE		0x00006804
#define APA_IOCTL2_TRANSFER_DATA	0x00006832	// used by pfs to read/write data :P
#define APA_IOCTL2_GETSIZE			0x00006833	// for main(0)/subs(1+)
#define APA_IOCTL2_SET_PART_ERROR	0x00006834

#define IOCTL2_TMODE_READ 0x00
#define IOCTL2_TMODE_WRITE 0x01

// for APA_IOCTL2_TRANSFER_DATA
typedef struct
{
	u32		sub;		// main(0)/subs(1+) to read/write
	u32		sector;		//
	u32		size;		// in sectors
	u32		mode;		// IOCTL2_TMODE_*
	void	*buffer;	//
} hdd_ioctl2_transfer_t;


// APA DEVCTL commands
#define APA_DEVCTL_MAX_SECTORS		0x00004801	// max partition size(in sectors)
#define APA_DEVCTL_TOTAL_SECTORS	0x00004802
#define APA_DEVCTL_IDLE				0x00004803
#define APA_DEVCTL_FLUSH_CACHE		0x00004804
#define APA_DEVCTL_SWAP_TMP			0x00004805
#define APA_DEVCTL_DEV9_SHUTDOWN	0x00004806
#define APA_DEVCTL_STATUS			0x00004807
#define APA_DEVCTL_FORMAT			0x00004808
#define APA_DEVCTL_SMART_STAT		0x00004809
#define APA_DEVCTL_FREE_SECTORS		0x0000480A
#define APA_DEVCTL_GETTIME			0x00006832

///////////////////////////////////////////////////////////////////////////////
//   Function declerations

int fsckStat(pfs_mount_t *pfsMount, pfs_super_block *superblock, u32 stat, int mode);

void *allocMem(int size);
int getPs2Time(pfs_datetime *tm);
void printBitmap(u32 *bitmap);

block_device *getDeviceTable(const char *name);
int getScale(int num, int size);
u32 fixIndex(u32 index);
int checkAccess(pfs_cache_t *clink, int flags);
pfs_cache_t *sdGetNext(pfs_cache_t *clink, int *result);
char* splitPath(char *filename, char *path, int *result);
u16 getMaxthIndex(pfs_mount_t *pfsMount);

int hddTransfer(int fd, void *buffer, u32 sub/*0=main 1+=subs*/, u32 sector,
		u32 size/* in sectors*/, u32 mode);
u32 hddGetSubNumber(int fd);
u32 hddGetSize(int fd, u32 sub/*0=main 1+=subs*/);
void hddSetPartError(int fd);
int hddFlushCache(int fd);

#endif /* _MISC_H */
