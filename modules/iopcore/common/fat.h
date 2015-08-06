#define MAX_DIR_CLUSTER		128	//Number of clusters to record, whenever the FAT chain is referenced (One typical 512-byte sector = 128 = 512/4).
/* Performance will be bad on a heavily-fragmented disk because of the number of FAT sectors to be read,
	so limit the number of FAT sectors that will be scanned through in an attempt to prevent performance from being affected too much.	*/
#define MAX_FRAG_FAT_SECTORS	1
#define OPL_DIR_CHAIN_SIZE	128	//Number of points to record, along a file's FAT chain (for quicker seeking).
#define FAT_IO_BLOCK_SIZE	4096	//Maximum size for USB transfers

#define FAT_IO_MODE_READ	0
#define FAT_IO_MODE_WRITE	1

//Special part numbers for the VMCs.
#define FAT_PARTNUM_VMC0	0x0080
#define FAT_PARTNUM_VMC1	0x0081

typedef struct _fat_bpb {
	unsigned int fatStart;		//sector where FAT starts (partition start + reserved sectors)
	unsigned int dataStart;		//sector where data starts
	unsigned short int sectorSize;	//bytes per sector - should be 512
	unsigned char clusterSize;	//sectors per cluster - power of two
} fat_bpb;

typedef struct _fat_driver {
	mass_dev *dev;
	fat_bpb partBpb;	//partition bios parameter block

	unsigned int cbuf[MAX_DIR_CLUSTER]; //cluster index buffer
	unsigned int lastChainCluster;
	int lastChainResult;

	unsigned int lastCBufCluster, lastCBufClusterOffset;
	unsigned short int lastCBufCount, FilePartNum;
} fat_driver;

typedef struct _fat_dir_chain_record {
	unsigned int cluster;
	unsigned int index;
} fat_dir_chain_record;

typedef struct _fat_dir {
	unsigned int numPoints;
	fat_dir_chain_record *points;  //cluser/offset cache - for seeking purpose
} fat_dir;

//---------------------------------------------------------------------------
static inline unsigned int fat_cluster2sector(fat_bpb* partBpb, unsigned int cluster)
{
	return  partBpb->dataStart + (partBpb->clusterSize * (cluster-2));
}

void fat_init(mass_dev *dev);
void fat_deinit(void);
void fat_setFatDirChain(fat_dir* fatDir, unsigned int cluster, unsigned int size, unsigned int numChainPoints, fat_dir_chain_record *chainPointsBuf);
int fat_fileIO(fat_dir* fatDir, unsigned short int part_num, short int mode, unsigned int filePos, unsigned char* buffer, unsigned int size);
