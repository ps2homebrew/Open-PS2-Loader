#ifndef _SCACHE_H
#define _SCACHE_H 1

cache_set* scache_init(mass_dev* dev, int sectorSize);
void scache_close(cache_set* cache);
void scache_kill(cache_set* cache); //dlanor: added for disconnection events (flush impossible)
int  scache_allocSector(cache_set* cache, unsigned int sector, void** buf);
int  scache_readSector(cache_set* cache, unsigned int sector, void** buf);
int  scache_writeSector(cache_set* cache, unsigned int sector);
int  scache_flushSectors(cache_set* cache);

void scache_getStat(cache_set* cache, unsigned int* access, unsigned int* hits);

#endif 
