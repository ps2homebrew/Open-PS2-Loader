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

#ifndef _PFS_H
#define _PFS_H

#include "types.h"
#include "defs.h"
#include "irx.h"
#include "loadcore.h"
#include "poweroff.h"
#include "sysmem.h"
#include "stdio.h"
#include "sysclib.h"
#include "errno.h"
#include "sys/fcntl.h"
#include "sys/stat.h"
#include "iomanX.h"
#include "thsemap.h"
#include "intrman.h"
#include "cdvdman.h"

#include "pfs_types.h"

#ifndef min
#define min(a, b)	((a) < (b) ? (a) : (b))
#endif

#define PFS_MAJOR	1
#define PFS_MINOR	0

///////////////////////////////////////////////////////////////////////////////
//   Global types

typedef int (*deviceTransfer_p)(int fd, void *buffer, /*u16*/u32 sub, u32 sector, u32 size, u32 mode);
typedef u32 (*deviceGetSubNumber_p)(int fd);
typedef u32 (*deviceGetSize_p)(int fd, /*u16*/u32 sub/*0=main 1+=subs*/);
typedef void (*deviceSetPartError_p)(int fd);
typedef int/*0 only :P*/ (*deviceFlushCache_p)(int fd);

typedef struct {
	char					*devName;			//
	deviceTransfer_p		transfer;			//
	deviceGetSubNumber_p	getSubNumber;		//
	deviceGetSize_p			getSize;			//
	deviceSetPartError_p	setPartitionError;	// set open partition as haveing a error
	deviceFlushCache_p		flushCache;			//

} block_device;


typedef struct {
	block_device *blockDev;		// call table for hdd(hddCallTable)
	int fd;						//
	u32 flags;					// rename to attr ones checked
	u32 total_sector;			// number of sectors in the filesystem
	u32 zfree;					// zone free
	u32 sector_scale;			//
	u32 inode_scale;			//
	u32 zsize;					// zone size
	u32 num_subs;				// number of sub partitions in the filesystem
	pfs_blockinfo root_dir;		// block info for root directory
	pfs_blockinfo log;			// block info for the log
	pfs_blockinfo current_dir;	// block info for current directory
	u32 lastError;				// 0 if no error :)
	u16 uid;					//
	u16 gid;					//
	u32 free_zone[65];			// free zones in each partition (1 main + 64 possible subs)
} pfs_mount_t;

typedef struct pfs_cache_s {
	struct pfs_cache_s *next;	//
	struct pfs_cache_s *prev;	//
	u16 flags;					//
	u16 nused;					//
	pfs_mount_t *pfsMount;		//
	u32 sub;					// main(0)/sub(+1) partition
	u32 sector;					// block/sector for partition
	union{						//
		void *data;
		pfs_inode *inode;
		pfs_aentry_t *aentry;
		pfs_dentry *dentry;
		pfs_super_block *superblock;
		u32	*bitmap;
	} u;
} pfs_cache_t;

typedef struct
{
	u16 dirty;	//
	u16 sub;	// Sub/main partition
	u32 sector;	// Sector
} pfs_restsInfo_t;

typedef struct
{
	char key[256];
	char value[256];
} pfs_ioctl2attr_t;

typedef struct
{
	pfs_cache_t *inode;
	u32 block_segment;		// index into data array in inode structure for current zone segment
	u32 block_offset;		// block offset from start of current zone segment
	u32 byte_offset;		// byte offset into current zone
} pfs_blockpos_t;

typedef struct {
	iop_file_t *fd;				//
	pfs_cache_t *clink;			//
	u32 aentryOffset;			// used for read offset
	u64 position;				//
	pfs_blockpos_t block_pos;	// current position into file
	pfs_restsInfo_t restsInfo;	//
	u8 restsBuffer[512];		// used for reading mis-aligned/remainder data
} pfs_file_slot_t;

typedef struct {
	u32 maxMount;
	u32 maxOpen;
} pfs_config_t;

///////////////////////////////////////////////////////////////////////////////
//   Global defines

// attribute flags
#define FIO_ATTR_READABLE		0x0001
#define FIO_ATTR_WRITEABLE		0x0002
#define FIO_ATTR_EXECUTABLE		0x0004
#define FIO_ATTR_COPYPROTECT	0x0008
#define FIO_ATTR_UNK0010		0x0010
#define FIO_ATTR_SUBDIR			0x0020
#define FIO_ATTR_UNK0040		0x0040
#define FIO_ATTR_CLOSED			0x0080
#define FIO_ATTR_UNK0100		0x0100
#define FIO_ATTR_UNK0200		0x0200
#define FIO_ATTR_UNK0400		0x0400
#define FIO_ATTR_PDA			0x0800
#define FIO_ATTR_PSX			0x1000
#define FIO_ATTR_UNK2000		0x2000
#define FIO_ATTR_HIDDEN			0x4000

// cache flags(status)
#define CACHE_FLAG_DIRTY		0x01
#define CACHE_FLAG_NOLOAD		0x02
#define CACHE_FLAG_MASKSTATUS	0x0F

// cache flags(types)
#define CACHE_FLAG_NOTHING		0x00
#define CACHE_FLAG_SEGD			0x10
#define CACHE_FLAG_SEGI			0x20
#define CACHE_FLAG_BITMAP		0x40
#define CACHE_FLAG_MASKTYPE		0xF0

// fsck stats
#define FSCK_STAT_OK			0x00
#define FSCK_STAT_WRITE_ERROR	0x01
#define FSCK_STAT_ERROR_0x02	0x02

// odd and end
#define MODE_SET_FLAG			0x00
#define MODE_REMOVE_FLAG		0x01
#define MODE_CHECK_FLAG			0x02

// mount flags
#define MOUNT_BUSY				0x8000

///////////////////////////////////////////////////////////////////////////////
//   Externs

extern pfs_file_slot_t *fileSlots;
extern pfs_config_t pfsConfig;
extern pfs_cache_t *cacheBuf;
extern u32 numBuffers;
extern u32 metaSize;
extern int blockSize;
extern pfs_mount_t *pfsMountBuf;
extern int symbolicLinks;
extern int pfsFioSema;

///////////////////////////////////////////////////////////////////////////////
//   Function declerations

pfs_mount_t * getMountedUnit(u32 unit);
void clearMount(pfs_mount_t *pfsMount);

extern int pfsDebug;

///////////////////////////////////////////////////////////////////////////////
//   Debug stuff

#define DEBUG_CALL_LOG
#define DEBUG_VERBOSE_LOG

#ifdef DEBUG_VERBOSE_LOG
#define dprintf if(pfsDebug) printf
#else
#define dprintf(format, args...)
#endif

#ifdef DEBUG_CALL_LOG
#define dprintf2 if(pfsDebug) printf
#else
#define dprintf2(format, args...)
#endif

#include "misc.h"
#include "bitmap.h"
#include "dir.h"
#include "inode.h"
#include "journal.h"
#include "super.h"
#include "cache.h"
#include "block.h"
#include "pfs_fio.h"
#include "pfs_fioctl.h"

#endif /* _PFS_H */
