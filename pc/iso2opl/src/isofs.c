/*
  Copyright 2009, jimmikaelkael
  Copyright (c) 2002, A.Lee & Nicholas Van Veen  
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Some parts of the code are taken from libcdvd by A.Lee & Nicholas Van Veen
  Review license_libcdvd file for further details.
*/

#include "iso2opl.h"

static int isofs_inited = 0;
static int gIsBigEnd = 0;

#define MAX_DIR_CACHE_SECTORS 32

//int g_fh_iso;
FILE *g_fh_iso;

typedef struct {
	u8  status;
	u8  pad[3];
	u32 lsn;
	u32 position;
	u32 filesize;
} FHANDLE;

#define MAX_FDHANDLES 		32
FHANDLE isofs_fdhandles[MAX_FDHANDLES];

struct dir_cache_info {
	char pathname[1024];	// The pathname of the cached directory
	u32  valid;		// 1 if cache data is valid, 0 if not
	u32  path_depth;	// The path depth of the cached directory (0 = root)
	u32  sector_start;	// The start sector (LBA) of the cached directory
	u32  sector_num;	// The total size of the directory (in sectors)
	u32  cache_offset;	// The offset from sector_start of the cached area
	u32  cache_size;	// The size of the cached directory area (in sectors)
	u8  *cache;		// The actual cached data
};

static struct dir_cache_info CachedDirInfo;
static u8 isofs_dircache[MAX_DIR_CACHE_SECTORS * 2048];

enum Cache_getMode {
	CACHE_START = 0,
	CACHE_NEXT = 1
};

enum PathMatch {
	NOT_MATCH = 0,
	MATCH,
	SUBDIR
};

struct rootDirTocHeader {
	u16	length;
	u32 	tocLBA;
	u32 	tocLBA_bigend;
	u32 	tocSize;
	u32 	tocSize_bigend;
	u8	dateStamp[8];
	u8	reserved[6];
	u8	reserved2;
	u8	reserved3;
} __attribute__((packed));

struct asciiDate {
	char	year[4];
	char	month[2];
	char	day[2];
	char	hours[2];
	char	minutes[2];
	char	seconds[2];
	char	hundreths[2];
	char	terminator[1];
} __attribute__((packed));

struct cdVolDesc {
	u8		filesystemType;	// 0x01 = ISO9660, 0x02 = Joliet, 0xFF = NULL
	u8		volID[5];	// "CD001"
	u8		reserved2;
	u8		reserved3;
	u8		sysIdName[32];
	u8		volName[32];	// The ISO9660 Volume Name
	u8		reserved5[8];
	u32		volSize;	// Volume Size
	u32		volSizeBig;	// Volume Size Big-Endian
	u8		reserved6[32];
	u32		unknown1;
	u32		unknown1_bigend;
	u16		volDescSize;
	u16		volDescSize_bigend;
	u32		unknown3;
	u32		unknown3_bigend;
	u32		priDirTableLBA;	// LBA of Primary Dir Table
	u32		reserved7;
	u32		secDirTableLBA;	// LBA of Secondary Dir Table
	u32		reserved8;
	struct rootDirTocHeader	rootToc;
	u8		volSetName[128];
	u8		publisherName[128];
	u8		preparerName[128];
	u8		applicationName[128];
	u8		copyrightFileName[37];
	u8		abstractFileName[37];
	u8		bibliographyFileName[37];
	struct	asciiDate	creationDate;
	struct	asciiDate	modificationDate;
	struct	asciiDate	effectiveDate;
	struct	asciiDate	expirationDate;
	u8		reserved10;
	u8		reserved11[1166];
} __attribute__((packed));

struct dirTocEntry {
	short length;
	u32	 fileLBA;
	u32	 fileLBA_bigend;
	u32	 fileSize;
	u32	 fileSize_bigend;
	u8	 dateStamp[6];
	u8	 reserved1;
	u8	 fileProperties;
	u8	 reserved2[6];
	u8	 filenameLength;
	char 	 filename[128];
} __attribute__((packed));

struct TocEntry {
	u32	fileLBA;
	u32	fileSize;
	u8	dateStamp[6];
	u8 	padding0[2];
	u8	fileProperties;
	u8	padding1[3];
	char	filename[128+1];
	u8	padding2[3];
} __attribute__((packed));

static struct cdVolDesc CDVolDesc;

//-------------------------------------------------------------------------
u16 be16(u16 le_val)
{
	return ((le_val & 0xff) << 8) | ((le_val & 0xffff) >> 8);
}

//-------------------------------------------------------------------------
int isofs_ReadISO(s64 offset, u32 nbytes, void *buf)
{
	int r;

	#ifdef DEBUG
		printf("isofs_ReadISO: offset = %lld nbytes = %d\n", offset, nbytes);
	#endif

	//lseek64(g_fh_iso, offset, SEEK_SET);	 
	//r = read(g_fh_iso, buf, nbytes);
	fseeko64(g_fh_iso, offset, SEEK_SET);	 
	r = fread(buf, 1, nbytes, g_fh_iso);

/*
	int i;
	u8 *p = (u8 *)buf;
	for (i=0; i<nbytes; i++) {
		if ((i%16)==0)
			printf("\n");
		printf("%02x ", p[i]);
	}
	printf("\n");
*/
	return r;
}

//-------------------------------------------------------------------------
int isofs_ReadSect(u32 lsn, u32 nsectors, void *buf)
{
	register int r;
	register u32 nbytes;
	s64 offset;

	#ifdef DEBUG
		printf("isofs_ReadSect: LBA = %d nsectors = %d\n", lsn, nsectors);
	#endif

	offset = lsn * 2048;
	nbytes = nsectors * 2048;
	
	r = isofs_ReadISO(offset, nbytes, buf);
	if (r != nbytes)
		return 0;

	return 1;
}

//-------------------------------------------------------------------------
int isofs_FlushCache(void)
{
	strcpy(CachedDirInfo.pathname, "");		// The pathname of the cached directory
	CachedDirInfo.valid = 0;			// Cache is not valid
	CachedDirInfo.path_depth = 0;			// 0 = root)
	CachedDirInfo.sector_start = 0;			// The start sector (LBA) of the cached directory
	CachedDirInfo.sector_num = 0;			// The total size of the directory (in sectors)
	CachedDirInfo.cache_offset = 0;			// The offset from sector_start of the cached area
	CachedDirInfo.cache_size = 0;			// The size of the cached directory area (in sectors)

	memset(CachedDirInfo.cache, 0, MAX_DIR_CACHE_SECTORS * 2048);

	return 1;
}

//-------------------------------------------------------------------------
int isofs_GetVolumeDescriptor(void)
{
	// Read until we find the last valid Volume Descriptor
	register int volDescSector;
	static struct cdVolDesc localVolDesc;

	#ifdef DEBUG
		printf("isofs_GetVolumeDescriptor called\n");
	#endif

	for (volDescSector = 16; volDescSector < 20; volDescSector++) {
		isofs_ReadSect(volDescSector, 1, &localVolDesc);

		// If this is still a volume Descriptor
		if (strncmp((char *)localVolDesc.volID, "CD001", 5) == 0) {
			if ((localVolDesc.filesystemType == 1) ||
				(localVolDesc.filesystemType == 2))	{
				memcpy(&CDVolDesc, &localVolDesc, sizeof(struct cdVolDesc));
			}
		}
		else
			break;
	}

	#ifdef DEBUG
		switch (CDVolDesc.filesystemType) {
			case 1:
				printf("CD FileSystem is ISO9660\n");
				break;
			case 2:
				printf("CD FileSystem is Joliet\n");
				break;
			default:
				printf("CD FileSystem is unknown type\n");
				break;
		}
	#endif

	return 1;
}

//-------------------------------------------------------------- 
// Used in findfile
int strcasecmp(const char *s1, const char *s2)
{
	while (*s1 != '\0' && tolower(*s1) == tolower(*s2)) {
		s1++;
		s2++;
	}

	return tolower(*(u8 *) s1) - tolower(*(u8 *) s2);
}

//-------------------------------------------------------------- 
// Copy a TOC Entry from the CD native format to our tidier format
void TocEntryCopy(struct TocEntry* tocEntry, struct dirTocEntry* internalTocEntry)
{
	register int i;
	register int filenamelen;

	tocEntry->fileSize = (gIsBigEnd ? internalTocEntry->fileSize_bigend : internalTocEntry->fileSize);
	tocEntry->fileLBA = (gIsBigEnd ? internalTocEntry->fileLBA_bigend : internalTocEntry->fileLBA);
	tocEntry->fileProperties = internalTocEntry->fileProperties;

	if (CDVolDesc.filesystemType == 2) {
		// This is a Joliet Filesystem, so use Unicode to ISO string copy
		filenamelen = internalTocEntry->filenameLength / 2;

		for (i=0; i < filenamelen; i++)
			tocEntry->filename[i] = internalTocEntry->filename[(i * 2) + 1];
	}
	else {
		filenamelen = internalTocEntry->filenameLength;

		// use normal string copy
		strncpy(tocEntry->filename, internalTocEntry->filename, 128);
	}

	tocEntry->filename[filenamelen] = 0;

	if (!(tocEntry->fileProperties & 0x02))
		// strip the ;1 from the filename (if it's there)
		strtok(tocEntry->filename, ";");
}

//-------------------------------------------------------------- 
int FindPath(char *pathname)
{
	char *dirname;
	char *seperator;
	int dir_entry;
	int found_dir;
	struct dirTocEntry *tocEntryPointer;
	struct TocEntry localTocEntry;

	dirname = strtok(pathname,"\\/");

	while(dirname != NULL) {
		found_dir = 0;

		tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;

		// Always skip the first entry (self-refencing entry)
		tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + (gIsBigEnd ? be16(tocEntryPointer->length) : tocEntryPointer->length));

		dir_entry = 0;

		for ( ; (u8 *)tocEntryPointer < (CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048)); \
		tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + (gIsBigEnd ? be16(tocEntryPointer->length) : tocEntryPointer->length))) \
		{
			// If we have a null toc entry, then we've either reached the end of the dir, or have reached a sector boundary
			if ((gIsBigEnd ? be16(tocEntryPointer->length) : tocEntryPointer->length) == 0) {
				#ifdef DEBUG
					printf("Got a null pointer entry, so either reached end of dir, or end of sector\n");
				#endif

				tocEntryPointer = (struct dirTocEntry *)(CachedDirInfo.cache + (((((u8 *)tocEntryPointer - CachedDirInfo.cache) / 2048) + 1) * 2048));
			}

			if ((u8 *)tocEntryPointer >= (CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048))) {
				// If we've gone past the end of the cache
				// then check if there are more sectors to load into the cache

				if ((CachedDirInfo.cache_offset + CachedDirInfo.cache_size) < CachedDirInfo.sector_num) {
					// If there are more sectors to load, then load them
					CachedDirInfo.cache_offset += CachedDirInfo.cache_size;
					CachedDirInfo.cache_size = CachedDirInfo.sector_num - CachedDirInfo.cache_offset;

					if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
						CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

					if (isofs_ReadSect(CachedDirInfo.sector_start+CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache) != 1) {
						#ifdef DEBUG
							printf("Couldn't Read from CD !\n");
						#endif

						CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read time?
						return 0;
					}

					tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;
				}
				else {
					CachedDirInfo.valid = 0;
					return 0;
				}
			}

			// If the toc Entry is a directory ...
			if (tocEntryPointer->fileProperties & 0x02) {
				// Convert to our format (inc ascii name), for the check
				TocEntryCopy(&localTocEntry, tocEntryPointer);

				// If it's the link to the parent directory, then give it the name ".."
				if (dir_entry == 0) {
					if (CachedDirInfo.path_depth != 0) {
						#ifdef DEBUG
							printf("First directory entry in dir, so name it '..'\n");
						#endif

						strcpy(localTocEntry.filename, "..");
					}
				}

				// Check if this is the directory that we are looking for
				if (strcasecmp(dirname, localTocEntry.filename) == 0) {
					#ifdef DEBUG
						printf("Found the matching sub-directory\n");
					#endif

					found_dir = 1;

					if (dir_entry == 0) {
						// We've matched with the parent directory
						// so truncate the pathname by one level

						if (CachedDirInfo.path_depth > 0)
							CachedDirInfo.path_depth --;

						if (CachedDirInfo.path_depth == 0)
						{
							// If at root then just clear the path to root
							// (simpler than finding the colon seperator etc)
							CachedDirInfo.pathname[0] = 0;
						}
						else {
							seperator = strrchr(CachedDirInfo.pathname, '/');

							if (seperator != NULL)
								*seperator = 0;
						}
					}
					else {
						// otherwise append a seperator, and the matched directory
						// to the pathname
						strcat(CachedDirInfo.pathname, "/");

						#ifdef DEBUG
							printf("Adding '%s' to cached pathname - path depth = %d\n",dirname,CachedDirInfo.path_depth);
						#endif

						strcat(CachedDirInfo.pathname, dirname);

						CachedDirInfo.path_depth++;
					}

					// Exit out of the search loop
					// (and find the next sub-directory, if there is one)
					break;
				}
				else {
					#ifdef DEBUG
						printf("Found a directory, but it doesn't match\n");
					#endif
				}
			}
			dir_entry++;
		} // end of cache block search loop


		// if we've reached here, without finding the directory, then it's not there
		if (found_dir != 1) {
			CachedDirInfo.valid = 0;
			return 0;
		}

		// find name of next dir
		dirname = strtok(NULL,"\\/");

		CachedDirInfo.sector_start = localTocEntry.fileLBA;
		CachedDirInfo.sector_num = localTocEntry.fileSize / 2048;
		if (localTocEntry.fileSize & 0x7ff)
			CachedDirInfo.sector_num++;

		// Cache the start of the found directory
		// (used in searching if this isn't the last dir,
		// or used by whatever requested the cache in the first place if it is the last dir)
		CachedDirInfo.cache_offset = 0;
		CachedDirInfo.cache_size = CachedDirInfo.sector_num;

		if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
			CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

		if (isofs_ReadSect(CachedDirInfo.sector_start+CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache) != 1) {
			#ifdef DEBUG
				printf("Couldn't Read from CD, trying to read %d sectors, starting at sector %d !\n",
					CachedDirInfo.cache_size, CachedDirInfo.sector_start+CachedDirInfo.cache_offset);
			#endif

			CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read time?
			return 0;
		}
	}

	// If we've got here then we found the requested directory
	#ifdef DEBUG
		printf("FindPath found the path\n");
	#endif

	CachedDirInfo.valid = 1;
	return 1;
}

//-------------------------------------------------------------- 
enum PathMatch ComparePath(const char *path)
{
	register int length;
	register int i;

	length = strlen(CachedDirInfo.pathname);

	for (i=0;i<length;i++) {
		// check if character matches
		if (path[i] != CachedDirInfo.pathname[i]) {
			// if not, then is it just because of different path seperator ?
			if ((path[i] == '/') || (path[i] == '\\')) {
				if ((CachedDirInfo.pathname[i] == '/') || (CachedDirInfo.pathname[i]=='\\'))
					continue;
			}

			// if the characters don't match for any other reason then report a failure
			return NOT_MATCH;
		}
	}

	// Reached the end of the Cached pathname
	// if requested path is same length, then report exact match
	if (path[length] == 0)
		return MATCH;

	// if requested path is longer, and next char is a dir seperator
	// then report sub-dir match
	if ((path[length]=='/') || (path[length]=='\\'))
		return SUBDIR;
	else
		return NOT_MATCH;
}

//-------------------------------------------------------------- 
// Find, and cache, the requested directory, for use by GetDir or  (and thus open)
// provide an optional offset variable, for use when caching dirs of greater than 500 files

// returns TRUE if all TOC entries have been retrieved, or
// returns FALSE if there are more TOC entries to be retrieved
int isofs_Cache_Dir(const char *pathname, enum Cache_getMode getMode)
{
	// make sure that the requested pathname is not directly modified
	static char dirname[1024];
	int path_len;

	#ifdef DEBUG
		printf("Attempting to find, and cache, directory: %s\n", pathname);
	#endif

	// only take any notice of the existing cache, if it's valid
	if (CachedDirInfo.valid == 1) {
	// Check if the requested path is already cached
		if (ComparePath(pathname) == MATCH) {
			#ifdef DEBUG
				printf("CacheDir: The requested path is already cached\n");
			#endif

			// If so, is the request ot cache the start of the directory, or to resume the next block ?
			if (getMode == CACHE_START) {
				#ifdef DEBUG
					printf("          and requested cache from start of dir\n");
				#endif

				if (CachedDirInfo.cache_offset == 0) {
					// requested cache of start of the directory, and thats what's already cached
					// so sit back and do nothing
					#ifdef DEBUG
						printf("          and start of dir is already cached so nothing to do :o)\n");
					#endif
					
					CachedDirInfo.valid = 1;
					return 1;
				}
				else {
					// Requested cache of start of the directory, but thats not what's cached
					// so re-cache the start of the directory

					#ifdef DEBUG
						printf("          but dir isn't cached from start, so re-cache existing dir from start\n");
					#endif

					// reset cache data to start of existing directory
					CachedDirInfo.cache_offset = 0;
					CachedDirInfo.cache_size = CachedDirInfo.sector_num;

					if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
						CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

					// Now fill the cache with the specified sectors
					if (isofs_ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache) != 1) {
						#ifdef DEBUG
							printf("Couldn't Read from CD !\n");
						#endif

						CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read first time?
						return 0;
					}

					CachedDirInfo.valid = 1;
					return 1;
				}
			} 
			else {// getMode == CACHE_NEXT
				// So get the next block of the existing directory

				CachedDirInfo.cache_offset += CachedDirInfo.cache_size;

				CachedDirInfo.cache_size = CachedDirInfo.sector_num - CachedDirInfo.cache_offset;

				if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
					CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

				// Now fill the cache with the specified sectors
				if (isofs_ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache) != 1) {
					#ifdef DEBUG
						printf("Couldn't Read from CD !\n");
					#endif

					CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read first time?
					return 0;
				}

				CachedDirInfo.valid = 1;
				return 1;
			}
		} 
		else { // requested directory is not the cached directory (but cache is still valid)
			#ifdef DEBUG
				printf("Cache is valid, but cached directory, is not the requested one\n"
					"so check if the requested directory is a sub-dir of the cached one\n");

				printf("Requested Path = %s , Cached Path = %s\n",pathname, CachedDirInfo.pathname);
			#endif
			
			// Is the requested pathname a sub-directory of the current-directory ?

			// if the requested pathname is longer than the pathname of the cached dir
			// and the pathname of the cached dir matches the beginning of the requested pathname
			// and the next character in the requested pathname is a dir seperator

			if (ComparePath(pathname) == SUBDIR) {
				// If so then we can start our search for the path, from the currently cached directory
				#ifdef DEBUG
					printf("Requested dir is a sub-dir of the cached directory,\n"
						"so start search from current cached dir\n");
				#endif
				// if the cached chunk, is not the start of the dir, 
				// then we will need to re-load it before starting search
				if (CachedDirInfo.cache_offset != 0) {
					CachedDirInfo.cache_offset = 0;
					CachedDirInfo.cache_size = CachedDirInfo.sector_num;
					if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
						CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

					// Now fill the cache with the specified sectors
					if (isofs_ReadSect(CachedDirInfo.sector_start+CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache) != 1) {
						#ifdef DEBUG
							printf("Couldn't Read from CD !\n");
						#endif

						CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read time?
						return 0;
					}
				}

				// start the search, with the path after the current directory
				path_len = strlen(CachedDirInfo.pathname);
				strcpy(dirname, pathname + path_len);

				// FindPath should use the current directory cache to start it's search
				// and should change CachedDirInfo.pathname, to the path of the dir it finds
				// it should also cache the first chunk of directory sectors,
				// and fill the contents of the other elements of CachedDirInfo appropriately

				return (FindPath(dirname));
			}
		}
	}

	// If we've got here, then either the cache was not valid to start with
	// or the requested path is not a subdirectory of the currently cached directory
	// so lets start again
	#ifdef DEBUG
		printf("The cache is not valid, or the requested directory is not a sub-dir of the cached one\n");
	#endif

	// Read the main volume descriptor
	if (isofs_GetVolumeDescriptor() != 1)
	{
		#ifdef DEBUG
			printf("Could not read the CD/DVD Volume Descriptor err %d\n", isofs_GetVolumeDescriptor());
		#endif

		return -1;
	}

	#ifdef DEBUG
		printf("Read the CD root TOC - LBA = %d size = %d\n", \
			(gIsBigEnd ? CDVolDesc.rootToc.tocLBA_bigend : CDVolDesc.rootToc.tocLBA), \
			(gIsBigEnd ? CDVolDesc.rootToc.tocSize_bigend : CDVolDesc.rootToc.tocSize));
	#endif

	CachedDirInfo.path_depth = 0;

	strcpy(CachedDirInfo.pathname, "");
	
	// Setup the lba and sector size, for retrieving the root toc
	CachedDirInfo.cache_offset = 0;
	CachedDirInfo.sector_start = (gIsBigEnd ? CDVolDesc.rootToc.tocLBA_bigend : CDVolDesc.rootToc.tocLBA);
	CachedDirInfo.sector_num = (gIsBigEnd ? CDVolDesc.rootToc.tocSize_bigend : CDVolDesc.rootToc.tocSize) / 2048;
	if ((gIsBigEnd ? CDVolDesc.rootToc.tocSize_bigend : CDVolDesc.rootToc.tocSize) & 0x7ff)
		CachedDirInfo.sector_num++;

	CachedDirInfo.cache_size = CachedDirInfo.sector_num;

	if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
		CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

	// Now fill the cache with the specified sectors
	if (isofs_ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache) != 1) {
		#ifdef DEBUG
			printf("Couldn't Read from CD !\n");
		#endif

		CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read time?
		return 0;
	}

	#ifdef DEBUG
		printf("Read the first block from the root directory\n");
	#endif

	// FindPath should use the current directory cache to start it's search (in this case the root)
	// and should change CachedDirInfo.pathname, to the path of the dir it finds
	// it should also cache the first chunk of directory sectors,
	// and fill the contents of the other elements of CachedDirInfo appropriately
	#ifdef DEBUG
		printf("Calling FindPath pathname %s\n", pathname);
	#endif
	strcpy(dirname, pathname);

	return (FindPath(dirname));
}

//-------------------------------------------------------------- 
void __splitpath(const char *constpath, char *dir, char *fname)
{
	// 255 char max path-length is an ISO9660 restriction
	// we must change this for Joliet or relaxed iso restriction support
	static char pathcopy[1024+1];
	char* slash;

	strncpy(pathcopy, constpath, 1024);

    slash = strrchr (pathcopy, '/');

	// if the path doesn't contain a '/' then look for a '\'
    if (!slash)
		slash = strrchr (pathcopy, (int)'\\');

	// if a slash was found
	if (slash != NULL) {
		// null terminate the path
		slash[0] = 0;
		// and copy the path into 'dir'
		strncpy(dir, pathcopy, 1024);
		dir[255]=0;

		// copy the filename into 'fname'
		strncpy(fname, slash+1, 128);
		fname[128]=0;
	}
	else {
		dir[0] = 0;

		strncpy(fname, pathcopy, 128);
		fname[128]=0;
	}

	strtok(fname, ";");
}

//-------------------------------------------------------------- 
int isofs_FindFile(const char *fname, struct TocEntry *tocEntry) // Export #6
{
	static char filename[128+1];
	static char pathname[1024+1];
	struct dirTocEntry *tocEntryPointer;

	#ifdef DEBUG
		printf("isofs_FindFile called\n");
	#endif

	__splitpath(fname, pathname, filename);

	#ifdef DEBUG
		printf("Trying to find file: %s in directory: %s\n",filename, pathname);
	#endif

	if ((CachedDirInfo.valid == 1) && (ComparePath(pathname) == MATCH)) {
		// the directory is already cached, so check through the currently
		// cached chunk of the directory first

		tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;

		for ( ; (u8 *)tocEntryPointer < (CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048)); \
		tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + (gIsBigEnd ? be16(tocEntryPointer->length) : tocEntryPointer->length))) \
		{
			if ((gIsBigEnd ? be16(tocEntryPointer->length) : tocEntryPointer->length) == 0) {
				#ifdef DEBUG
					printf("Got a null pointer entry, so either reached end of dir, or end of sector\n");
				#endif

				tocEntryPointer = (struct dirTocEntry *)(CachedDirInfo.cache + (((((u8 *)tocEntryPointer - CachedDirInfo.cache) / 2048) + 1) * 2048));
			}

			if ((u8 *)tocEntryPointer >= (CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048))) {
				// reached the end of the cache block
				break;
			}

			//if ((tocEntryPointer->fileProperties & 0x02) == 0) {
				// It's a file
				TocEntryCopy(tocEntry, tocEntryPointer);

				if (strcasecmp(tocEntry->filename, filename) == 0) {
					// and it matches !!
					return 1;
				}
			//}
		} // end of for loop

		// If that was the only dir block, and we havent found it, then fail
		if (CachedDirInfo.cache_size == CachedDirInfo.sector_num)
			return 0;

		// Otherwise there is more dir to check
		if (CachedDirInfo.cache_offset == 0) {
			// If that was the first block then continue with the next block
			if (isofs_Cache_Dir(pathname, CACHE_NEXT) != 1)
				return 0;
		}
		else {
			// otherwise (if that wasnt the first block) then start checking from the start 
			if (isofs_Cache_Dir(pathname, CACHE_START) != 1)
				return 0;
		}
	}
	else {
		#ifdef DEBUG
			printf("Trying to cache directory\n");
		#endif
		// The wanted directory wasnt already cached, so cache it now
		if (isofs_Cache_Dir(pathname, CACHE_START) != 1) {
			#ifdef DEBUG
				printf("Failed to cache directory\n");
			#endif

			return 0;
		}
	}

	// If we've got here, then we have a block of the directory cached, and want to check
	// from this point, to the end of the dir
	#ifdef DEBUG
		printf("cache_size = %d\n", CachedDirInfo.cache_size);
	#endif

	while (CachedDirInfo.cache_size > 0) {
		tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;

		if (CachedDirInfo.cache_offset == 0)
			tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + (gIsBigEnd ? be16(tocEntryPointer->length) : tocEntryPointer->length));

		for ( ; (u8 *)tocEntryPointer < (CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048)); \
		tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + (gIsBigEnd ? be16(tocEntryPointer->length) : tocEntryPointer->length))) \
		{

			if ((gIsBigEnd ? be16(tocEntryPointer->length) : tocEntryPointer->length) == 0) {
				#ifdef DEBUG
					printf("Got a null pointer entry, so either reached end of dir, or end of sector\n");
					printf("Offset into cache = %d bytes\n", (int)tocEntryPointer - (int)CachedDirInfo.cache);
				#endif

				tocEntryPointer = (struct dirTocEntry *)(CachedDirInfo.cache + (((((u8 *)tocEntryPointer - CachedDirInfo.cache) / 2048) + 1) * 2048));
			}

			if ((u8 *)tocEntryPointer >= (CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048))) {
				// reached the end of the cache block
				break;
			}

			TocEntryCopy(tocEntry, tocEntryPointer);

			if (strcasecmp(tocEntry->filename, filename) == 0) {
				#ifdef DEBUG
					printf("Found a matching file\n");
				#endif
				// and it matches !!
				return 1;
			}

			#ifdef DEBUG
				printf("Non-matching file - looking for %s , found %s\n",filename, tocEntry->filename);
			#endif
		} // end of for loop

		#ifdef DEBUG
			printf("Reached end of cache block\n");
		#endif
		// cache the next block
		isofs_Cache_Dir(pathname, CACHE_NEXT);
	}

	// we've run out of dir blocks to cache, and still not found it, so fail
	#ifdef DEBUG
		printf("isofs_FindFile: could not find file\n");
	#endif

	return 0;
}

//-------------------------------------------------------------- 
int isofs_Open(const char *filename)
{
	register int fd;
	FHANDLE *fh;
	static struct TocEntry tocEntry;

	#ifdef DEBUG
		printf("isofs_Open: filename %s\n", filename);
	#endif

	if (!isofs_inited)
		return -13;

	// attribute new file handle	
	for (fd = 0; fd < MAX_FDHANDLES; fd++) {
		fh = (FHANDLE *)&isofs_fdhandles[fd];
		if (fh->status == 0)
			break;
	}

	// check if the file exists
	if (isofs_FindFile(filename, &tocEntry) != 1)
		return -1;

	// too much files opened
	if (fd == MAX_FDHANDLES)
		return -7;

	memset((void *)fh, 0, sizeof (FHANDLE));

	fh->lsn = tocEntry.fileLBA;
	fh->filesize = tocEntry.fileSize;
	fh->status = 1;

	return fd;
}

//-------------------------------------------------------------- 
int isofs_Close(int fd)
{
	FHANDLE *fh;

	#ifdef DEBUG
		printf("isofs_Close: fd = %d\n", fd);
	#endif

	if (!isofs_inited)
		return -13;

	if (!((u32)fd < MAX_FDHANDLES))
		return -5;

	fh = (FHANDLE *)&isofs_fdhandles[fd];
	if (!fh->status)
		return -5;

	fh->status = 0;

	return 0;
}

//-------------------------------------------------------------- 
int isofs_Read(int fd, void *buf, u32 nbytes)
{
	register int r;
	FHANDLE *fh;
	s64 offset;

	#ifdef DEBUG
		printf("isofs_Read: fd = %d nbytes = %d\n", fd, nbytes);
	#endif

	if (!isofs_inited)
		return -13;

	if (!((u32)fd < MAX_FDHANDLES))
		return -5;

	fh = (FHANDLE *)&isofs_fdhandles[fd];
	if (!fh->status) 
		return -5;

	if (fh->position >= fh->filesize)
		return 0;

	if (nbytes >= (fh->filesize - fh->position))
		nbytes = fh->filesize - fh->position;

	offset = (((s64)fh->lsn) * 2048)+ fh->position;

	#ifdef DEBUG
		printf("isofs_Read: offset =%lld nbytes = %d\n", offset, nbytes);
	#endif

	r = isofs_ReadISO(offset, nbytes, buf);
	fh->position += r;

	#ifdef DEBUG
		printf("isofs_Read: readed bytes = %d\n", r);
	#endif

    return r;
}

//-------------------------------------------------------------- 
int isofs_Seek(int fd, u32 offset, int origin)
{
	register int r;
	FHANDLE *fh;
	
	#ifdef DEBUG
		printf("isofs_Seek: fd = %d offset = %d origin = %d\n", fd, offset, origin);
	#endif

	if (!isofs_inited)
		return -13;
		
	if (!((u32)fd < MAX_FDHANDLES))
		return -5;
			
	fh = (FHANDLE *)&isofs_fdhandles[fd];	
	if (!fh->status) 
		return -5;
	
	switch (origin) {
		case SEEK_CUR:
			r = fh->position + offset;
			break;
		case SEEK_SET:	
			r = offset;
			break;			
		case SEEK_END:	
			r = fh->filesize + offset;		
			break;	
		default:
			return -1;		
	}
	
	if (r > fh->filesize)
		r = fh->filesize;

	return fh->position = (r < 0) ? 0 : r;
}

//-------------------------------------------------------------------------
s64 isofs_Init(const char *iso_path, int isBigEndian)
{
	s64 r;

	#ifdef DEBUG
		printf("isofs_Init: ISO path %s\n", iso_path);
	#endif

	if (!isofs_inited) {
		gIsBigEnd = isBigEndian;
		CachedDirInfo.cache = (u8 *)isofs_dircache;			
		isofs_FlushCache();	
	
		//g_fh_iso = open(iso_path, O_RDONLY);
		//if (g_fh_iso < 0)
		//	return 0;
		g_fh_iso = fopen(iso_path, "rb");
		if (!g_fh_iso)
			return 0;		
	}
	
	//r = lseek64(g_fh_iso, 0, SEEK_END);		
	//lseek64(g_fh_iso, 0, SEEK_SET);
	fseeko64(g_fh_iso, 0, SEEK_END);
	r = ftello64(g_fh_iso);		
	fseeko64(g_fh_iso, 0, SEEK_SET);
	
	isofs_inited = 1;
	
	return r;	
}

//-------------------------------------------------------------------------
int isofs_Reset(void)
{
	#ifdef DEBUG
		printf("isofs_Reset\n");
	#endif

	if (isofs_inited) {
		
		isofs_FlushCache();	
		//close(g_fh_iso);
		fclose(g_fh_iso);
	}
	
	//g_fh_iso = -1;
	g_fh_iso = NULL;
	isofs_inited = 0;
	
	return 1;	
}
