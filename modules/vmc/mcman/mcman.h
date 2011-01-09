/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#ifndef __MCMAN_H__
#define __MCMAN_H__

#include <loadcore.h>
#include <intrman.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>
#include <timrman.h>
#include <modload.h>
#include "modload_add.h"
#include <ioman.h>
#include "secrman.h"
#include <cdvdman.h>
#include "cdvdman_add.h"
#include <stdio.h>
#include <errno.h>
#include <io_common.h>
#include "sio2man_imports.h"

#ifdef SIO_DEBUG
	#include <sior.h>
	#define DEBUG
	#define DPRINTF(args...)	sio_printf(args)
#else
	#define DPRINTF(args...)	printf(args)
#endif

#define MODNAME "mcman"
#define MODVER  0x101

// modInfo struct returned by xmcman exports 42
struct modInfo_t { 
	const char *name;
	u16 version;
};

struct irx_export_table _exp_mcman;

typedef struct _sceMcStDateTime {
	u8  Resv2;
	u8  Sec;
	u8  Min;
	u8  Hour;
	u8  Day;
	u8  Month;
	u16 Year;
} sceMcStDateTime;

/* MCMAN public structure */
typedef struct _sceMcTblGetDir { //size = 64
	sceMcStDateTime _Create; // 0
	sceMcStDateTime _Modify; // 8
	u32 FileSizeByte;		 // 16	
	u16 AttrFile;			 // 20
	u16 Reserve1;			 // 22
	u32 Reserve2;			 // 24
	u32 PdaAplNo;			 // 28
	u8  EntryName[32];		 // 32
} sceMcTblGetDir;

typedef struct _MCCacheEntry {
	int  cluster;   // 0
	u8  *cl_data;   // 4
	u16  mc_slot;   // 8
	u8   wr_flag;   // 10
	u8   mc_port;   // 11
	u8   rd_flag;   // 12
	u8   unused[3]; // 13
} McCacheEntry;

typedef struct _MCCacheDir {
	int  cluster;   // 0
	int  fsindex;   // 4
	int  maxent;    // 8
	u32  unused;
} McCacheDir;

// Card Flags
#define CF_USE_ECC   				0x01
#define CF_BAD_BLOCK 				0x08
#define CF_ERASE_ZEROES 			0x10

#define MCMAN_MAXSLOT				4
#define MCMAN_CLUSTERSIZE 			1024
#define MCMAN_CLUSTERFATENTRIES		256

typedef struct _McFatCluster {
	int entry[MCMAN_CLUSTERFATENTRIES];
} McFatCluster;


#define MAX_CACHEENTRY 			0x24
u8 mcman_cachebuf[MAX_CACHEENTRY * MCMAN_CLUSTERSIZE];
McCacheEntry mcman_entrycache[MAX_CACHEENTRY];
McCacheEntry *mcman_mccache[MAX_CACHEENTRY];

McCacheEntry *pmcman_entrycache;
McCacheEntry **pmcman_mccache;

typedef struct {
	int entry[1 + (MCMAN_CLUSTERFATENTRIES * 2)];
} McFatCache;

McFatCache mcman_fatcache[2][MCMAN_MAXSLOT];

typedef struct {				// size = 128
	int mode;					// 0
	int length;					// 4
	s16 linked_block;			// 8
	u8  name[20];				// 10
	u8  field_1e;				// 30
	u8  field_1f;				// 31
	sceMcStDateTime created;	// 32
	int field_28;				// 40
	u16 field_2c;				// 44
	u16 field_2e;				// 46
	sceMcStDateTime modified;   // 48
	int field_38;				// 56
	u8  unused2[65];			// 60	
	u8  field_7d;				// 125
	u8  field_7e;				// 126
	u8  edc;					// 127 
} McFsEntryPS1;

typedef struct {				// size = 512
	u16 mode;					// 0
	u16 unused;					// 2	
	u32 length;					// 4
	sceMcStDateTime created;	// 8
	u32 cluster;				// 16
	u32 dir_entry;				// 20
	sceMcStDateTime modified;	// 24
	u32 attr;					// 32
	u32 unused2[7];				// 36
	u8  name[32];				// 64
	u8  unused3[416];			// 96
} McFsEntry;

#define MAX_CACHEDIRENTRY 		0x3
McFsEntry mcman_dircache[MAX_CACHEDIRENTRY];

int mcman_curdirmaxent;
int mcman_curdirlength;
char mcman_curdirpath[1024];
char *mcman_curdirname;

int mcman_PS1curcluster;
u8 mcman_PS1curdir[64];

typedef struct {  // size = 48		
	u8  status;   // 0
	u8  wrflag;   // 1
	u8  rdflag;   // 2
	u8  unknown1; // 3 
	u8  drdflag;  // 4
	u8  unknown2; // 5
	u16 port;     // 6
	u16 slot;     // 8
	u16 unknown3; // 10 
	u32 position; // 12 
	u32 filesize; // 16
	u32 freeclink; // 20 link to next free cluster
	u32 clink;	  // 24  link to next cluster
	u32 clust_offset;// 28
	u32 field_20; // 32
	u32 field_24; // 36
	u32 field_28; // 40
	u32 field_2C; // 44
} MC_FHANDLE;

#define MAX_FDHANDLES 		3
MC_FHANDLE mcman_fdhandles[MAX_FDHANDLES];

sceMcStDateTime mcman_fsmodtime;

/* MCMAN EXPORTS */
/* 05 */ int  McDetectCard(int port, int slot);
/* 06 */ int  McOpen(int port, int slot, char *filename, int flags);
/* 07 */ int  McClose(int fd);
/* 08 */ int  McRead(int fd, void *buf, int length);
/* 09 */ int  McWrite(int fd, void *buf, int length);
/* 10 */ int  McSeek(int fd, int offset, int origin);
/* 11 */ int  McFormat(int port, int slot);
/* 12 */ int  McGetDir(int port, int slot, char *dirname, int flags, int maxent, sceMcTblGetDir *info);
/* 13 */ int  McDelete(int port, int slot, char *filename, int flags);
/* 14 */ int  McFlush(int fd);
/* 15 */ int  McChDir(int port, int slot, char *newdir, char *currentdir);
/* 16 */ int  McSetFileInfo(int port, int slot, char *filename, sceMcTblGetDir *info, int flags);
/* 17 */ int  McEraseBlock(int port, int block, void **pagebuf, void *eccbuf);
/* 18 */ int  McReadPage(int port, int slot, int page, void *buf);
/* 19 */ int  McWritePage(int port, int slot, int page, void *pagebuf, void *eccbuf);
/* 20 */ void McDataChecksum(void *buf, void *ecc);
/* 29 */ int  McReadPS1PDACard(int port, int slot, int page, void *buf);
/* 30 */ int  McWritePS1PDACard(int port, int slot, int page, void *buf);
/* 36 */ int  McUnformat(int port, int slot);
/* 37 */ int  McRetOnly(int fd);
/* 38 */ int  McGetFreeClusters(int port, int slot);
/* 39 */ int  McGetMcType(int port, int slot);
/* 40 */ void McSetPS1CardFlag(int flag);

/* Available in XMCMAN only */
/* 17 */ int  McEraseBlock2(int port, int slot, int block, void **pagebuf, void *eccbuf);
/* 21 */ int  McDetectCard2(int port, int slot);
/* 22 */ int  McGetFormat(int port, int slot);
/* 23 */ int  McGetEntSpace(int port, int slot, char *dirname);
/* 24 */ int  mcman_replacebadblock(void);
/* 25 */ int  McCloseAll(void);
/* 42 */ struct modInfo_t *McGetModuleInfo(void);
/* 43 */ int  McGetCardSpec(int port, int slot, u16 *pagesize, u16 *blocksize, int *cardsize, u8 *flags);
/* 44 */ int  mcman_getFATentry(int port, int slot, int fat_index, int *fat_entry);
/* 45 */ int  McCheckBlock(int port, int slot, int block);
/* 46 */ int  mcman_setFATentry(int port, int slot, int fat_index, int fat_entry);
/* 47 */ int  mcman_readdirentry(int port, int slot, int cluster, int fsindex, McFsEntry **pfse);
/* 48 */ void mcman_1stcacheEntsetwrflagoff(void);


// internal functions prototypes
int  mcsio2_transfer(int port, int slot, sio2_transfer_data_t *sio2data);
int  mcsio2_transfer2(int port, int slot, sio2_transfer_data_t *sio2data);
void long_multiply(u32 v1, u32 v2, u32 *HI, u32 *LO);
int  mcman_chrpos(char *str, int chr);
void mcman_wmemset(void *buf, int size, int value);
int  mcman_calcEDC(void *buf, int size);
int  mcman_checkpath(char *str);
int  mcman_checkdirpath(char *str1, char *str2);
void mcman_invhandles(int port, int slot);
int  McCloseAll(void);
int  mcman_detectcard(int port, int slot);
int  mcman_dread(int fd, fio_dirent_t *dirent);
int  mcman_getstat(int port, int slot, char *filename, fio_stat_t *stat);
int  mcman_getmcrtime(sceMcStDateTime *time);
void mcman_initPS2com(void);
void sio2packet_add(int port, int slot, int cmd, u8 *buf);
int  mcman_eraseblock(int port, int slot, int block, void **pagebuf, void *eccbuf);
int  mcman_readpage(int port, int slot, int page, void *buf, void *eccbuf);
int  mcman_cardchanged(int port, int slot);
int  mcman_resetauth(int port, int slot);
int  mcman_probePS2Card2(int port, int slot);
int  mcman_probePS2Card(int port, int slot);
int  secrman_mc_command(int port, int slot, sio2_transfer_data_t *sio2data);
int  mcman_getcnum (int port, int slot);
int  mcman_correctdata(void *buf, void *ecc);
int  mcman_sparesize(int port, int slot);
int  mcman_setdevspec(int port, int slot);
int  mcman_reportBadBlocks(int port, int slot);
int  mcman_setdevinfos(int port, int slot);
int  mcman_format2(int port, int slot);
int  mcman_createDirentry(int port, int slot, int parent_cluster, int num_entries, int cluster, sceMcStDateTime *ctime);
int  mcman_fatRseek(int fd);
int  mcman_fatWseek(int fd);
int  mcman_findfree2(int port, int slot, int reserve);
int  mcman_dread2(int fd, fio_dirent_t *dirent);
int  mcman_getstat2(int port, int slot, char *filename, fio_stat_t *stat);
int  mcman_setinfo2(int port, int slot, char *filename, sceMcTblGetDir *info, int flags);
int  mcman_read2(int fd, void *buffer, int nbyte);
int  mcman_write2(int fd, void *buffer, int nbyte);
int  mcman_close2(int fd);
int  mcman_getentspace(int port, int slot, char *dirname);
int  mcman_cachedirentry(int port, int slot, char *filename, McCacheDir *pcacheDir, McFsEntry **pfse, int unknown_flag);
int  mcman_getdirinfo(int port, int slot, McFsEntry *pfse, char *filename, McCacheDir *pcd, int unknown_flag);
int  mcman_open2(int port, int slot, char *filename, int flags);
int  mcman_chdir(int port, int slot, char *newdir, char *currentdir);
int  mcman_writecluster(int port, int slot, int cluster, int flag);
int  mcman_setdirentrystate(int port, int slot, int cluster, int fsindex, int flags);
int  mcman_getdir2(int port, int slot, char *dirname, int flags, int maxent, sceMcTblGetDir *info);
int  mcman_delete2(int port, int slot, char *filename, int flags);
int  mcman_checkBackupBlocks(int port, int slot);
int  mcman_unformat2(int port, int slot);
void mcman_initPS1PDAcom(void);
int  mcman_probePS1Card2(int port, int slot);
int  mcman_probePS1Card(int port, int slot);
int  mcman_probePDACard(int port, int slot);
int  mcman_setPS1devinfos(int port, int slot);
int  mcman_format1(int port, int slot);
int  mcman_open1(int port, int slot, char *filename, int flags);
int  mcman_read1(int fd, void *buffer, int nbyte);
int  mcman_write1(int fd, void *buffer, int nbyte);
int  mcman_getPS1direntry(int port, int slot, char *filename, McFsEntryPS1 **pfse, int flag);
int  mcman_dread1(int fd, fio_dirent_t *dirent);
int  mcman_getstat1(int port, int slot, char *filename, fio_stat_t *stat);
int  mcman_setinfo1(int port, int slot, char *filename, sceMcTblGetDir *info, int flags);
int  mcman_getdir1(int port, int slot, char *dirname, int flags, int maxent, sceMcTblGetDir *info);
int  mcman_clearPS1direntry(int port, int slot, int cluster, int flags);
int  mcman_delete1(int port, int slot, char *filename, int flags);
int  mcman_close1(int fd);
int  mcman_findfree1(int port, int slot, int reserve);
int  mcman_fatRseekPS1(int fd);
int  mcman_fatWseekPS1(int fd);
int  mcman_FNC8ca4(int port, int slot, MC_FHANDLE *fh);
int  mcman_PS1pagetest(int port, int slot, int page);
int  mcman_unformat1(int port, int slot);
int  mcman_cachePS1dirs(int port, int slot);
int  mcman_fillPS1backuparea(int port, int slot, int block);
void mcman_initcache(void);
int  mcman_clearcache(int port, int slot);
McCacheEntry *mcman_getcacheentry(int port, int slot, int cluster);
void mcman_freecluster(int port, int slot, int cluster);
int  mcman_getFATindex(int port, int slot, int num);
McCacheEntry *mcman_get1stcacheEntp(void);
void mcman_addcacheentry(McCacheEntry *mce);
int  mcman_flushmccache(int port, int slot);
int  mcman_flushcacheentry(McCacheEntry *mce);
int  mcman_readcluster(int port, int slot, int cluster, McCacheEntry **pmce);
int  mcman_readdirentryPS1(int port, int slot, int cluster, McFsEntryPS1 **pfse);
int  mcman_readclusterPS1(int port, int slot, int cluster, McCacheEntry **pmce);
int  mcman_replaceBackupBlock(int port, int slot, int block);
int  mcman_fillbackupblock1(int port, int slot, int block, void **pagedata, void *eccdata);
int  mcman_clearsuperblock(int port, int slot);
int  mcman_ioerrcode(int errcode);
int  mcman_modloadcb(char *filename, int *unit, u8 *arg3); // used as callback by modload
void mcman_unit2card(u32 unit);
int  mcman_initdev(void);


// in addition to errno
#define EFORMAT						  140

// MCMAN basic error codes
#define sceMcResSucceed               0
#define sceMcResChangedCard          -1
#define sceMcResNoFormat             -2
#define sceMcResFullDevice           -3
#define sceMcResNoEntry              -4
#define sceMcResDeniedPermit         -5
#define sceMcResNotEmpty             -6
#define sceMcResUpLimitHandle        -7
#define sceMcResFailReplace          -8
#define sceMcResFailResetAuth        -11
#define sceMcResFailDetect        	 -12
#define sceMcResFailDetect2        	 -13
#define sceMcResDeniedPS1Permit    	 -51
#define sceMcResFailAuth        	 -90


// Memory Card device types
#define sceMcTypeNoCard               0
#define sceMcTypePS1                  1
#define sceMcTypePS2                  2
#define sceMcTypePDA                  3


typedef struct { 				// size = 384
	u8  magic[28];				// Superblock magic, on PS2 MC : "Sony PS2 Memory Card Format "
    u8  version[12];  			// Version number of the format used, 1.2 indicates full support for bad_block_list
    s16 pagesize;				// size in bytes of a memory card page
    u16 pages_per_cluster;		// number of pages in a cluster
    u16 blocksize;				// number of pages in an erase block
    u16 unused;					// unused
    u32 clusters_per_card;		// total size in clusters of the memory card
    u32 alloc_offset;			// Cluster offset of the first allocatable cluster. Cluster values in the FAT and directory entries are relative to this. This is the cluster immediately after the FAT
    u32 alloc_end;				// The cluster after the highest allocatable cluster. Relative to alloc_offset. Not used
    u32 rootdir_cluster;		// First cluster of the root directory. Relative to alloc_offset. Must be zero
    u32 backup_block1;			// Erase block used as a backup area during programming. Normally the the last block on the card, it may have a different value if that block was found to be bad
    u32 backup_block2;			// This block should be erased to all ones. Normally the the second last block on the card
    u8  unused2[8];
    u32 ifc_list[32];			// List of indirect FAT clusters. On a standard 8M card there's only one indirect FAT cluster
    int bad_block_list[32];		// List of erase blocks that have errors and shouldn't be used
    u8  cardtype;				// Memory card type. Must be 2, indicating that this is a PS2 memory card
    u8  cardflags;				// Physical characteristics of the memory card 
    u16 unused3;
    u32 cluster_size;
    u32 FATentries_per_cluster;
    u32 clusters_per_block;
    int cardform;
    u32 rootdir_cluster2;
    u32 unknown1;
    u32 unknown2;
    u32 max_allocatable_clusters;
    u32 unknown3;
    u32 unknown4;
    int unknown5;
} MCDevInfo;

MCDevInfo mcman_devinfos[4][MCMAN_MAXSLOT];


/* High-Level File I/O */
#define SCE_CST_MODE                  0x01
#define SCE_CST_ATTR                  0x02
#define SCE_CST_SIZE                  0x04
#define SCE_CST_CT                    0x08
#define SCE_CST_AT                    0x10
#define SCE_CST_MT                    0x20
#define SCE_CST_PRVT                  0x40

#define SCE_STM_R                     0x01
#define SCE_STM_W                     0x02
#define SCE_STM_X                     0x04
#define SCE_STM_C                     0x08
#define SCE_STM_F                     0x10
#define SCE_STM_D                     0x20

/* file attributes */
#define sceMcFileAttrReadable         SCE_STM_R
#define sceMcFileAttrWriteable        SCE_STM_W
#define sceMcFileAttrExecutable       SCE_STM_X
#define sceMcFileAttrDupProhibit      SCE_STM_C
#define sceMcFileAttrFile             SCE_STM_F
#define sceMcFileAttrSubdir           SCE_STM_D
#define sceMcFileCreateDir            0x0040
#define sceMcFileAttrClosed           0x0080
#define sceMcFileCreateFile           0x0200
#define sceMcFile0400		          0x0400
#define sceMcFileAttrPDAExec          0x0800
#define sceMcFileAttrPS1              0x1000
#define sceMcFileAttrHidden           0x2000
#define sceMcFileAttrExists           0x8000


sio2_transfer_data_t mcman_sio2packet;  // buffer for mcman sio2 packet
u8 mcman_wdmabufs[0x0b * 0x90]; 		// buffer array for SIO2 DMA I/O (write)
u8 mcman_rdmabufs[0x0b * 0x90]; 		// not sure here for size, buffer array for SIO2 DMA I/O (read)

sio2_transfer_data_t mcman_sio2packet_PS1PDA; 
u8 mcman_sio2inbufs_PS1PDA[0x90];
u8 mcman_sio2outbufs_PS1PDA[0x90];

u8 mcman_pagebuf[1056]; 
void *mcman_pagedata[32]; 
u8 mcman_eccdata[512]; // size for 32 ecc

u8 mcman_backupbuf[16384];

u8 mcman_PS1PDApagebuf[128];

u32 mcman_timercount;
int mcman_timerthick;

int mcman_badblock_port;
int mcman_badblock_slot;
int mcman_badblock;
int mcman_replacementcluster[16];

int (*mcman_sio2transfer)(int port, int slot, sio2_transfer_data_t *sio2data);
int (*mc_detectcard)(int port, int slot);

#endif
