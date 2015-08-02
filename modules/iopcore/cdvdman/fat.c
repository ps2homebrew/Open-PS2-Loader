#include <errno.h>
#include <intrman.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thsemap.h>
#include <stdio.h>
#include <usbd.h>

#include "mass_debug.h"
#include "mass_common.h"
#include "mass_stor.h"
#include "smsutils.h"
#include "fat.h"
#include "cdvd_config.h"

extern struct cdvdman_settings_usb cdvdman_settings;

static fat_driver gFatd;
static u8 *gSbuf;

#define FAT_END_OF_CHAIN	0x00004000

#ifdef VMC_DRIVER
static int io_sema;

#define WAITIOSEMA(x) WaitSema(x)
#define SIGNALIOSEMA(x) SignalSema(x)
#else
#define WAITIOSEMA(x)
#define SIGNALIOSEMA(x)
#endif

static int fat_getClusterChain(fat_driver* fatd, unsigned int cluster, unsigned int* buf, int startFlag);

//Set chain info (cluster/offset) cache
void fat_setFatDirChain(fat_dir* fatDir, unsigned int cluster, unsigned int size, unsigned int numChainPoints, fat_dir_chain_record *chainPointsBuf) {
	unsigned int index, clusterChainStart, fileCluster, fileSize, blockSize;
	int i, j, chainSize, chainSizeResult;
	unsigned char nextChain;

	XPRINTF("USBHDFSD: reading cluster chain  \n");
	fatDir->numPoints = numChainPoints;
	fatDir->points = chainPointsBuf;
	fatDir->points[0].cluster = fileCluster = cluster;
	fatDir->points[0].index = 0;

	blockSize = size / fatDir->numPoints;
	nextChain = 1;
	clusterChainStart = 0;
	j = 1;
	fileSize = 0;
	index = 0;

	WAITIOSEMA(io_sema);

	//Invalidate information on the cluster buffer, since the cluster buffer will be used to scan through an entire file.
	gFatd.lastCBufCluster = 0;
	gFatd.lastCBufClusterOffset = 0;
	gFatd.lastCBufCount = 0;
	gFatd.FilePartNum = 0;

	while (nextChain) {
		if((chainSizeResult = fat_getClusterChain(&gFatd, fileCluster, gFatd.cbuf, 1)) >=0){
			chainSize = chainSizeResult & ~FAT_END_OF_CHAIN;

			if (!(chainSizeResult & FAT_END_OF_CHAIN)) { //the chain is full, but more chain parts exist
				fileCluster = gFatd.cbuf[chainSize - 1];
			}else { //chain fits in the chain buffer completely - no next chain exist
				nextChain = 0;
			}
		}else{
			XPRINTF("USBHDFSD: fat_setFatDirChain(): fat_getClusterChain() failed: %d\n", chainSizeResult);
			SIGNALIOSEMA(io_sema);
			return;
		}

		//process the cluster chain (gFatd.cbuf)
		for (i = clusterChainStart; i < chainSize; i++) {
			fileSize += (gFatd.partBpb.clusterSize * gFatd.partBpb.sectorSize);
			while (fileSize >= (j * blockSize) && j < fatDir->numPoints) {
				fatDir->points[j].cluster = gFatd.cbuf[i];
				fatDir->points[j].index = index;
				j++;
			}//ends "while"
			index++;
		}//ends "for"
		clusterChainStart = 1;
	}//ends "while"

	XPRINTF("USBHDFSD: read cluster chain  done!\n");

	SIGNALIOSEMA(io_sema);
}

void fat_init(mass_dev *dev){
	int OldState;
#ifdef VMC_DRIVER
	iop_sema_t smp;

	smp.initial = 1;
	smp.max = 1;
	smp.option = 0;
	smp.attr = SA_THPRI;
	io_sema = CreateSema(&smp);
#endif

	CpuSuspendIntr(&OldState);
	gSbuf = AllocSysMemory(ALLOC_FIRST, dev->sectorSize, NULL);
	CpuResumeIntr(OldState);

	//Initialize fat driver
	gFatd.dev = dev;
	gFatd.partBpb.fatStart = cdvdman_settings.fatStart;
	gFatd.partBpb.dataStart = cdvdman_settings.dataStart;
	gFatd.partBpb.clusterSize = cdvdman_settings.clusterSize;
	gFatd.partBpb.sectorSize = dev->sectorSize;
}

void fat_deinit(void){
	int OldState;

	if(gSbuf != NULL)
	{
		CpuSuspendIntr(&OldState);
		FreeSysMemory(gSbuf);
		CpuResumeIntr(OldState);
	}
}

//for fat32
static inline int fat_getClusterChain32(fat_driver* fatd, unsigned int cluster, unsigned int* buf, int startFlag) {
	unsigned int indexCount, fatSector, lastFatSector, FatSectorCount;
	int i;
	unsigned char cont;

	cont = 1;
	FatSectorCount = 0;
	indexCount = fatd->partBpb.sectorSize / 4; //FAT16->2, FAT32->4
	lastFatSector = -1;
	i = 0;
	if (startFlag) {
		buf[i] = cluster; //store first cluster
		i++;
	}
	while(i < MAX_DIR_CLUSTER && cont) {
		fatSector = cluster / indexCount;
		if (lastFatSector !=  fatSector) {
			if(FatSectorCount > MAX_FRAG_FAT_SECTORS)
				break;

			mass_stor_readSector(fatd->partBpb.fatStart + fatSector, 1, gSbuf);
			lastFatSector = fatSector;
			FatSectorCount++;
		}
		cluster = getI32(gSbuf + ((cluster % indexCount) * 4));
		if ((cluster & 0xFFFFFFF) >= 0xFFFFFF8) {
			i |= FAT_END_OF_CHAIN;
			cont = 0; //continue = false
		} else {
			buf[i] = cluster & 0xFFFFFFF;
			i++;
		}
	}
	return i;
}

//---------------------------------------------------------------------------
static int fat_getClusterChain(fat_driver* fatd, unsigned int cluster, unsigned int* buf, int startFlag) {
	//This operation is a fairly time-consuming operation. Avoid having to re-read the cluster chain if possible.
	if (cluster == fatd->lastChainCluster) {
		return fatd->lastChainResult;
	}

	fatd->lastChainResult = fat_getClusterChain32(fatd, cluster, buf, startFlag);
	fatd->lastChainCluster = cluster;
	return fatd->lastChainResult;
}

//---------------------------------------------------------------------------
static inline void fat_getClusterAtFilePos(fat_driver* fatd, fat_dir* fatDir, unsigned int filePos, unsigned int* cluster, unsigned int* clusterPos) {
	unsigned int i, j, blockSize;

	blockSize = fatd->partBpb.clusterSize * fatd->partBpb.sectorSize;

	for (i = 0, j = (fatDir->numPoints-1); i < (fatDir->numPoints-1); i++) {
		if (fatDir->points[i].index   * blockSize <= filePos &&
			fatDir->points[i+1].index * blockSize >  filePos) {
				j = i;
				break;
			}
	}
	*cluster    = fatDir->points[j].cluster;
	*clusterPos = (fatDir->points[j].index * blockSize);
}

//---------------------------------------------------------------------------
int fat_fileIO(fat_dir* fatDir, unsigned short int part_num, short int mode, unsigned int filePos, unsigned char* buffer, unsigned int size) {
	int chainSize, chainSizeResult;
	unsigned int startSector, clusterChainStart, bufSize, sectorSkip, clusterSkip, blockSize;
	unsigned short int i, j;
	unsigned char nextChain, blockSizeSectors;
	mass_dev* mass_device = gFatd.dev;
	blockSize = gFatd.partBpb.clusterSize * gFatd.partBpb.sectorSize;

	unsigned int bufferPos, fileCluster, clusterPos;

	WAITIOSEMA(io_sema);

	/*	filePos		->	Offset in the file, to read/write to (in bytes).
		fileCluster	->	Cluster number of the cluster that is closest to the region that is to be read/written too.
		clusterPos	->	The offset (in bytes) in the file, that the cluster <fileCluster> begins at.
		clusterSkip	->	Number of clusters to skip from cluster <fileCluster>,
					to get to the cluster where offset <filePos> is located in.
		sectorSkip	->	Number of sectors to skip from the sum of cluster <fileCluster> and <clusterSkip>,
					 to get to the sector where offset <filePos> is located in.	*/

	//Avoid seeking, if possible.
	if((part_num == gFatd.FilePartNum)  && (filePos >= gFatd.lastCBufClusterOffset * blockSize) && (filePos  < (gFatd.lastCBufClusterOffset + (gFatd.lastCBufCount & ~FAT_END_OF_CHAIN)) * blockSize)){
		//Totally within the cluster buffer. Data can be reused.
		fileCluster = gFatd.lastCBufCluster;
		clusterPos = gFatd.lastCBufClusterOffset * blockSize;
	}else{
		//Requested data is out of range.
		fat_getClusterAtFilePos(&gFatd, fatDir, filePos, &fileCluster, &clusterPos);
		gFatd.FilePartNum = part_num;
	}

	sectorSkip = (filePos - clusterPos) / gFatd.partBpb.sectorSize;
	clusterSkip = sectorSkip / gFatd.partBpb.clusterSize;
	sectorSkip %= gFatd.partBpb.clusterSize;
	bufferPos = 0;

	XPRINTF("USBHDFSD: fileCluster = %u,  clusterPos= %u clusterSkip=%u, sectorSkip=%u\n",
		fileCluster, clusterPos, clusterSkip, sectorSkip);

	nextChain = 1;
	clusterChainStart = 1;

	while (nextChain && size > 0 ) {
		//Check if the data is within range of the data currently in the cluster buffer (cbuf).
		chainSize = (chainSizeResult = gFatd.lastCBufCount) & ~FAT_END_OF_CHAIN;
		if(fileCluster == gFatd.lastCBufCluster && (clusterSkip < chainSize)) {
			clusterChainStart = 0;
			if (!(chainSizeResult & FAT_END_OF_CHAIN)) { //the chain is full, but more chain parts exist
				fileCluster = gFatd.cbuf[chainSize - 1];
			}else { //chain fits in the chain buffer completely - no next chain needed
				nextChain = 0;
			}
		}else{
			//Keep skipping clusters until the clusters to be read/written to are within range.
			while(1){
				if((chainSizeResult = fat_getClusterChain(&gFatd, fileCluster, gFatd.cbuf, clusterChainStart))<0){
					SIGNALIOSEMA(io_sema);
					return chainSizeResult;
				}
				chainSize = chainSizeResult & ~FAT_END_OF_CHAIN;

				clusterChainStart = 0;
				if (!(chainSizeResult & FAT_END_OF_CHAIN)) { //the chain is full, but more chain parts exist
					fileCluster = gFatd.cbuf[chainSize - 1];
				}else { //chain fits in the chain buffer completely - no next chain needed
					nextChain = 0;
				}

				if (clusterSkip < MAX_DIR_CLUSTER)
					break;

				clusterPos += (unsigned int)chainSize * blockSize;
				clusterSkip -= chainSize;
			}

			//Record information on the cluster chain data in cbuf, so that it can be reused.
			gFatd.lastCBufCluster = gFatd.cbuf[0];
			gFatd.lastCBufClusterOffset = clusterPos / blockSize;
			gFatd.lastCBufCount = chainSizeResult;
		}

		//process the cluster chain (gFatd.cbuf) and skip leading clusters if needed
		for (i = 0 + clusterSkip; i < chainSize && size > 0; i++) {
			//read cluster and save cluster content
			startSector = fat_cluster2sector(&gFatd.partBpb, gFatd.cbuf[i]);
			//process all sectors of the cluster (and skip leading sectors if needed)
			for (j = 0 + sectorSkip; j < gFatd.partBpb.clusterSize && size > 0; j+=blockSizeSectors) {
				bufSize = (gFatd.partBpb.clusterSize - j) * mass_device->sectorSize;

				//compute exact size of transfered bytes
				if (size < bufSize)
					bufSize = size;
				if (bufSize > FAT_IO_BLOCK_SIZE)	//Limit per transfer (USB).
					bufSize = FAT_IO_BLOCK_SIZE;
				blockSizeSectors = bufSize / mass_device->sectorSize;

				if(blockSizeSectors < 1) {
					//This should only ever happen at the very end of the read request.
					if(mode == FAT_IO_MODE_READ)
						mass_stor_readSector(startSector + j, 1, gSbuf);
#ifdef VMC_DRIVER
					else
						mass_stor_writeSector(startSector + j, 1, gSbuf);
#endif
					mips_memcpy(buffer + bufferPos, gSbuf, bufSize);
				} else {
					if(mode == FAT_IO_MODE_READ)
						mass_stor_readSector(startSector + j, blockSizeSectors, buffer+bufferPos);
#ifdef VMC_DRIVER
					else
						mass_stor_writeSector(startSector + j, blockSizeSectors, buffer+bufferPos);
#endif
				}

				size-= bufSize;
				bufferPos +=  bufSize;
			}
			sectorSkip = 0;
		}
		clusterSkip = 0;
	}

	SIGNALIOSEMA(io_sema);

	return bufferPos;
}
