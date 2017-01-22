/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#ifndef _PFS_H
#define _PFS_H

#ifndef min
#define min(a, b)	((a) < (b) ? (a) : (b))
#endif

#define PFS_MAJOR	1
#define PFS_MINOR	4

///////////////////////////////////////////////////////////////////////////////
//   Global types

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
//	Global defines

// mount flags
#define PFS_MOUNT_BUSY				0x8000

///////////////////////////////////////////////////////////////////////////////
//	Function declarations

pfs_mount_t *pfsGetMountedUnit(s32 unit);
void pfsClearMount(pfs_mount_t *pfsMount);

#endif /* _PFS_H */
