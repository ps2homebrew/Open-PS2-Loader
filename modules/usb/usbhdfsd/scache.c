//---------------------------------------------------------------------------
//File name:    scache.c
//---------------------------------------------------------------------------
/*
 * scache.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 * (C) 2004  Hermes (support for sector sizes from 512 to 4096 bytes)
 *
 * Sector cache
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */
//---------------------------------------------------------------------------
#include <stdio.h>

#ifdef WIN32
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#else
#include <tamtypes.h>
#include <sysmem.h>
#endif

//#define SCACHE_RECORD_STATS	1

#include <usbhdfsd.h>
#include "usbhd_common.h"
#include "mass_stor.h"

//---------------------------------------------------------------------------
#define READ_SECTOR		mass_stor_readSector
#define WRITE_SECTOR	mass_stor_writeSector

//#define DEBUG  //comment out this line when not debugging

#include "mass_debug.h"

#define BLOCK_SIZE 4096

//number of cache slots (1 slot = block)
#define CACHE_SIZE 32

//when the flushCounter reaches FLUSH_TRIGGER then flushSectors is called
//#define FLUSH_TRIGGER 16

typedef struct _cache_record
{
	unsigned int sector;
	int tax;
	char writeDirty;
} cache_record;

struct _cache_set
{
	mass_dev* dev;
	unsigned int sectorSize;
	unsigned int indexLimit;
	unsigned char* sectorBuf; // = NULL;		//sector content - the cache buffer
	cache_record rec[CACHE_SIZE];	//cache info record

#ifdef SCACHE_RECORD_STATS
	//statistical information
	unsigned int cacheAccess;
	unsigned int cacheHits;
#endif
	unsigned int writeFlag;
};

//---------------------------------------------------------------------------
static void initRecords(cache_set* cache)
{
	unsigned int i;

	for (i = 0; i < CACHE_SIZE; i++)
	{
		cache->rec[i].sector = 0xFFFFFFF0;
		cache->rec[i].tax = 0;
		cache->rec[i].writeDirty = 0;
	}

	cache->writeFlag = 0;
}

//---------------------------------------------------------------------------
/* search cache records for the sector number stored in cache
  returns cache record (slot) number
 */
static int getSlot(cache_set* cache, unsigned int sector) {
	int i;

	for (i = 0; i < CACHE_SIZE; i++) {
		if (sector >= cache->rec[i].sector && sector < (cache->rec[i].sector + cache->indexLimit)) {
			return i;
		}
	}
	return -1;
}

//---------------------------------------------------------------------------
/* search cache records for the sector number stored in cache */
static int getIndexRead(cache_set* cache, unsigned int sector) {
	unsigned int i;
	int index =-1;

	for (i = 0; i < CACHE_SIZE; i++) {
		if (sector >= cache->rec[i].sector && sector < (cache->rec[i].sector + cache->indexLimit)) {
			if (cache->rec[i].tax < 0) cache->rec[i].tax = 0;
			cache->rec[i].tax +=2;
			index = i;
		}
		cache->rec[i].tax--;     //apply tax penalty
	}
	if (index < 0)
		return index;
	else
		return ((index * cache->indexLimit) + (sector - cache->rec[index].sector));
}

//---------------------------------------------------------------------------
/* select the best record where to store new sector */
static int getIndexWrite(cache_set* cache, unsigned int sector) {
	int ret;
	unsigned int minTax = 0x0FFFFFFF;
	unsigned int i, index = 0;

	for (i = 0; i < CACHE_SIZE; i++) {
		if (cache->rec[i].tax < minTax) {
			index = i;
			minTax = cache->rec[i].tax;
		}
	}

	//this sector is dirty - we need to flush it first
	if (cache->rec[index].writeDirty) {
		XPRINTF("scache: getIndexWrite: sector is dirty : %u   index=%d \n", cache->rec[index].sector, index);
		ret = WRITE_SECTOR(cache->dev, cache->rec[index].sector, cache->sectorBuf + (index * BLOCK_SIZE), BLOCK_SIZE/cache->sectorSize);
		if (ret < 0) {
			printf("scache: ERROR writing sector to disk! sector=%u\n", sector);
			return ret;
		}

		cache->rec[index].writeDirty = 0;
	}
	cache->rec[index].tax +=2;
	cache->rec[index].sector = sector;

	return index * cache->indexLimit;
}

//---------------------------------------------------------------------------
/*
	flush dirty sectors
 */
int scache_flushSectors(cache_set* cache) {
	unsigned int i;
	int counter = 0, ret;

	XPRINTF("cache: flushSectors devId = %i \n", cache->dev->devId);

	XPRINTF("scache: flushSectors writeFlag=%d\n", cache->writeFlag);
	//no write operation occured since last flush
	if (cache->writeFlag==0) {
		return 0;
	}

	for (i = 0; i < CACHE_SIZE; i++) {
		if (cache->rec[i].writeDirty) {
			XPRINTF("scache: flushSectors dirty index=%d sector=%u \n", i, cache->rec[i].sector);
			ret = WRITE_SECTOR(cache->dev, cache->rec[i].sector, cache->sectorBuf + (i * BLOCK_SIZE), BLOCK_SIZE/cache->sectorSize);
			if (ret < 0) {
				printf("scache: ERROR writing sector to disk! sector=%u\n", cache->rec[i].sector);
				return ret;
			}

			cache->rec[i].writeDirty = 0;
			counter ++;
		}
	}
	cache->writeFlag = 0;
	return counter;
}

//---------------------------------------------------------------------------
int scache_readSector(cache_set* cache, unsigned int sector, void** buf) {
	int index; //index is given in single sectors not octal sectors
	int ret;
	unsigned int alignedSector;

	XPRINTF("cache: readSector devId = %i %p sector = %u \n", cache->dev->devId, cache, sector);
	if (cache == NULL) {
		printf("cache: devId cache not created = %i \n", cache->dev->devId);
		return -1;
	}

#ifdef SCACHE_RECORD_STATS
	cache->cacheAccess ++;
#endif
	index = getIndexRead(cache, sector);
	XPRINTF("cache: indexRead=%i \n", index);
	if (index >= 0) { //sector found in cache
#ifdef SCACHE_RECORD_STATS
		cache->cacheHits ++;
#endif
		*buf = cache->sectorBuf + (index * cache->sectorSize);
		XPRINTF("cache: hit and done reading sector \n");

		return cache->sectorSize;
	}

	//compute alignedSector - to prevent storage of duplicit sectors in slots
	alignedSector = (sector/cache->indexLimit)*cache->indexLimit;
	index = getIndexWrite(cache, alignedSector);
	XPRINTF("cache: indexWrite=%i slot=%d  alignedSector=%u\n", index, index / cache->indexLimit, alignedSector);
	ret = READ_SECTOR(cache->dev, alignedSector, cache->sectorBuf + (index * cache->sectorSize), BLOCK_SIZE/cache->sectorSize);

	if (ret < 0) {
		printf("scache: ERROR reading sector from disk! sector=%u\n", alignedSector);
		return ret;
	}
	*buf = cache->sectorBuf + (index * cache->sectorSize) + ((sector%cache->indexLimit) * cache->sectorSize);
	XPRINTF("cache: done reading physical sector \n");

	//write precaution
	//cache->flushCounter++;
	//if (cache->flushCounter == FLUSH_TRIGGER) {
		//scache_flushSectors(cache);
	//}

	return cache->sectorSize;
}


//---------------------------------------------------------------------------
int scache_allocSector(cache_set* cache, unsigned int sector, void** buf) {
	int index; //index is given in single sectors not octal sectors
	//int ret;
	unsigned int alignedSector;

	XPRINTF("cache: allocSector devId = %i sector = %u \n", cache->dev->devId, sector);

	index = getIndexRead(cache, sector);
	XPRINTF("cache: indexRead=%i \n", index);
	if (index >= 0) { //sector found in cache
		*buf = cache->sectorBuf + (index * cache->sectorSize);
		XPRINTF("cache: hit and done allocating sector \n");
		return cache->sectorSize;
	}

	//compute alignedSector - to prevent storage of duplicit sectors in slots
	alignedSector = (sector/cache->indexLimit)*cache->indexLimit;
	index = getIndexWrite(cache, alignedSector);
	XPRINTF("cache: indexWrite=%i \n", index);
	*buf = cache->sectorBuf + (index * cache->sectorSize) + ((sector%cache->indexLimit) * cache->sectorSize);
	XPRINTF("cache: done allocating sector\n");
	return cache->sectorSize;
}

//---------------------------------------------------------------------------
int scache_writeSector(cache_set* cache, unsigned int sector) {
	int index; //index is given in single sectors not octal sectors
	//int ret;

	XPRINTF("cache: writeSector devId = %i sector = %u \n", cache->dev->devId, sector);

	index = getSlot(cache, sector);
	if (index <  0) { //sector not found in cache
		printf("cache: writeSector: ERROR! the sector is not allocated! \n");
		return -1;
	}
	XPRINTF("cache: slotFound=%i \n", index);

	//prefere written sectors to stay in cache longer than read sectors
	cache->rec[index].tax += 2;

	//set dirty status
	cache->rec[index].writeDirty = 1;
	cache->writeFlag++;

	XPRINTF("cache: done soft writing sector \n");

	//write precaution
	//cache->flushCounter++;
	//if (cache->flushCounter == FLUSH_TRIGGER) {
		//scache_flushSectors(devId);
	//}

	return cache->sectorSize;
}

//---------------------------------------------------------------------------
cache_set* scache_init(mass_dev* dev, int sectSize)
{
	cache_set* cache;
	XPRINTF("cache: init devId = %i sectSize = %u \n", dev->devId, sectSize);

	cache = malloc(sizeof(cache_set));
	if (cache == NULL) {
		printf("scache init! Sector cache: can't alloate cache!\n");
		return NULL;
	}

	XPRINTF("scache init! \n");
	cache->dev = dev;

	cache->sectorBuf = (unsigned char*) malloc(BLOCK_SIZE * CACHE_SIZE);
	if (cache->sectorBuf == NULL) {
		printf("Sector cache: can't alloate memory of size:%d \n", BLOCK_SIZE * CACHE_SIZE);
		free(cache);
		return NULL;
	}
	XPRINTF("Sector cache: allocated memory at:%p of size:%d \n", cache->sectorBuf, BLOCK_SIZE * CACHE_SIZE);

	//added by Hermes
	cache->sectorSize = sectSize;
	cache->indexLimit = BLOCK_SIZE/cache->sectorSize; //number of sectors per 1 cache slot
#ifdef SCACHE_RECORD_STATS
	cache->cacheAccess = 0;
	cache->cacheHits = 0;
#endif
	initRecords(cache);
	return cache;
}

#ifdef SCACHE_RECORD_STATS
//---------------------------------------------------------------------------
void scache_getStat(cache_set* cache, unsigned int* access, unsigned int* hits) {
	*access = cache->cacheAccess;
	*hits = cache->cacheHits;
}
#endif

//---------------------------------------------------------------------------
void scache_kill(cache_set* cache) //dlanor: added for disconnection events (flush impossible)
{
	XPRINTF("cache: kill devId = %i \n", cache->dev->devId);
	if(cache->sectorBuf != NULL)
	{
		free(cache->sectorBuf);
		cache->sectorBuf = NULL;
	}
	free(cache);
}
//---------------------------------------------------------------------------
void scache_close(cache_set* cache)
{
	XPRINTF("cache: close devId = %i \n", cache->dev->devId);
	scache_flushSectors(cache);
	scache_kill(cache);
}
//---------------------------------------------------------------------------
//End of file:  scache.c
//---------------------------------------------------------------------------
