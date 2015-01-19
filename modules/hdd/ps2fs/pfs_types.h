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

#ifndef _PFS_TYPES_H
#define _PFS_TYPES_H

///////////////////////////////////////////////////////////////////////////////
//   Pre-processor macros

// General constants
#define PFS_BLOCKSIZE 		0x2000
#define PFS_SUPER_MAGIC		0x50465300	// "PFS\0" aka Playstation Filesystem
#define PFS_JOUNRNAL_MAGIC	0x5046534C	// "PFSL" aka PFS Log
#define PFS_SEGD_MAGIC		0x53454744	// "SEGD" aka segment descriptor direct
#define PFS_SEGI_MAGIC		0x53454749	// "SEGI" aka segment descriptor indirect
#define PFS_MAX_SUBPARTS	64
#define PFS_NAME_LEN		255
#define PFS_VERSION			4

///////////////////////////////////////////////////////////////////////////////
//   Structures

// jounrnal/log
typedef struct {
	u32 magic;			// =PFS_JOUNRNAL_MAGIC
	u16 num;			//
	u16 checksum;		//
	struct{
		u32 sector;		// block/sector for partition
		u16 sub;		// main(0)/sub(+1) partition
		u16 logSector;	// block/sector offset in journal area
	} log[127];
} pfs_journal_t;

typedef struct{
	u8 kLen;			// key len/used for offset in to str for value
	u8 vLen;			// value len
	u16 aLen;			// allocated length == ((kLen+vLen+7) & ~3)
	char str[3];		// size = 3 so sizeof pfs_aentry_t=7 :P
} pfs_aentry_t;

typedef struct {
	u32	inode;
	u8	sub;
	u8	pLen;		// path length
	u16	aLen;		// allocated length == ((pLen+8+3) & ~3)
	char	path[512-8];
} pfs_dentry;

// Block number/count pair (used in inodes)
typedef struct {
    u32 number;		//
    u16 subpart;	//
    u16 count;		//
} pfs_blockinfo;

// Date/time descriptor
typedef struct {
    u8 unused;	//
    u8 sec;		//
    u8 min;		//
    u8 hour;	//
    u8 day;		//
    u8 month;	//
    u16 year;	//
} pfs_datetime;

// Superblock structure
typedef struct {
    u32 magic;			//
	u32 version;		//
    u32 unknown1;		//
	u32 fsckStat;		//
    u32 zone_size;		//
    u32 num_subs;		// number of subs attached to filesystem
	pfs_blockinfo log;	// block info for metadata log
	pfs_blockinfo root;	// block info for root directory
} pfs_super_block;

// Inode structure
typedef struct {
	u32 checksum;				// Sum of all other words in the inode
	u32 magic;					//
	pfs_blockinfo inode_block;	// start block of inode
	pfs_blockinfo next_segment;	// next segment descriptor inode
	pfs_blockinfo last_segment;	// last segment descriptor inode
	pfs_blockinfo unused;		//
	pfs_blockinfo data[114];	//
	u16 mode;					// file mode
	u16 attr;					// file attributes
	u16 uid;					//
	u16 gid;					//
	pfs_datetime atime;			//
	pfs_datetime ctime;			//
	pfs_datetime mtime;			//
	u64 size;					//
	u32 number_blocks;			// number of blocks/zones used by file
	u32 number_data;			// number of used entries in data array
	u32 number_segdesg;			// number of "indirect blocks"/next segment descriptor's
	u32 subpart;				// subpart of inode
	u32 reserved[4];			//
} pfs_inode;

#endif /* _PFS_TYPES_H */
