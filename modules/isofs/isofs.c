/*
  Copyright 2009, jimmikaelkael
  Copyright (c) 2002, A.Lee & Nicholas Van Veen  
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from libcdvd by A.Lee & Nicholas Van Veen
  Review license_libcdvd file for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <ioman.h>
#include <sifrpc.h>
#include <sysclib.h>
#include <thsemap.h>
#include <io_common.h>

//#define NETLOG_DEBUG

#define MODNAME "isofs"
IRX_ID(MODNAME, 1, 0);

static char g_ISO_name[]="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
static char g_ISO_parts=0x69;
static char g_ISO_media=0x69;
static char g_ISO_mode=0x69;
static char skipmod_tab[256] = "\0";

#define USB_MODE 	0
#define ETH_MODE 	1

extern void *dummy_irx;
extern int size_dummy_irx;

struct irx_export_table _exp_isofs;

static int g_isofs_io_sema;

#define ISO_MAX_PARTS	8
static int g_iso_fd[ISO_MAX_PARTS];

// fs driver ops functions prototypes
int fs_dummy(void);
int fs_init(iop_device_t *iop_dev);
int fs_deinit(iop_device_t *dev);
int fs_open(iop_file_t *f, char *filename, int mode, int flags);
int fs_close(iop_file_t *f);
int fs_lseek(iop_file_t *f, u32 pos, int where);
int fs_read(iop_file_t *f, void *buf, u32 size);
int fs_getstat(iop_file_t *f, char *filename, fio_stat_t *stat);
int fs_dopen(iop_file_t *f, char *dirname);
int fs_dclose(iop_file_t *f);
int fs_dread(iop_file_t *f, fio_dirent_t *dirent);

// driver ops func tab
static void *isofs_ops[17] = {
	(void*)fs_init,
	(void*)fs_deinit,
	(void*)fs_dummy,
	(void*)fs_open,
	(void*)fs_close,
	(void*)fs_read,
	(void*)fs_dummy,
	(void*)fs_lseek,
	(void*)fs_dummy,
	(void*)fs_dummy,
	(void*)fs_dummy,
	(void*)fs_dummy,
	(void*)fs_dopen,
	(void*)fs_dclose,
	(void*)fs_dread,
	(void*)fs_getstat,
	(void*)fs_dummy
};

// driver descriptor
static iop_device_t isofs_dev = {
	"iso", 
	IOP_DT_FS,
	1,
	"ISO-FS ",
	(struct _iop_device_ops *)&isofs_ops
};

typedef struct {
	u8  status;
	u8  pad[3]; 
	u32 lsn;  	
	u32 position;
	u32 filesize;
} FHANDLE;

#define MAX_FDHANDLES 		32
FHANDLE isofs_fdhandles[MAX_FDHANDLES] __attribute__((aligned(64)));

#define MAX_DIR_CACHE_SECTORS 32

struct dir_cache_info {
	char pathname[1024];// The pathname of the cached directory
	u32  valid;			// 1 if cache data is valid, 0 if not
	u32  path_depth;	// The path depth of the cached directory (0 = root)
	u32  sector_start;	// The start sector (LBA) of the cached directory
	u32  sector_num;	// The total size of the directory (in sectors)
	u32  cache_offset;	// The offset from sector_start of the cached area
	u32  cache_size;	// The size of the cached directory area (in sectors)
	u8  *cache;			// The actual cached data
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
	u32 tocLBA;
	u32 tocLBA_bigend;
	u32 tocSize;
	u32 tocSize_bigend;
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
	u8		volID[5];		// "CD001"
	u8		reserved2;
	u8		reserved3;
	u8		sysIdName[32];
	u8		volName[32];	// The ISO9660 Volume Name
	u8		reserved5[8];
	u32		volSize;		// Volume Size
	u32		volSizeBig;		// Volume Size Big-Endian
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
	char filename[128];
} __attribute__((packed));

struct TocEntry {
	u32		fileLBA;
	u32		fileSize;
	u8	    dateStamp[6];
	u8 		padding0[2];
	u8		fileProperties;
	u8		padding1[3];
	char	filename[128+1];
	u8		padding2[3];
} __attribute__((packed));

static struct cdVolDesc CDVolDesc;

static char isofs_dopen_dirname[1024+1];

static char skipmod_name[1024];
static char skipmod_modlist[256];	
	
#ifdef NETLOG_DEBUG
// !!! netlog exports functions pointers !!!
int (*netlog_send)(const char *format, ...);
#endif

void delay(int count)
{
	int i, ret;

	for (i  = 0; i < count; i++) {
		ret = 0x01000000;
		while(ret--) asm("nop\nnop\nnop\nnop");
	}
}

//-------------------------------------------------------------------------
iop_device_t *isofs_GetDevice(void) // Export #4
{
	#ifdef NETLOG_DEBUG
		netlog_send("isofs_GetDevice called\n");
	#endif

	return &isofs_dev;
}

//-------------------------------------------------------------------------
int isofs_Read0(u32 offset, u32 nbytes, void *buf, int part)
{
	register int r;
	register int bytes_to_read;
	u8 *p = (u8 *)buf;
	
	#ifdef NETLOG_DEBUG
		netlog_send("isofs_Read0: offset = %d nbytes = %d part = %d\n", offset, nbytes, part);
	#endif	

	r = 0;
	bytes_to_read = nbytes;
		
	if(offset<0x40000000){
		lseek(g_iso_fd[part+0], offset, SEEK_SET);
		if ((offset + nbytes) > 0x3fffffff) {
			bytes_to_read = 0x40000000 - offset;
			nbytes -= bytes_to_read; 
			offset = 0x40000000;
		}	
		r += read(g_iso_fd[part+0], &p[r], bytes_to_read);
		bytes_to_read = nbytes;	
	}
		
	if(offset>=0x40000000 && offset<0x80000000){
		lseek(g_iso_fd[part+1], offset-0x40000000, SEEK_SET);
		if ((offset + nbytes) > 0x7fffffff) {
			bytes_to_read = 0x80000000 - offset;
			nbytes -= bytes_to_read; 
			offset = 0x80000000;
		}			
		r += read(g_iso_fd[part+1], &p[r], bytes_to_read);	
		bytes_to_read = nbytes;
	}
	
	if(offset>=0x80000000 && offset<0xc0000000){
		lseek(g_iso_fd[part+2], offset-0x80000000, SEEK_SET);
		if ((offset + nbytes) > 0xbfffffff) {
			bytes_to_read = 0xc0000000 - offset;
			nbytes -= bytes_to_read; 
			offset = 0xc0000000;
		}					
		r += read(g_iso_fd[part+2], &p[r], bytes_to_read);	
		bytes_to_read = nbytes;
	}
		
	if(offset>=0xC0000000 && offset<0x100000000){
		lseek(g_iso_fd[part+3], offset-0xC0000000, SEEK_SET);						
		r += read(g_iso_fd[part+3], &p[r], bytes_to_read);	
	}
	
	return r;
}

//-------------------------------------------------------------------------
int isofs_ReadSect(u32 lsn, u32 nsectors, void *buf) // Export #5
{
	register int r;
	register u32 offset, nbytes_lo, nbytes_hi, u32limit;

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_ReadSect: LBA = %d nsectors = %d\n", lsn, nsectors);
	#endif	

	if (lsn < 0x200000) { 			// offset in bytes is inside u32 range
		offset = lsn << 11;
		nbytes_lo = nsectors << 11;
		nbytes_hi = 0;
		u32limit = 0xffffffff - offset;
		if (nbytes_lo > u32limit) {	// we'll have bytes outside u32 range to read
			nbytes_hi = nbytes_lo - u32limit; 
			nbytes_lo = u32limit;
		}
		r = isofs_Read0(offset, nbytes_lo, buf, 0);
		if (r != nbytes_lo)
			return 0;
		if (nbytes_hi) {
			r = isofs_Read0(0, nbytes_hi, (void *)(buf+nbytes_lo), 4);
			if (r != nbytes_hi)
				return 0;			
		}
	}
	else { 							// offset in bytes is outside u32 range
		offset = (lsn-0x200000) << 11;
		nbytes_hi = nsectors << 11;
		r = isofs_Read0(offset, nbytes_hi, buf, 4);
		if (r != nbytes_hi)
			return 0;
	}

	return 1;
}

//-------------------------------------------------------------------------
int isofs_ReadISO(u32 lsn, u32 pos, u32 nbytes, void *buf)
{
	register int r;
	register u32 offset, nbytes_lo, nbytes_hi, u32limit;

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_ReadISO: LBA = %d pos = %d nbytes = %d\n", lsn, pos, nbytes);
	#endif	

	if (lsn < 0x200000) { 			// offset in bytes is inside u32 range
		offset = (lsn << 11) + pos;
		nbytes_lo = nbytes;
		nbytes_hi = 0;
		u32limit = 0xffffffff - offset;
		if (nbytes_lo > u32limit) {	// we'll have bytes outside u32 range to read
			nbytes_hi = nbytes_lo - u32limit; 
			nbytes_lo = u32limit;
		}
		r = isofs_Read0(offset, nbytes_lo, buf, 0);
		if (r != nbytes_lo)
			return 0;
		if (nbytes_hi) {
			r = isofs_Read0(0, nbytes_hi, (void *)(buf+nbytes_lo), 4);
			if (r != nbytes_hi)
				return r;			
		}
	}
	else { 							// offset in bytes is outside u32 range
		offset = ((lsn-0x200000) << 11) + pos;
		nbytes_hi = nbytes;
		r = isofs_Read0(offset, nbytes_hi, buf, 4);
		if (r != nbytes_hi)
			return r;
	}

	return nbytes;
}

//-------------------------------------------------------------------------
int isofs_FlushCache(void)
{
	strcpy(CachedDirInfo.pathname, "");		// The pathname of the cached directory
	CachedDirInfo.valid = 0;				// Cache is not valid
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

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_GetVolumeDescriptor called\n");
	#endif

	for (volDescSector = 16; volDescSector < 20; volDescSector++) {
		isofs_ReadSect(volDescSector, 1, &localVolDesc);

		// If this is still a volume Descriptor
		if (strncmp(localVolDesc.volID, "CD001", 5) == 0) {
			if ((localVolDesc.filesystemType == 1) ||
				(localVolDesc.filesystemType == 2))	{
				memcpy(&CDVolDesc, &localVolDesc, sizeof(struct cdVolDesc));
			}
		}
		else
			break;
	}

	#ifdef NETLOG_DEBUG	
		switch (CDVolDesc.filesystemType) {
			case 1:
				netlog_send("CD FileSystem is ISO9660\n");
				break;
			case 2:
				netlog_send("CD FileSystem is Joliet\n");
				break;
			default:
				netlog_send("CD FileSystem is unknown type\n");
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

	tocEntry->fileSize = internalTocEntry->fileSize;
	tocEntry->fileLBA = internalTocEntry->fileLBA;
	tocEntry->fileProperties = internalTocEntry->fileProperties;

	if (CDVolDesc.filesystemType == 2) {
		// This is a Joliet Filesystem, so use Unicode to ISO string copy
		filenamelen = internalTocEntry->filenameLength >> 1;

		for (i=0; i < filenamelen; i++)
			tocEntry->filename[i] = internalTocEntry->filename[(i << 1) + 1];
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

		(u8 *)tocEntryPointer = CachedDirInfo.cache;
		
		// Always skip the first entry (self-refencing entry)
		(u8 *)tocEntryPointer += tocEntryPointer->length;

		dir_entry = 0;

		for ( ; (u8 *)tocEntryPointer < (CachedDirInfo.cache + (CachedDirInfo.cache_size << 11)); (u8 *)tocEntryPointer += tocEntryPointer->length) {
			// If we have a null toc entry, then we've either reached the end of the dir, or have reached a sector boundary
			if (tocEntryPointer->length == 0) {
				#ifdef NETLOG_DEBUG
					netlog_send("Got a null pointer entry, so either reached end of dir, or end of sector\n");
				#endif

				(u8 *)tocEntryPointer = CachedDirInfo.cache + (((((u8 *)tocEntryPointer - CachedDirInfo.cache) >> 11) + 1) << 11);
			}

			if ((u8 *)tocEntryPointer >= (CachedDirInfo.cache + (CachedDirInfo.cache_size << 11))) {
				// If we've gone past the end of the cache
				// then check if there are more sectors to load into the cache

				if ((CachedDirInfo.cache_offset + CachedDirInfo.cache_size) < CachedDirInfo.sector_num) {
					// If there are more sectors to load, then load them
					CachedDirInfo.cache_offset += CachedDirInfo.cache_size;
					CachedDirInfo.cache_size = CachedDirInfo.sector_num - CachedDirInfo.cache_offset;

					if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
						CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

					if (isofs_ReadSect(CachedDirInfo.sector_start+CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache) != 1) {
						#ifdef NETLOG_DEBUG
							netlog_send("Couldn't Read from CD !\n");
						#endif

						CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read time?
						return 0;
					}

					(u8 *)tocEntryPointer = CachedDirInfo.cache;
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
						#ifdef NETLOG_DEBUG
							netlog_send("First directory entry in dir, so name it '..'\n");
						#endif

						strcpy(localTocEntry.filename, "..");
					}
				}

				// Check if this is the directory that we are looking for
				if (strcasecmp(dirname, localTocEntry.filename) == 0) {
					#ifdef NETLOG_DEBUG
						netlog_send("Found the matching sub-directory\n");
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

						#ifdef NETLOG_DEBUG
							netlog_send("Adding '%s' to cached pathname - path depth = %d\n",dirname,CachedDirInfo.path_depth);
						#endif

						strcat(CachedDirInfo.pathname, dirname);

						CachedDirInfo.path_depth++;
					}

					// Exit out of the search loop
					// (and find the next sub-directory, if there is one)
					break;
				}
				else {
					#ifdef NETLOG_DEBUG
						netlog_send("Found a directory, but it doesn't match\n");
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
		CachedDirInfo.sector_num = localTocEntry.fileSize >> 11;
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
			#ifdef NETLOG_DEBUG
				netlog_send("Couldn't Read from CD, trying to read %d sectors, starting at sector %d !\n",
					CachedDirInfo.cache_size, CachedDirInfo.sector_start+CachedDirInfo.cache_offset);
			#endif

			CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read time?
			return 0;
		}
	}

	// If we've got here then we found the requested directory
	#ifdef NETLOG_DEBUG
		netlog_send("FindPath found the path\n");
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

	#ifdef NETLOG_DEBUG
		netlog_send("Attempting to find, and cache, directory: %s\n", pathname);
	#endif

	// only take any notice of the existing cache, if it's valid
	if (CachedDirInfo.valid == 1) {
	// Check if the requested path is already cached
		if (ComparePath(pathname) == MATCH) {
			#ifdef NETLOG_DEBUG
				netlog_send("CacheDir: The requested path is already cached\n");
			#endif

			// If so, is the request ot cache the start of the directory, or to resume the next block ?
			if (getMode == CACHE_START) {
				#ifdef NETLOG_DEBUG
					netlog_send("          and requested cache from start of dir\n");
				#endif

				if (CachedDirInfo.cache_offset == 0) {
					// requested cache of start of the directory, and thats what's already cached
					// so sit back and do nothing
					#ifdef NETLOG_DEBUG
						netlog_send("          and start of dir is already cached so nothing to do :o)\n");
					#endif
					
					CachedDirInfo.valid = 1;
					return 1;
				}
				else {
					// Requested cache of start of the directory, but thats not what's cached
					// so re-cache the start of the directory

					#ifdef NETLOG_DEBUG
						netlog_send("          but dir isn't cached from start, so re-cache existing dir from start\n");
					#endif

					// reset cache data to start of existing directory
					CachedDirInfo.cache_offset = 0;
					CachedDirInfo.cache_size = CachedDirInfo.sector_num;

					if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
						CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

					// Now fill the cache with the specified sectors
					if (isofs_ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache) != 1) {
						#ifdef NETLOG_DEBUG
							netlog_send("Couldn't Read from CD !\n");
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
					#ifdef NETLOG_DEBUG
						netlog_send("Couldn't Read from CD !\n");
					#endif

					CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read first time?
					return 0;
				}

				CachedDirInfo.valid = 1;
				return 1;
			}
		} 
		else { // requested directory is not the cached directory (but cache is still valid)
			#ifdef NETLOG_DEBUG
				netlog_send("Cache is valid, but cached directory, is not the requested one\n"
					"so check if the requested directory is a sub-dir of the cached one\n");

				netlog_send("Requested Path = %s , Cached Path = %s\n",pathname, CachedDirInfo.pathname);
			#endif
			
			// Is the requested pathname a sub-directory of the current-directory ?

			// if the requested pathname is longer than the pathname of the cached dir
			// and the pathname of the cached dir matches the beginning of the requested pathname
			// and the next character in the requested pathname is a dir seperator

			if (ComparePath(pathname) == SUBDIR) {
				// If so then we can start our search for the path, from the currently cached directory
				#ifdef NETLOG_DEBUG
					netlog_send("Requested dir is a sub-dir of the cached directory,\n"
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
						#ifdef NETLOG_DEBUG
							netlog_send("Couldn't Read from CD !\n");
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
	#ifdef NETLOG_DEBUG
		netlog_send("The cache is not valid, or the requested directory is not a sub-dir of the cached one\n");
	#endif

	// Read the main volume descriptor
	if (isofs_GetVolumeDescriptor() != 1)
	{
		#ifdef NETLOG_DEBUG
			netlog_send("Could not read the CD/DVD Volume Descriptor err %d\n", isofs_GetVolumeDescriptor());
		#endif

		return -1;
	}

	#ifdef NETLOG_DEBUG
		netlog_send("Read the CD root TOC - LBA = %d size = %d\n", CDVolDesc.rootToc.tocLBA, CDVolDesc.rootToc.tocSize);
	#endif

	CachedDirInfo.path_depth = 0;

	strcpy(CachedDirInfo.pathname, "");
	
	// Setup the lba and sector size, for retrieving the root toc
	CachedDirInfo.cache_offset = 0;
	CachedDirInfo.sector_start = CDVolDesc.rootToc.tocLBA;
	CachedDirInfo.sector_num = CDVolDesc.rootToc.tocSize >> 11;
	if (CDVolDesc.rootToc.tocSize & 0x7ff)
		CachedDirInfo.sector_num++;
			
    CachedDirInfo.cache_size = CachedDirInfo.sector_num;

	if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
		CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

	// Now fill the cache with the specified sectors
	if (isofs_ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache) != 1) {
		#ifdef NETLOG_DEBUG
			netlog_send("Couldn't Read from CD !\n");
		#endif

		CachedDirInfo.valid = 0;	// should we completely invalidate just because we couldnt read time?
		return 0;
	}

	#ifdef NETLOG_DEBUG
		netlog_send("Read the first block from the root directory\n");
	#endif
	
	// FindPath should use the current directory cache to start it's search (in this case the root)
	// and should change CachedDirInfo.pathname, to the path of the dir it finds
	// it should also cache the first chunk of directory sectors,
	// and fill the contents of the other elements of CachedDirInfo appropriately
	#ifdef NETLOG_DEBUG
		netlog_send("Calling FindPath pathname %s\n", pathname);
	#endif
	strcpy(dirname, pathname);

	return (FindPath(dirname));
}

//-------------------------------------------------------------- 
void _splitpath(const char *constpath, char *dir, char *fname)
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

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_FindFile called\n");
	#endif
	
	_splitpath(fname, pathname, filename);

	#ifdef NETLOG_DEBUG
		netlog_send("Trying to find file: %s in directory: %s\n",filename, pathname);
	#endif

	if ((CachedDirInfo.valid == 1) && (ComparePath(pathname) == MATCH)) {
		// the directory is already cached, so check through the currently
		// cached chunk of the directory first

		(u8 *)tocEntryPointer = CachedDirInfo.cache;

		for ( ; (u8 *)tocEntryPointer < (CachedDirInfo.cache + (CachedDirInfo.cache_size << 11)); (u8 *)tocEntryPointer += tocEntryPointer->length) {
			if (tocEntryPointer->length == 0) {
				#ifdef NETLOG_DEBUG
					netlog_send("Got a null pointer entry, so either reached end of dir, or end of sector\n");
				#endif

				(u8 *)tocEntryPointer = CachedDirInfo.cache + (((((u8 *)tocEntryPointer - CachedDirInfo.cache) >> 11) + 1) << 11);
			}

			if ((u8 *)tocEntryPointer >= (CachedDirInfo.cache + (CachedDirInfo.cache_size << 11))) {
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
		#ifdef NETLOG_DEBUG
			netlog_send("Trying to cache directory\n");
		#endif
		// The wanted directory wasnt already cached, so cache it now
		if (isofs_Cache_Dir(pathname, CACHE_START) != 1) {
			#ifdef NETLOG_DEBUG
				netlog_send("Failed to cache directory\n");
			#endif

			return 0;
		}
	}

	// If we've got here, then we have a block of the directory cached, and want to check
	// from this point, to the end of the dir
	#ifdef NETLOG_DEBUG
		netlog_send("cache_size = %d\n", CachedDirInfo.cache_size);
	#endif

	while (CachedDirInfo.cache_size > 0) {
		(u8 *)tocEntryPointer = CachedDirInfo.cache;
		
		if (CachedDirInfo.cache_offset == 0)
			(u8 *)tocEntryPointer += tocEntryPointer->length;

		for ( ; (u8 *)tocEntryPointer < (CachedDirInfo.cache + (CachedDirInfo.cache_size << 11)); (u8 *)tocEntryPointer += tocEntryPointer->length) {
			
			if (tocEntryPointer->length == 0) {
				#ifdef NETLOG_DEBUG
					netlog_send("Got a null pointer entry, so either reached end of dir, or end of sector\n");
					netlog_send("Offset into cache = %d bytes\n", (int)tocEntryPointer - (int)CachedDirInfo.cache);
				#endif

				(u8 *)tocEntryPointer = CachedDirInfo.cache + (((((u8 *)tocEntryPointer - CachedDirInfo.cache) >> 11) + 1) << 11);
			}

			if ((u8 *)tocEntryPointer >= (CachedDirInfo.cache + (CachedDirInfo.cache_size << 11))) {
				// reached the end of the cache block
				break;
			}

			TocEntryCopy(tocEntry, tocEntryPointer);
			
			if (strcasecmp(tocEntry->filename, filename) == 0) {
				#ifdef NETLOG_DEBUG
					netlog_send("Found a matching file\n");
				#endif
				// and it matches !!
				return 1;
			}

			#ifdef NETLOG_DEBUG
				netlog_send("Non-matching file - looking for %s , found %s\n",filename, tocEntry->filename);
			#endif
		} // end of for loop
		
		#ifdef NETLOG_DEBUG
			netlog_send("Reached end of cache block\n");
		#endif
		// cache the next block
		isofs_Cache_Dir(pathname, CACHE_NEXT);
	}
			
	// we've run out of dir blocks to cache, and still not found it, so fail
	#ifdef NETLOG_DEBUG
		netlog_send("isofs_FindFile: could not find file\n");
	#endif
	
	return 0;
}

//-------------------------------------------------------------- 
int isofs_GetDir(const char *pathname, struct TocEntry *tocEntry, int index)
{
	int matched_entries;
	int dir_entry;
	struct TocEntry localTocEntry;
	struct dirTocEntry *tocEntryPointer;

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_GetDir call\n");
	#endif

	matched_entries = 0;

	// pre-cache the dir (and get the new pathname - in-case selected "..")
	if (isofs_Cache_Dir(pathname, CACHE_START) != 1) {
		#ifdef NETLOG_DEBUG
			netlog_send("isofs_GetDir - Call of isofs_Cache_Dir failed\n");
		#endif

		return -1;
	}

	#ifdef NETLOG_DEBUG
		netlog_send("requested directory is %d sectors\n", CachedDirInfo.sector_num);
	#endif

	// Cache the start of the requested directory
	if (isofs_Cache_Dir(CachedDirInfo.pathname, CACHE_START) != 1) {
		#ifdef NETLOG_DEBUG
			netlog_send("isofs_GetDir - Call of isofs_Cache_Dir failed\n");
		#endif

		return -1;
	}

	(u8 *) tocEntryPointer = CachedDirInfo.cache;

	// skip the first self-referencing entry
	(u8 *)tocEntryPointer += tocEntryPointer->length;

	// skip the parent entry if this is the root
	if (CachedDirInfo.path_depth == 0)
		(u8 *)tocEntryPointer += tocEntryPointer->length;

	dir_entry = 0;

	while(1) {
		#ifdef NETLOG_DEBUG
			netlog_send("isofs_GetDir - inside while-loop\n");
		#endif

		// parse the current cache block
		for( ; (u8 *)tocEntryPointer < (CachedDirInfo.cache + (CachedDirInfo.cache_size << 11)); (u8 *)tocEntryPointer += tocEntryPointer->length) {
			if (tocEntryPointer->length == 0) {
				// if we have a toc entry length of zero,
				// then we've either reached the end of the sector, or the end of the dir
				// so point to next sector (if there is one - will be checked by next condition)

				(u8 *)tocEntryPointer = CachedDirInfo.cache + (((((u8 *)tocEntryPointer - CachedDirInfo.cache) >> 11) + 1) << 11);
			}

			if ((u8 *)tocEntryPointer >= CachedDirInfo.cache + (CachedDirInfo.cache_size << 11)) {
				// we've reached the end of the current cache block (which may be end of entire dir
				// so just break the loop
				break;
			}

			// Check if the current entry is a dir or a file
			if (tocEntryPointer->fileProperties & 0x02)	{
				#ifdef NETLOG_DEBUG
					netlog_send("We found a dir: %s\n", tocEntryPointer->filename);
				#endif

				TocEntryCopy(&localTocEntry, tocEntryPointer);

				if (dir_entry == 0) {
					if (CachedDirInfo.path_depth != 0) {
						#ifdef NETLOG_DEBUG
							netlog_send("It's the first directory entry, so name it '..'\n");
						#endif

						strcpy(localTocEntry.filename, "..");
					}
				}
				
				if ((tocEntry) && (matched_entries == index)) {
					TocEntryCopy(tocEntry, tocEntryPointer);
					strcpy(tocEntry->filename, localTocEntry.filename);
					tocEntry->fileSize = 0;
					return 1;
				}
								
				matched_entries++;
			}
			dir_entry++;
		} // end of the current cache block

		// if there is more dir to load, then load next chunk, else finish
		if ((CachedDirInfo.cache_offset+CachedDirInfo.cache_size) < CachedDirInfo.sector_num) {
			if (isofs_Cache_Dir(CachedDirInfo.pathname, CACHE_NEXT) != 1) {
				// failed to cache next block (should return TRUE even if
				// there is no more directory, as long as a CD read didnt fail
				return -1;
			}
		}
		else
			break;

		(u8 *)tocEntryPointer = CachedDirInfo.cache;
	}

	
	// Next do files
	// Cache the start of the requested directory
	if (isofs_Cache_Dir(CachedDirInfo.pathname, CACHE_START) != 1) {
		#ifdef NETLOG_DEBUG
			netlog_send("isofs_GetDir - Call of isofs_Cache_Dir failed\n");
		#endif

		return -1;
	}

	(u8 *)tocEntryPointer = CachedDirInfo.cache;

	// skip the first self-referencing entry
	(u8 *)tocEntryPointer += tocEntryPointer->length;

	// skip the parent entry if this is the root
	if (CachedDirInfo.path_depth == 0)
		(u8 *)tocEntryPointer += tocEntryPointer->length;

	dir_entry = 0;

	while(1) {
		#ifdef NETLOG_DEBUG
			netlog_send("isofs_GetDir - inside while-loop\n");
		#endif

		// parse the current cache block
		for( ; (u8 *)tocEntryPointer < (CachedDirInfo.cache + (CachedDirInfo.cache_size << 11)); (u8 *)tocEntryPointer += tocEntryPointer->length)	{
			if (tocEntryPointer->length == 0) {
				// if we have a toc entry length of zero,
				// then we've either reached the end of the sector, or the end of the dir
				// so point to next sector (if there is one - will be checked by next condition)

				(u8 *)tocEntryPointer = CachedDirInfo.cache + (((((u8 *)tocEntryPointer - CachedDirInfo.cache) >> 11) + 1) << 11);
			}

			if ((u8 *)tocEntryPointer >= CachedDirInfo.cache + (CachedDirInfo.cache_size << 11)) {
				// we've reached the end of the current cache block (which may be end of entire dir
				// so just break the loop
				break;
			}

			// Check if the current entry is a dir or a file
			if ((tocEntryPointer->fileProperties & 0x02) == 0)	{
				// it must be a file
				TocEntryCopy(&localTocEntry, tocEntryPointer);

				#ifdef NETLOG_DEBUG
					netlog_send("We found a file: %s\n", tocEntryPointer->filename);
				#endif

				if ((tocEntry) && (matched_entries == index)) {
					TocEntryCopy(tocEntry, tocEntryPointer);
					return 1;
				}
				
				matched_entries++;
			}
			dir_entry++;
		} // end of the current cache block
			
		// if there is more dir to load, then load next chunk, else finish
		if ((CachedDirInfo.cache_offset+CachedDirInfo.cache_size) < CachedDirInfo.sector_num) {
			if (isofs_Cache_Dir(CachedDirInfo.pathname, CACHE_NEXT) != 1) {
				// failed to cache next block (should return TRUE even if
				// there is no more directory, as long as a CD read didnt fail
				return -1;
			}
		}
		else
			break;

		(u8 *)tocEntryPointer = CachedDirInfo.cache;
	}

	// reached the end of the dir
	return (matched_entries);
}

//-------------------------------------------------------------- 
int skipmod_check(const char *filename)
{
	int i;
	char *p;
	
	for (i=0; i<strlen(filename); i++)
		skipmod_name[i] = toupper(filename[i]);
			
	strcpy(skipmod_modlist, skipmod_tab);	
		
	p = strtok(skipmod_modlist, "\n");

	while (p) {
		if (strlen(skipmod_name) >= strlen(p)) {
			if (strstr(skipmod_name, p))
				return 1;		
		}
			
		p = strtok(NULL, "\n");			
	}
	
	return 0;
}

//-------------------------------------------------------------- 
int isofs_Open(const char *filename, int mode)
{
	register int fd;
	FHANDLE *fh;
	static struct TocEntry tocEntry;	

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_Open: filename %s mode = %d\n", filename, mode);
	#endif

	// attribute new file handle	
	for (fd = 0; fd < MAX_FDHANDLES; fd++) {
		fh = (FHANDLE *)&isofs_fdhandles[fd];
		if (fh->status == 0)
			break;
	}

	// check mode
	if (mode != O_RDONLY)
		return -2;	
			
	// too much files opened
	if (fd == MAX_FDHANDLES)
		return -7;
			
	memset((void *)fh, 0, sizeof (FHANDLE));	
		
	// check if the file exists
	if (isofs_FindFile(filename, &tocEntry) != 1)
		return -1;
				
	fh->lsn = tocEntry.fileLBA;
	fh->filesize = tocEntry.fileSize;	
	fh->status = 1;

	if (skipmod_check(filename)) {
		fh->filesize = size_dummy_irx;
		fh->status = 0xff;
	}
		
	#ifdef NETLOG_DEBUG
		netlog_send("isofs_Open: fd = %d LBA = %d filesize = %d\n", fd, fh->lsn, fh->filesize);
	#endif
	
	return fd;	
}

//-------------------------------------------------------------- 
int isofs_Close(int fd)
{
	FHANDLE *fh;

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_Close: fd = %d\n", fd);
	#endif
		
	if (!((u32)fd < MAX_FDHANDLES))
		return -5;
			
	fh = (FHANDLE *)&isofs_fdhandles[fd];	
	if (!fh->status) 
		return -5;	
				
	fh->status = 0;
			
	return 0;		
}

//-------------------------------------------------------------- 
int isofs_CloseAll(void)
{
	register int fd = 0;
	register int rv = 0;
	register int rc;
	
	do {
		if (isofs_fdhandles[fd].status) {
			rc = isofs_Close(fd);
			if (rc < rv)
				rv = rc;   
		}
		fd++;
		
	} while (fd < MAX_FDHANDLES);

	return rv;
}

//-------------------------------------------------------------- 
int isofs_Read(int fd, void *buf, u32 nbytes)
{
	register int r;
	FHANDLE *fh;

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_Read: fd = %d nbytes = %d\n", fd, nbytes);
	#endif
			
	if (!((u32)fd < MAX_FDHANDLES))
		return -5;
			
	fh = (FHANDLE *)&isofs_fdhandles[fd];	
	if (!fh->status) 
		return -5;
		
	if (fh->position >= fh->filesize)
		return 0;

	if (nbytes >= (fh->filesize - fh->position))
		nbytes = fh->filesize - fh->position;
		
	if (fh->status == 0xff) {
		u8 *p = (u8 *)&dummy_irx;
		memcpy(buf, &p[fh->position], nbytes);
		r = nbytes;		
	}	
	else {
		r = isofs_ReadISO(fh->lsn, fh->position, nbytes, buf);
	}
	fh->position += r;

    return r;
}

//-------------------------------------------------------------- 
int isofs_Seek(int fd, u32 offset, int origin)
{
	register int r;
	FHANDLE *fh;
	
	#ifdef NETLOG_DEBUG
		netlog_send("isofs_Seek: fd = %d offset = %d origin = %d\n", fd, offset, origin);
	#endif
	
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

//--------------------------------------------------------------
void isofs_fillstat(struct TocEntry *tocEntry, fio_stat_t *stat)
{
	u16 year = tocEntry->dateStamp[0] + 1900;
	stat->ctime[7] = year >> 8;
	stat->ctime[6] = year;
	stat->ctime[5] = tocEntry->dateStamp[1]; // month
    stat->ctime[4] = tocEntry->dateStamp[2]; // day
	stat->ctime[3] = tocEntry->dateStamp[3]; // hour
	stat->ctime[2] = tocEntry->dateStamp[4]; // minute
    stat->ctime[1] = tocEntry->dateStamp[5]; // second

    memcpy((void *)stat->atime, (void *)stat->ctime, 8);
    memcpy((void *)stat->mtime, (void *)stat->ctime, 8);    			

	stat->size = tocEntry->fileSize;
	stat->mode = ((tocEntry->fileProperties & 0x2) ? 0x1049 : 0x2000) | 0x124;	
}

//-------------------------------------------------------------- 
int isofs_GetStat(char *filename, fio_stat_t *stat)
{
	static struct TocEntry tocEntry;
	
	#ifdef NETLOG_DEBUG
		netlog_send("isofs_GetStat: filename %s\n", filename);
	#endif

	memset((void *)stat, 0, sizeof(fio_stat_t));
		
	// check if the file exists
	if (isofs_FindFile(filename, &tocEntry) != 1)
		return 0;
		
	isofs_fillstat(&tocEntry, stat);	

	//#ifdef NETLOG_DEBUG
	//	netlog_send("isofs_GetStat: mode = %x\n", stat->mode);
	//	netlog_send("isofs_GetStat: size = %x\n", stat->size);
	//#endif
				
	return 1;		
}

//-------------------------------------------------------------- 
int isofs_dOpen(const char *dirname)
{
	register int fd, r, filecount;
	FHANDLE *fh;

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_dOpen: dirname %s\n", dirname);
	#endif

	// attribute new file handle	
	for (fd = 0; fd < MAX_FDHANDLES; fd++) {
		fh = (FHANDLE *)&isofs_fdhandles[fd];
		if (fh->status == 0)
			break;
	}

	r = isofs_GetDir(dirname, NULL, 0);	
	if (r < 0)
		return -3;
		
	filecount = r;	
							
	// too much files opened
	if (fd == MAX_FDHANDLES)
		return -7;
			
	memset((void *)fh, 0, sizeof (FHANDLE));	
				
	fh->filesize = filecount;	
	fh->status = 1;

	strcpy(isofs_dopen_dirname, dirname);
		
	#ifdef NETLOG_DEBUG
		netlog_send("isofs_dOpen: fd = %d LBA = %d filesize = %d\n", fd, fh->lsn, fh->filesize);
	#endif
	
	return fd;	
}

//-------------------------------------------------------------- 
int isofs_dRead(int fd, fio_dirent_t *dirent)
{
	FHANDLE *fh;
	static struct TocEntry dtocEntry;		

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_dRead: fd = %d\n", fd);
	#endif
			
	if (!((u32)fd < MAX_FDHANDLES))
		return -5;
			
	fh = (FHANDLE *)&isofs_fdhandles[fd];	
	if (!fh->status) 
		return -5;

	memset((void *)dirent, 0, sizeof(fio_dirent_t));
		
	if (fh->position >= fh->filesize)
		return 0;
		
	if (isofs_GetDir(isofs_dopen_dirname, &dtocEntry, fh->position) != 1)
		return -1;
			
	fh->position++;	

	isofs_fillstat(&dtocEntry, &dirent->stat);
	
	strncpy(dirent->name, dtocEntry.filename, 128); 	

	#ifdef NETLOG_DEBUG
		netlog_send("isofs_dRead: name = %s\n", dirent->name);	
		netlog_send("isofs_dRead: mode = %x\n", dirent->stat.mode);
		netlog_send("isofs_dRead: size = %d\n", dirent->stat.size);
	#endif
				
    return 1;
}

//-------------------------------------------------------------- 
int isofs_InitDevice(void)
{
	DelDrv(isofs_dev.name);
	if (AddDrv(&isofs_dev)) {
		isofs_CloseAll();
		return 1;
	}
	
	return 0;
}

//-------------------------------------------------------------- 
int isofs_GetDiscType(void) // Export #7
{
	return (int)g_ISO_media;
}

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{				
	iop_sema_t smp;
	register int i;
	char path[255];

	SifInitRpc(0);
	
	// Register isofs export table	
	if (RegisterLibraryEntries(&_exp_isofs) < 0)
		return MODULE_NO_RESIDENT_END;

	// Install new device driver
	if (isofs_InitDevice())
		return MODULE_NO_RESIDENT_END;
		
	smp.attr = 1;
	smp.initial = 1;
	smp.max = 1;
	smp.option = 0;
	g_isofs_io_sema = CreateSema(&smp);

	CachedDirInfo.cache = (u8 *)isofs_dircache;			
	isofs_FlushCache();

#ifdef NETLOG_DEBUG	
	iop_library_table_t *libtable;
	iop_library_t *libptr;
	void **export_tab;	
	char netlog_modname[8] = "netlog\0\0";

	// Get netlog lib ptr
	libtable = GetLibraryEntryTable();
	libptr = libtable->tail;
	while (libptr != 0) {
		for (i=0; i<8; i++) {
			if (libptr->name[i] != netlog_modname[i])
				break;
		} 
		if (i==8)
			break;	
		libptr = libptr->prev;
	}
	
	// Get netlog export table	 
	export_tab = (void **)(((struct irx_export_table *)libptr)->fptrs);
	
	// Set functions pointers here
	netlog_send = export_tab[6];
#endif

	// check if number of parts have been filled
	if (g_ISO_parts==0x69){
		printf("ERROR: incorrect number of parts\n");
		return MODULE_NO_RESIDENT_END;
	}

	// set all file handles to -1
	memset(&g_iso_fd, -1,sizeof(g_iso_fd));

	// open all ISO parts
	for (i=0; i<g_ISO_parts; i++) {
		if (g_ISO_mode == USB_MODE)
			sprintf(path,"mass:%s.%02x", g_ISO_name, i);
		else if (g_ISO_mode == ETH_MODE)
			sprintf(path,"smb0:\\%s.%02x", g_ISO_name, i);
		g_iso_fd[i] = open(path, O_RDONLY);
		printf("%s ret %d\n", path, g_iso_fd[i]);
	}
		
	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------- 
int fs_dummy(void)
{
	return 0;
}

//-------------------------------------------------------------- 
int fs_init(iop_device_t *dev)
{
	return 0;
}

//-------------------------------------------------------------- 
int fs_deinit(iop_device_t *dev)
{
	isofs_CloseAll();
	
	return 0;
}

//-------------------------------------------------------------- 
int fs_open(iop_file_t *f, char *filename, int mode, int flags)
{
	register int r;
	
	WaitSema(g_isofs_io_sema);
	
	r = isofs_Open(filename, mode);
	if (r >= 0)
		f->privdata = (void*)r;

	SignalSema(g_isofs_io_sema);
	
	return r;
}

//-------------------------------------------------------------- 
int fs_close(iop_file_t *f)
{
	register int r;

	WaitSema(g_isofs_io_sema);
	
	r = isofs_Close((int)f->privdata);
	
	SignalSema(g_isofs_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int fs_lseek(iop_file_t *f, u32 pos, int where)
{
	register int r;

	WaitSema(g_isofs_io_sema);
	
	r = isofs_Seek((int)f->privdata, pos, where);
	
	SignalSema(g_isofs_io_sema);
	
	return r;
}

//-------------------------------------------------------------- 
int fs_read(iop_file_t *f, void *buf, u32 size)
{
	register int r;

	WaitSema(g_isofs_io_sema);
	
	r = isofs_Read((int)f->privdata, buf, size);	
	
	SignalSema(g_isofs_io_sema);
	
	return r;
}

//-------------------------------------------------------------- 
int fs_getstat(iop_file_t *f, char *filename, fio_stat_t *stat)
{
	register int r;

	WaitSema(g_isofs_io_sema);
	
	r = isofs_GetStat(filename, stat);	
	
	SignalSema(g_isofs_io_sema);
	
	return r;
}

//-------------------------------------------------------------- 
int fs_dopen(iop_file_t *f, char *dirname)
{		
	register int r;
	
	WaitSema(g_isofs_io_sema);
	
	r = isofs_dOpen(dirname);
	if (r >= 0)
		f->privdata = (void*)r;

	SignalSema(g_isofs_io_sema);
	
	return r;
}

//-------------------------------------------------------------- 
int fs_dclose(iop_file_t *f)
{
	register int r;

	WaitSema(g_isofs_io_sema);
	
	r = isofs_Close((int)f->privdata);
	
	SignalSema(g_isofs_io_sema);
	
	return r;
}

//-------------------------------------------------------------- 
int fs_dread(iop_file_t *f, fio_dirent_t *dirent)
{
	register int r;

	WaitSema(g_isofs_io_sema);
	
	r = isofs_dRead((int)f->privdata, dirent);	
	
	SignalSema(g_isofs_io_sema);
	
	return r;
}
