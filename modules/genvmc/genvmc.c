/*
  Copyright 2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include <stdio.h>
#include <loadcore.h>
#include <ioman.h>
#include "ioman_add.h"
#include <io_common.h>
#include <intrman.h>
#include <thsemap.h>
#include <sysclib.h>
#include <sysmem.h>
#include <errno.h>

#include "genvmc.h"

#define MODNAME "genvmc"
IRX_ID(MODNAME, 1, 1);

// driver ops protypes
int genvmc_dummy(void);
int genvmc_init(iop_device_t *dev);
int genvmc_deinit(iop_device_t *dev);
int genvmc_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen);

// driver ops func tab
void *genvmc_ops[27] = {
	(void*)genvmc_init,
	(void*)genvmc_deinit,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_devctl,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy,
	(void*)genvmc_dummy
};

// driver descriptor
static iop_ext_device_t genvmc_dev = {
	"genvmc", 
	IOP_DT_FS | IOP_DT_FSEXT,
	1,
	"genvmc",
	(struct _iop_ext_device_ops *)&genvmc_ops
};

static int genvmc_io_sema;
static int genvmc_fh = -1;

// mc file attributes
#define SCE_STM_R                     0x01
#define SCE_STM_W                     0x02
#define SCE_STM_X                     0x04
#define SCE_STM_C                     0x08
#define SCE_STM_F                     0x10
#define SCE_STM_D                     0x20
#define sceMcFileAttrReadable         SCE_STM_R
#define sceMcFileAttrWriteable        SCE_STM_W
#define sceMcFileAttrExecutable       SCE_STM_X
#define sceMcFileAttrDupProhibit      SCE_STM_C
#define sceMcFileAttrFile             SCE_STM_F
#define sceMcFileAttrSubdir           SCE_STM_D
#define sceMcFileCreateDir            0x0040
#define sceMcFileAttrClosed           0x0080
#define sceMcFileCreateFile           0x0200
#define sceMcFile0400	              0x0400
#define sceMcFileAttrPDAExec          0x0800
#define sceMcFileAttrPS1              0x1000
#define sceMcFileAttrHidden           0x2000
#define sceMcFileAttrExists           0x8000

// SONY superblock magic & version
static char SUPERBLOCK_MAGIC[]   = "Sony PS2 Memory Card Format ";
static char SUPERBLOCK_VERSION[] = "1.2.0.0";

// superblock struct
typedef struct { 			// size = 384
	u8  magic[28];			// Superblock magic, on PS2 MC : "Sony PS2 Memory Card Format "
	u8  version[12];		// Version number of the format used, 1.2 indicates full support for bad_block_list
	s16 pagesize;			// size in bytes of a memory card page
	u16 pages_per_cluster;		// number of pages in a cluster
	u16 blocksize;			// number of pages in an erase block
	u16 unused;			// unused
	u32 clusters_per_card;		// total size in clusters of the memory card
	u32 alloc_offset;		// Cluster offset of the first allocatable cluster. Cluster values in the FAT and directory entries are relative to this. This is the cluster immediately after the FAT
	u32 alloc_end;			// The cluster after the highest allocatable cluster. Relative to alloc_offset. Not used
	u32 rootdir_cluster;		// First cluster of the root directory. Relative to alloc_offset. Must be zero
	u32 backup_block1;		// Erase block used as a backup area during programming. Normally the the last block on the card, it may have a different value if that block was found to be bad
	u32 backup_block2;		// This block should be erased to all ones. Normally the the second last block on the card
	u8  unused2[8];
	u32 ifc_list[32];		// List of indirect FAT clusters. On a standard 8M card there's only one indirect FAT cluster
	int bad_block_list[32];		// List of erase blocks that have errors and shouldn't be used
	u8  cardtype;			// Memory card type. Must be 2, indicating that this is a PS2 memory card
	u8  cardflags;			// Physical characteristics of the memory card 
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

typedef struct _sceMcStDateTime {
	u8  Resv2;
	u8  Sec;
	u8  Min;
	u8  Hour;
	u8  Day;
	u8  Month;
	u16 Year;
} sceMcStDateTime;

typedef struct {			// size = 512
	u16 mode;			// 0
	u16 unused;			// 2	
	u32 length;			// 4
	sceMcStDateTime created;	// 8
	u32 cluster;			// 16
	u32 dir_entry;			// 20
	sceMcStDateTime modified;	// 24
	u32 attr;			// 32
	u32 unused2[7];			// 36
	char name[32];			// 64
	u8  unused3[416];		// 96
} McFsEntry;

static MCDevInfo devinfo;
static u8 cluster_buf[8192];

// from cdvdman
typedef struct {
	u8 stat;  			
	u8 second; 			
	u8 minute; 			
	u8 hour; 			
	u8 week; 			
	u8 day; 			
	u8 month; 			
	u8 year; 			
} cd_clock_t;

int sceCdRC(cd_clock_t *rtc);		 // #51

//-------------------------------------------------------------- 
static void long_multiply(u32 v1, u32 v2, u32 *HI, u32 *LO)
{
	register long a, b, c, d;
	register long x, y;

	a = (v1 >> 16) & 0xffff;
	b = v1 & 0xffff;
	c = (v2 >> 16) & 0xffff;
	d = v2 & 0xffff;

	*LO = b * d;   
	x = a * d + c * b;
	y = ((*LO >> 16) & 0xffff) + x;

	*LO = (*LO & 0xffff) | ((y & 0xffff) << 16);
	*HI = (y >> 16) & 0xffff;

	*HI += a * c;     
}

//--------------------------------------------------------------
static int mc_getmcrtime(sceMcStDateTime *time)
{
	register int retries;
	cd_clock_t cdtime;

	retries = 64;

	do {
		if (sceCdRC(&cdtime))
			break;
	} while (--retries > 0);

	if (cdtime.stat & 128) {
		*((u16 *)&cdtime.month) = 0x7d0;
		cdtime.day = 3;
		cdtime.week = 4;
		cdtime.hour = 0;
		cdtime.minute = 0;
		cdtime.second = 0;
		cdtime.stat = 0;
	}

	time->Resv2 = 0;
	time->Sec = ((((cdtime.second >> 4) << 2) + (cdtime.second >> 4)) << 1) + (cdtime.second & 0xf);
	time->Min = ((((cdtime.minute >> 4) << 2) + (cdtime.minute >> 4)) << 1) + (cdtime.minute & 0xf);
	time->Hour = ((((cdtime.hour >> 4) << 2) + (cdtime.hour >> 4)) << 1) + (cdtime.hour & 0xf);
	time->Day = ((((cdtime.day >> 4) << 2) + (cdtime.day >> 4)) << 1) + (cdtime.day & 0xf);

	if ((cdtime.month & 0x10) != 0)
		time->Month = (cdtime.month & 0xf) + 0xa;
	else	
		time->Month = cdtime.month & 0xf;

	time->Year = ((((cdtime.year >> 4) << 2) + (cdtime.year >> 4)) << 1) + ((cdtime.year & 0xf) | 0x7d0);

	return 0;	
}

//--------------------------------------------------------------
static int mc_writecluster(int cluster, void *buf)
{
	register int r;
	MCDevInfo *mcdi = (MCDevInfo *)&devinfo;

	lseek(genvmc_fh, cluster * mcdi->cluster_size, SEEK_SET);
	r = write(genvmc_fh, buf, mcdi->cluster_size);
	if (r != mcdi->cluster_size)
		return -1;

	return 0;
}

//--------------------------------------------------------------
static int vmc_create(int size_kb, int blocksize)
{
	register int i, b, allocatable_clusters_per_card, ifc_index, fat_index;
	register int ifc_length, fat_length, fat_entry, alloc_offset;
	register int j = 0, z = 0;
	int oldstate;
	MCDevInfo *mcdi = (MCDevInfo *)&devinfo;

	// set superblock magic & version
	memset((void *)&mcdi->magic, 0, sizeof (mcdi->magic) + sizeof (mcdi->version));
	strcpy((char *)&mcdi->magic, SUPERBLOCK_MAGIC);
	strcat((char *)&mcdi->magic, SUPERBLOCK_VERSION);

	// set mc specs
	mcdi->cluster_size = 1024; 	// size in KB of clusters
	mcdi->blocksize = blocksize;	// how many pages in a block of data
	mcdi->pages_per_cluster = 2;	// how many pages in a cluster
	mcdi->pagesize = mcdi->cluster_size / mcdi->pages_per_cluster;
	mcdi->clusters_per_block = mcdi->blocksize / mcdi->pages_per_cluster;
	mcdi->clusters_per_card = (size_kb*1024) / mcdi->cluster_size;
	mcdi->cardtype = 0x02;		// PlayStation2 card type
	mcdi->cardflags = 0x2b;
	mcdi->cardform = -1;
	mcdi->FATentries_per_cluster = mcdi->cluster_size / sizeof(u32);

	// clear bad blocks list
	for (i=0; i<32; i++)
		mcdi->bad_block_list[i] = -1;

	// erase all clusters
	memset(cluster_buf, 0xff, mcdi->cluster_size);
	for (i=0; i<mcdi->clusters_per_card; i++)
		mc_writecluster(i, cluster_buf);

	// calculate fat & ifc length
	fat_length = (((mcdi->clusters_per_card << 2) - 1) / mcdi->cluster_size) + 1; 	// get length of fat in clusters
	ifc_length = (((fat_length << 2) - 1) / mcdi->cluster_size) + 1; 		// get number of needed ifc clusters

	if (!(ifc_length <= 32)) {
		ifc_length = 32;
		fat_length = mcdi->FATentries_per_cluster << 5;
	}

	// clear ifc list
	for (i=0; i<32; i++)
		mcdi->ifc_list[i] = -1;
	ifc_index = mcdi->blocksize / 2;
	i = ifc_index;
	for (j=0; j<ifc_length; j++, i++)
		mcdi->ifc_list[j] = i;

	// keep fat cluster index
	fat_index = i;

	// allocate memory for ifc clusters
	CpuSuspendIntr(&oldstate);
	u8 *ifc_mem = AllocSysMemory(ALLOC_FIRST, (ifc_length * mcdi->cluster_size)+0XFF, NULL);
	CpuResumeIntr(oldstate);
	if (!ifc_mem)
		return -1;
	memset(ifc_mem, 0, ifc_length * mcdi->cluster_size);

	// build ifc clusters
	u32 *ifc = (u32 *)ifc_mem;
	for (j=0; j<fat_length; j++, i++) {
		// just as security...
		if (i >= mcdi->clusters_per_card) {
			CpuSuspendIntr(&oldstate);
			FreeSysMemory(ifc_mem);
			CpuResumeIntr(oldstate);
			return -2;
		}
		ifc[j] = i;
	}

	// write ifc clusters
	for (z=0; z<ifc_length; z++)
		mc_writecluster(mcdi->ifc_list[z], &ifc_mem[z * mcdi->cluster_size]);

	// freeing ifc clusters memory
	CpuSuspendIntr(&oldstate);
	FreeSysMemory(ifc_mem);
	CpuResumeIntr(oldstate);

	// set alloc offset
	alloc_offset = i;

	// set backup blocks
	mcdi->backup_block1 = (mcdi->clusters_per_card / mcdi->clusters_per_block) - 1;
	mcdi->backup_block2 = (mcdi->clusters_per_card / mcdi->clusters_per_block) - 2;

	// calculate number of allocatable clusters per card
	u32 hi, lo, temp;
	long_multiply(mcdi->clusters_per_card, 0x10624dd3, &hi, &lo);
	temp = (hi >> 6) - (mcdi->clusters_per_card >> 31);
	allocatable_clusters_per_card = (((((temp << 5) - temp) << 2) + temp) << 3) + 1;
	j = alloc_offset;

	// allocate memory for fat clusters
	CpuSuspendIntr(&oldstate);
	u8 *fat_mem = AllocSysMemory(ALLOC_FIRST, (fat_length * mcdi->cluster_size)+0XFF, NULL);
	CpuResumeIntr(oldstate);
	if (!fat_mem)
		return -3;
	memset(fat_mem, 0, fat_length * mcdi->cluster_size);

	// building FAT
	u32 *fc = (u32 *)fat_mem;
	i = (mcdi->clusters_per_card / mcdi->clusters_per_block) - 2; // 2 backup blocks
	if (j < i * mcdi->clusters_per_block) {
		z = 0;
		do {
			if (z == 0) {
				mcdi->alloc_offset = j;
				mcdi->rootdir_cluster = 0;
				fat_entry = 0xffffffff; // marking rootdir end
			}
			else 
				fat_entry = 0x7fffffff;	// marking free cluster
			z++;
			if (z == allocatable_clusters_per_card)
				mcdi->max_allocatable_clusters = (j - mcdi->alloc_offset) + 1;
			
			fc[j - mcdi->alloc_offset] = fat_entry;
			j++;	
		} while (j < (i * mcdi->clusters_per_block));
	}

	// write fat clusters
	for (b=0; b<fat_length; b++)
		mc_writecluster(fat_index + b, &fat_mem[b * mcdi->cluster_size]);

	// freeing fat clusters memory
	CpuSuspendIntr(&oldstate);
	FreeSysMemory(fat_mem);
	CpuResumeIntr(oldstate);

	// calculate alloc_end
	mcdi->alloc_end = (i * mcdi->clusters_per_block) - mcdi->alloc_offset;

	// calculate max allocatable clusters
	if (mcdi->max_allocatable_clusters == 0)		
		mcdi->max_allocatable_clusters = i * mcdi->clusters_per_block;

	// just a security...
	if (z < mcdi->clusters_per_block)
		return -4;

	mcdi->unknown1 = 0;
	mcdi->unknown2 = 0;
	mcdi->unknown5 = -1;
	mcdi->rootdir_cluster2 = mcdi->rootdir_cluster;

	// build root directory
	McFsEntry *rootdir_entry[2];
	sceMcStDateTime time;

	mc_getmcrtime(&time);
	rootdir_entry[0] = (McFsEntry *)&cluster_buf[0];
	rootdir_entry[1] = (McFsEntry *)&cluster_buf[sizeof(McFsEntry)];
	memset((void *)rootdir_entry[0], 0, sizeof(McFsEntry));
	memset((void *)rootdir_entry[1], 0, sizeof(McFsEntry));
	rootdir_entry[0]->mode = sceMcFileAttrExists | sceMcFile0400 | sceMcFileAttrSubdir | sceMcFileAttrReadable | sceMcFileAttrWriteable | sceMcFileAttrExecutable;
	rootdir_entry[0]->length = 2;
	memcpy((void *)&rootdir_entry[0]->created, (void *)&time, sizeof(sceMcStDateTime));
	memcpy((void *)&rootdir_entry[0]->modified, (void *)&time, sizeof(sceMcStDateTime));
	rootdir_entry[0]->cluster = 0;
	rootdir_entry[0]->dir_entry = 0;
	strcpy(rootdir_entry[0]->name, ".");
	rootdir_entry[1]->mode = sceMcFileAttrExists | sceMcFileAttrHidden | sceMcFile0400 | sceMcFileAttrSubdir | sceMcFileAttrWriteable | sceMcFileAttrExecutable;
	rootdir_entry[1]->length = 2;
	memcpy((void *)&rootdir_entry[1]->created, (void *)&time, sizeof(sceMcStDateTime));
	memcpy((void *)&rootdir_entry[1]->modified, (void *)&time, sizeof(sceMcStDateTime));
	rootdir_entry[1]->cluster = 0;
	rootdir_entry[1]->dir_entry = 0;
	strcpy(rootdir_entry[1]->name, "..");

	// write root directory cluster
	mc_writecluster(mcdi->alloc_offset + mcdi->rootdir_cluster, cluster_buf);

	// set superblock formatted flag
	mcdi->cardform = 1;

	// finally write superblock	
	memset(cluster_buf, 0xff, mcdi->cluster_size);
	memcpy(cluster_buf, (void *)mcdi, sizeof(MCDevInfo));
	mc_writecluster(0, cluster_buf);

	return 0;
}

//-------------------------------------------------------------- 
int genvmc_dummy(void)
{
	return -EPERM;
}

//-------------------------------------------------------------- 
int genvmc_init(iop_device_t *dev)
{
	genvmc_io_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

	return 0;
}

//-------------------------------------------------------------- 
int genvmc_deinit(iop_device_t *dev)
{
	DeleteSema(genvmc_io_sema);

	return 0;
}

//-------------------------------------------------------------- 
int genvmc_devctl(iop_file_t *f, const char *name, int cmd, void *args, u32 arglen, void *buf, u32 buflen)
{
	register int r = 0;
	createVMCparam_t *p;

	if (!name)
		return -ENOENT;

	WaitSema(genvmc_io_sema);

	switch (cmd) {

		case GENVMC_DEVCTL_CREATE_VMC:

			p = (createVMCparam_t *)args;

			genvmc_fh = open(p->VMC_filename, O_RDWR|O_CREAT|O_TRUNC);
			if (genvmc_fh < 0) {
				r = -EIO;
				break;
			}

			r = vmc_create(p->VMC_size_mb * 1024, p->VMC_blocksize);
			if (r < 0)
				r = -EIO;

			close(genvmc_fh);

			break;

		default:
			r = -EINVAL;
	}

	SignalSema(genvmc_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int _start(int argc, char** argv)
{
	DelDrv("genvmc");
	AddDrv((iop_device_t *)&genvmc_dev);

	return MODULE_RESIDENT_END;
}

//--------------------------------------------------------------
DECLARE_IMPORT_TABLE(cdvdman, 1, 1)
DECLARE_IMPORT(51, sceCdRC)
END_IMPORT_TABLE

