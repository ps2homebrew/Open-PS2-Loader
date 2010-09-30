#ifndef _FAT_DRIVER_H
#define _FAT_DRIVER_H 1

#define DIR_CHAIN_SIZE 32

#define FAT_MAX_NAME 128

//attributes (bits:5-Archive 4-Directory 3-Volume Label 2-System 1-Hidden 0-Read Only)
#define FAT_ATTR_READONLY     0x01
#define FAT_ATTR_HIDDEN       0x02
#define FAT_ATTR_SYSTEM       0x04
#define FAT_ATTR_VOLUME_LABEL 0x08
#define FAT_ATTR_DIRECTORY    0x10
#define FAT_ATTR_ARCHIVE      0x20

typedef struct _fat_bpb {
	unsigned int  sectorSize;	//bytes per sector - should be 512
	unsigned char clusterSize;	//sectors per cluster - power of two
	unsigned int  resSectors;	//reserved sectors - typically 1 (boot sector)
	unsigned char fatCount;		//number of FATs - must be 2
	unsigned int  rootSize;		//number of rootdirectory entries - typically 512
	unsigned int  fatSize;		//sectors per FAT - varies
	unsigned int  trackSize;	//sectors per track
	unsigned int  headCount;	//number of heads
	unsigned int  sectorCount;	//number of sectors
	unsigned int  partStart;	//sector where partition starts (boot sector)
	unsigned int  rootDirStart;	//sector where root directory starts
	unsigned int  rootDirCluster;	//fat32 - cluster of the root directory
	unsigned int  activeFat;	//fat32 - current active fat number
	unsigned char fatType;		//12-FAT16, 16-FAT16, 32-FAT32
	unsigned char fatId[9];		//File system ID. "FAT12", "FAT16" or "FAT  " - for debug only
	unsigned int  dataStart;	//sector where data starts
} fat_bpb;

typedef struct _fat_driver {
	mass_dev* dev;
	fat_bpb  partBpb;	//partition bios parameter block

	// modified by Hermes
#define MAX_DIR_CLUSTER 512
	unsigned int cbuf[MAX_DIR_CLUSTER]; //cluster index buffer // 2048 by Hermes

	unsigned int  lastChainCluster;
	int lastChainResult;

/* enough for long filename of length 260 characters (20*13) and one short filename */
#define MAX_DE_STACK 21
	unsigned int deSec[MAX_DE_STACK]; //direntry sector
	int          deOfs[MAX_DE_STACK]; //direntry offset
	int          deIdx; //direntry index

#define SEQ_MASK_SIZE 2048         //Allow 2K files per directory
	u8 seq_mask[SEQ_MASK_SIZE/8];      //bitmask for consumed seq numbers
#define DIR_MASK_SIZE 2048*11      //Allow 2K maxed fullnames per directory
	u8 dir_used_mask[DIR_MASK_SIZE/8]; //bitmask for used directory entries

#define MAX_CLUSTER_STACK 128
	unsigned int clStack[MAX_CLUSTER_STACK]; //cluster allocation stack
	int clStackIndex;
	unsigned int clStackLast; // last free cluster of the fat table
} fat_driver;

typedef struct _fat_dir_list {
	unsigned int  direntryCluster; //the directory cluster requested by getFirstDirentry
	int direntryIndex; //index of the directory children
} fat_dir_list;

typedef struct _fat_dir_chain_record {
	unsigned int cluster;
	unsigned int index;
} fat_dir_chain_record;

typedef struct _fat_dir {
	unsigned char attr;		//attributes (bits:5-Archive 4-Directory 3-Volume Label 2-System 1-Hidden 0-Read Only)
	unsigned char name[FAT_MAX_NAME];
	unsigned char cdate[4];	//D:M:Yl:Yh
	unsigned char ctime[3]; //H:M:S
	unsigned char adate[4];	//D:M:Yl:Yh
	unsigned char atime[3]; //H:M:S
	unsigned char mdate[4];	//D:M:Yl:Yh
	unsigned char mtime[3]; //H:M:S
	unsigned int  size;		//file size, 0 for directory
	unsigned int  lastCluster;
	fat_dir_chain_record  chain[DIR_CHAIN_SIZE];  //cluser/offset cache - for seeking purpose
} fat_dir;

int strEqual(const unsigned char *s1, const unsigned char* s2);

int fat_mount(mass_dev* dev, unsigned int start, unsigned int count);
void fat_forceUnmount(mass_dev* dev);
void fat_setFatDirChain(fat_driver* fatd, fat_dir* fatDir);
int fat_readFile(fat_driver* fatd, fat_dir* fatDir, unsigned int filePos, unsigned char* buffer, unsigned int size);
int fat_getFirstDirentry(fat_driver* fatd, const unsigned char* dirName, fat_dir_list* fatdlist, fat_dir* fatDir);
int fat_getNextDirentry(fat_driver* fatd, fat_dir_list* fatdlist, fat_dir* fatDir);

fat_driver * fat_getData(int device);
int      fat_getFileStartCluster(fat_driver* fatd, const unsigned char* fname, unsigned int* startCluster, fat_dir* fatDir);
int      fat_getClusterChain(fat_driver* fatd, unsigned int cluster, unsigned int* buf, int bufSize, int start);

#endif

