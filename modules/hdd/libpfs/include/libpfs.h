
/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#ifndef _LIBPFS_H
#define _LIBPFS_H

// General constants
#define PFS_BLOCKSIZE 		0x2000
#define PFS_SUPER_MAGIC		0x50465300	// "PFS\0" aka Playstation Filesystem
#define PFS_JOUNRNAL_MAGIC	0x5046534C	// "PFSL" aka PFS Log
#define PFS_SEGD_MAGIC		0x53454744	// "SEGD" aka segment descriptor direct
#define PFS_SEGI_MAGIC		0x53454749	// "SEGI" aka segment descriptor indirect
#define PFS_MAX_SUBPARTS	64
#define PFS_NAME_LEN		255
#define PFS_VERSION			4

// attribute flags
#define PFS_FIO_ATTR_READABLE		0x0001
#define PFS_FIO_ATTR_WRITEABLE		0x0002
#define PFS_FIO_ATTR_EXECUTABLE		0x0004
#define PFS_FIO_ATTR_COPYPROTECT	0x0008
#define PFS_FIO_ATTR_UNK0010		0x0010
#define PFS_FIO_ATTR_SUBDIR			0x0020
#define PFS_FIO_ATTR_UNK0040		0x0040
#define PFS_FIO_ATTR_CLOSED			0x0080
#define PFS_FIO_ATTR_UNK0100		0x0100
#define PFS_FIO_ATTR_UNK0200		0x0200
#define PFS_FIO_ATTR_UNK0400		0x0400
#define PFS_FIO_ATTR_PDA			0x0800
#define PFS_FIO_ATTR_PSX			0x1000
#define PFS_FIO_ATTR_UNK2000		0x2000
#define PFS_FIO_ATTR_HIDDEN			0x4000

// cache flags (status)
#define PFS_CACHE_FLAG_DIRTY		0x01
#define PFS_CACHE_FLAG_NOLOAD		0x02
#define PFS_CACHE_FLAG_MASKSTATUS	0x0F

// cache flags (types)
#define PFS_CACHE_FLAG_NOTHING		0x00
#define PFS_CACHE_FLAG_SEGD			0x10
#define PFS_CACHE_FLAG_SEGI			0x20
#define PFS_CACHE_FLAG_BITMAP		0x40
#define PFS_CACHE_FLAG_MASKTYPE		0xF0

// fsck stats
#define PFS_FSCK_STAT_OK			0x00
#define PFS_FSCK_STAT_WRITE_ERROR	0x01
#define PFS_FSCK_STAT_ERRORS_FIXED	0x02

// odd and end
#define PFS_MODE_SET_FLAG			0x00
#define PFS_MODE_REMOVE_FLAG		0x01
#define PFS_MODE_CHECK_FLAG			0x02

// journal/log
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
} pfs_dentry_t;

// Block number/count pair (used in inodes)
typedef struct {
    u32 number;		//
    u16 subpart;	//
    u16 count;		//
} pfs_blockinfo_t;

// Date/time descriptor
typedef struct {
    u8 unused;	//
    u8 sec;		//
    u8 min;		//
    u8 hour;	//
    u8 day;		//
    u8 month;	//
    u16 year;	//
} pfs_datetime_t;

// Superblock structure
typedef struct {
    u32 magic;			//
	u32 version;		//
    u32 unknown1;		//
	u32 pfsFsckStat;		//
    u32 zone_size;		//
    u32 num_subs;		// number of subs attached to filesystem
	pfs_blockinfo_t log;	// block info for metadata log
	pfs_blockinfo_t root;	// block info for root directory
} pfs_super_block_t;

// Inode structure
typedef struct {
	u32 checksum;				// Sum of all other words in the inode
	u32 magic;					//
	pfs_blockinfo_t inode_block;	// start block of inode
	pfs_blockinfo_t next_segment;	// next segment descriptor inode
	pfs_blockinfo_t last_segment;	// last segment descriptor inode
	pfs_blockinfo_t unused;		//
	pfs_blockinfo_t data[114];	//
	u16 mode;					// file mode
	u16 attr;					// file attributes
	u16 uid;					//
	u16 gid;					//
	pfs_datetime_t atime;			//
	pfs_datetime_t ctime;			//
	pfs_datetime_t mtime;			//
	u64 size;					//
	u32 number_blocks;			// number of blocks/zones used by file
	u32 number_data;			// number of used entries in data array
	u32 number_segdesg;			// number of "indirect blocks"/next segment descriptor's
	u32 subpart;				// subpart of inode
	u32 reserved[4];			//
} pfs_inode_t;

typedef struct {
	char *devName;
	int (*transfer)(int fd, void *buffer, /*u16*/u32 sub, u32 sector, u32 size, u32 mode);
	u32 (*getSubNumber)(int fd);
	u32 (*getSize)(int fd, /*u16*/u32 sub/*0=main 1+=subs*/);
	void (*setPartitionError)(int fd);	// set open partition as having an error
	int	(*flushCache)(int fd);
} pfs_block_device_t;

typedef struct {
	pfs_block_device_t *blockDev;		// call table for hdd(hddCallTable)
	int fd;						//
	u32 flags;					// rename to attr ones checked
	u32 total_sector;			// number of sectors in the filesystem
	u32 zfree;					// zone free
	u32 sector_scale;			//
	u32 inode_scale;			//
	u32 zsize;					// zone size
	u32 num_subs;				// number of sub partitions in the filesystem
	pfs_blockinfo_t root_dir;		// block info for root directory
	pfs_blockinfo_t log;			// block info for the log
	pfs_blockinfo_t current_dir;	// block info for current directory
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
		pfs_inode_t *inode;
		pfs_aentry_t *aentry;
		pfs_dentry_t *dentry;
		pfs_super_block_t *superblock;
		u32	*bitmap;
	} u;
} pfs_cache_t;

typedef struct
{
	pfs_cache_t *inode;
	u32 block_segment;		// index into data array in inode structure for current zone segment
	u32 block_offset;		// block offset from start of current zone segment
	u32 byte_offset;		// byte offset into current zone
} pfs_blockpos_t;

///////////////////////////////////////////////////////////////////////////////
// Super Block functions

#define PFS_SUPER_SECTOR			8192
#define PFS_SUPER_BACKUP_SECTOR		8193

int pfsCheckZoneSize(u32 zone_size);
u32 pfsGetBitmapSizeSectors(int zoneScale, u32 partSize);
u32 pfsGetBitmapSizeBlocks(int scale, u32 mainsize);
int pfsFormatSub(pfs_block_device_t *blockDev, int fd, u32 sub, u32 reserved, u32 scale, u32 fragment);
int pfsFormat(pfs_block_device_t *blockDev, int fd, int zonesize, int fragment);
int pfsUpdateSuperBlock(pfs_mount_t *pfsMount, pfs_super_block_t *superblock, u32 sub);
int pfsMountSuperBlock(pfs_mount_t *pfsMount);

///////////////////////////////////////////////////////////////////////////////
// Cache functions

void pfsCacheFree(pfs_cache_t *clink);
void pfsCacheLink(pfs_cache_t *clink, pfs_cache_t *cnew);
pfs_cache_t *pfsCacheUnLink(pfs_cache_t *clink);
pfs_cache_t *pfsCacheUsedAdd(pfs_cache_t *clink);
int pfsCacheTransfer(pfs_cache_t* clink, int mode);
void pfsCacheFlushAllDirty(pfs_mount_t *pfsMount);
pfs_cache_t *pfsCacheAlloc(pfs_mount_t *pfsMount, u16 sub, u32 scale, int flags, int *result);
pfs_cache_t *pfsCacheGetData(pfs_mount_t *pfsMount, u16 sub, u32 scale, int flags, int *result);
pfs_cache_t *pfsCacheAllocClean(int *result);
int pfsCacheIsFull(void);
int pfsCacheInit(u32 numBuf, u32 bufSize);
void pfsCacheClose(pfs_mount_t *pfsMount);
void pfsCacheMarkClean(pfs_mount_t *pfsMount, u32 subpart, u32 sectorStart, u32 sectorEnd);

///////////////////////////////////////////////////////////////////////////////
//	Bitmap functions

#define PFS_BITMAP_ALLOC	0
#define PFS_BITMAP_FREE		1

typedef struct
{
	u32 chunk;
	u32 index;
	u32 bit;
	u32 partitionChunks;
	u32 partitionRemainder;
} pfs_bitmapInfo_t;

void pfsBitmapSetupInfo(pfs_mount_t *pfsMount, pfs_bitmapInfo_t *info, u32 subpart, u32 number);
void pfsBitmapAllocFree(pfs_cache_t *clink, u32 operation, u32 subpart, u32 chunk, u32 index, u32 _bit, u32 count);
int pfsBitmapAllocateAdditionalZones(pfs_mount_t *pfsMount, pfs_blockinfo_t *bi, u32 count);
int pfsBitmapAllocZones(pfs_mount_t *pfsMount, pfs_blockinfo_t *bi, u32 amount);
int pfsBitmapSearchFreeZone(pfs_mount_t *pfsMount, pfs_blockinfo_t *bi, u32 max_count);
void pfsBitmapFreeBlockSegment(pfs_mount_t *pfsMount, pfs_blockinfo_t *bi);
int pfsBitmapCalcFreeZones(pfs_mount_t *pfsMount, int sub);
void pfsBitmapShow(pfs_mount_t *pfsMount);
void pfsBitmapFreeInodeBlocks(pfs_cache_t *clink);

///////////////////////////////////////////////////////////////////////////////
//	Block functions

int pfsBlockSeekNextSegment(pfs_cache_t *clink, pfs_blockpos_t *blockpos);
u32 pfsBlockSyncPos(pfs_blockpos_t *blockpos, u64 size);
int pfsBlockInitPos(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u64 position);
int pfsBlockExpandSegment(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 count);
int pfsBlockAllocNewSegment(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 blocks);
pfs_blockinfo_t* pfsBlockGetCurrent(pfs_blockpos_t *blockpos);
pfs_cache_t *pfsBlockGetNextSegment(pfs_cache_t *clink, int *result);
pfs_cache_t *pfsBlockGetLastSegmentDescriptorInode(pfs_cache_t *clink, int *result);

///////////////////////////////////////////////////////////////////////////////
//	Directory-Entry (DEntry) inode functions

pfs_cache_t *pfsGetDentry(pfs_cache_t *clink, char *path, pfs_dentry_t **dentry, u32 *size, int option);
int pfsGetNextDentry(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 *position, char *name, pfs_blockinfo_t *bi);
pfs_cache_t *pfsFillDentry(pfs_cache_t *clink, pfs_dentry_t *dentry, char *path1, pfs_blockinfo_t *bi, u32 len, u16 mode);
pfs_cache_t *pfsDirAddEntry(pfs_cache_t *dir, char *filename, pfs_blockinfo_t *bi, u16 mode, int *result);
pfs_cache_t *pfsDirRemoveEntry(pfs_cache_t *clink, char *path);
int pfsCheckDirForFiles(pfs_cache_t *clink);
void pfsFillSelfAndParentDentries(pfs_cache_t *clink, pfs_blockinfo_t *self, pfs_blockinfo_t *parent);
pfs_cache_t* pfsSetDentryParent(pfs_cache_t *clink, pfs_blockinfo_t *bi, int *result);
pfs_cache_t *pfsInodeGetFileInDir(pfs_cache_t *dirInode, char *path, int *result);
pfs_cache_t *pfsInodeGetFile(pfs_mount_t *pfsMount, pfs_cache_t *clink, const char *name, int *result);
void pfsInodeFill(pfs_cache_t *ci, pfs_blockinfo_t *bi, u16 mode, u16 uid, u16 gid);
int pfsInodeRemove(pfs_cache_t *parent, pfs_cache_t *inode, char *path);
pfs_cache_t *pfsInodeGetParent(pfs_mount_t *pfsMount, pfs_cache_t *clink, const char *filename, char *path, int *result);
pfs_cache_t *pfsInodeCreate(pfs_cache_t *clink, u16 mode, u16 uid, u16 gid, int *result);
int pfsCheckAccess(pfs_cache_t *clink, int flags);
char* pfsSplitPath(char *filename, char *path, int *result);
u16 pfsGetMaxIndex(pfs_mount_t *pfsMount);

int pfsAllocZones(pfs_cache_t *clink, int msize, int mode);
void pfsFreeZones(pfs_cache_t *pfree);

///////////////////////////////////////////////////////////////////////////////
//	Inode functions

void pfsInodePrint(pfs_inode_t *inode);
int pfsInodeCheckSum(pfs_inode_t *inode);
void pfsInodeSetTime(pfs_cache_t *clink);
pfs_cache_t *pfsInodeGetData(pfs_mount_t *pfsMount, u16 sub, u32 inode, int *result);
int pfsInodeSync(pfs_blockpos_t *blockpos, u64 size, u32 used_segments);
pfs_cache_t *pfsGetDentriesChunk(pfs_blockpos_t *position, int *result);
pfs_cache_t *pfsGetDentriesAtPos(pfs_cache_t *clink, u64 position, int *offset, int *result);

///////////////////////////////////////////////////////////////////////////////
//	Journal functions

int pfsJournalChecksum(void *header);
void pfsJournalWrite(pfs_mount_t *pfsMount, pfs_cache_t *clink, u32 pfsCacheNumBuffers);
int pfsJournalReset(pfs_mount_t *pfsMount);
int pfsJournalFlush(pfs_mount_t *pfsMount);
int pfsJournalRestore(pfs_mount_t *pfsMount);
int pfsJournalResetThis(pfs_block_device_t *blockDev, int fd, u32 sector);

///////////////////////////////////////////////////////////////////////////////
//	Function declerations

int pfsFsckStat(pfs_mount_t *pfsMount, pfs_super_block_t *superblock, u32 stat, int mode);

void *pfsAllocMem(int size);
void pfsFreeMem(void *buffer);
int pfsGetTime(pfs_datetime_t *tm);
void pfsPrintBitmap(const u32 *bitmap);

pfs_block_device_t *pfsGetBlockDeviceTable(const char *name);
int pfsGetScale(int num, int size);
u32 pfsFixIndex(u32 index);

#endif /* _LIBPFS_H */
