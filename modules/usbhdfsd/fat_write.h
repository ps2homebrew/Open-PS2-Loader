#ifndef _FAT_WRITE_H
#define _FAT_WRITE_H 1

unsigned char toUpperChar(unsigned char c);
int fat_createFile(fat_driver* fatd, const unsigned char* fname, char directory, char escapeNotExist, unsigned int* cluster, unsigned int* sfnSector, int* sfnOffset);
int fat_deleteFile(fat_driver* fatd, const unsigned char* fname, char directory);
int fat_truncateFile(fat_driver* fatd, unsigned int cluster, unsigned int sfnSector, int sfnOffset );
int fat_writeFile(fat_driver* fatd, fat_dir* fatDir, int* updateClusterIndices, unsigned int filePos, unsigned char* buffer, int size) ;
int fat_updateSfn(fat_driver* fatd, int size, unsigned int sfnSector, int sfnOffset );

int fat_allocSector(fat_driver* fatd, unsigned int sector, unsigned char** buf);
int fat_writeSector(fat_driver* fatd, unsigned int sector);
int fat_flushSectors(fat_driver* fatd);
#endif /* _FAT_WRITE_H */
