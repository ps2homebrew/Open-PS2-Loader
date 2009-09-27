//---------------------------------------------------------------------------
//File name:    fs_driver.c
//---------------------------------------------------------------------------
/*
 * fat_driver.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 * (C) 2004  Hermes (support for sector sizes from 512 to 4096 bytes)
 * (C) 2004  raipsu (fs_dopen, fs_dclose, fs_dread, fs_getstat implementation)
 *
 * FAT filesystem layer
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */
//---------------------------------------------------------------------------
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <io_common.h>
#include <ioman.h>

#ifdef WIN32
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#else
#include <sysclib.h>
#include <thsemap.h>
#include <thbase.h>
#include <sysmem.h>
#define malloc(a)       AllocSysMemory(0,(a), NULL)
#define free(a)         FreeSysMemory((a))
#endif

#include "usbhd_common.h"
#include "fat_driver.h"
#include "fat_write.h"

//#define DEBUG  //comment out this line when not debugging

#include "mass_debug.h"

#define IOCTL_CKFREE 0xBABEC0DE  //dlanor: Ioctl request code => Check free space
#define IOCTL_RENAME 0xFEEDC0DE  //dlanor: Ioctl request code => Rename

#define FLUSH_SECTORS		fat_flushSectors

typedef struct _fs_rec {
	int           file_flag;
  //This flag is always 1 for a file, and always 0 for a folder (different typedef)
  //Routines that handle both must test it, and then typecast the privdata pointer
  //to the type that is appropriate for the given case. (see also fs_dir typedef)
	unsigned int  filePos;
	int           mode;	//file open mode
	unsigned int  sfnSector; //short filename sector  - write support
	int           sfnOffset; //short filename offset  - write support
	int           sizeChange; //flag
	fat_dir fatdir;
} fs_rec;

typedef struct _fs_dir {
	int           file_flag;
	//This flag is always 1 for a file, and always 0 for a folder (different typedef)
	//Routines that handle both must test it, and then typecast the privdata pointer
	//to the type that is appropriate for the given case. (see also fs_rec typedef)
	int status;
	fat_dir fatdir;
	fat_dir_list fatdlist;
} fs_dir;

#define MAX_FILES 16
fs_rec  fsRec[MAX_FILES]; //file info record

#ifndef WIN32
static iop_device_t fs_driver;
static iop_device_ops_t fs_functarray;
#endif

void fillStat(fio_stat_t *stat, fat_dir *fatdir)
{
	stat->mode = FIO_SO_IROTH | FIO_SO_IXOTH;
	if (fatdir->attr & FAT_ATTR_DIRECTORY) {
		stat->mode |= FIO_SO_IFDIR;
	} else {
		stat->mode |= FIO_SO_IFREG;
	}
	if (!(fatdir->attr & FAT_ATTR_READONLY)) {
		stat->mode |= FIO_SO_IWOTH;
	}

	stat->size = fatdir->size;

	//set created Date: Day, Month, Year
	stat->ctime[4] = fatdir->cdate[0];
	stat->ctime[5] = fatdir->cdate[1];
	stat->ctime[6] = fatdir->cdate[2];
	stat->ctime[7] = fatdir->cdate[3];

	//set created Time: Hours, Minutes, Seconds
	stat->ctime[3] = fatdir->ctime[0];
	stat->ctime[2] = fatdir->ctime[1];
	stat->ctime[1] = fatdir->ctime[2];

	//set accessed Date: Day, Month, Year
	stat->atime[4] = fatdir->adate[0];
	stat->atime[5] = fatdir->adate[1];
	stat->atime[6] = fatdir->adate[2];
	stat->atime[7] = fatdir->adate[3];

	//set modified Date: Day, Month, Year
	stat->mtime[4] = fatdir->mdate[0];
	stat->mtime[5] = fatdir->mdate[1];
	stat->mtime[6] = fatdir->mdate[2];
	stat->mtime[7] = fatdir->mdate[3];
 
	//set modified Time: Hours, Minutes, Seconds
	stat->mtime[3] = fatdir->mtime[0];
	stat->mtime[2] = fatdir->mtime[1];
	stat->mtime[1] = fatdir->mtime[2];
}

/*************************************************************************************/
/*    File IO functions                                                              */
/*************************************************************************************/


int	nameSignature;
int	removalTime;
int     removalResult;

//---------------------------------------------------------------------------
fs_rec* fs_findFreeFileSlot(void) {
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (fsRec[i].file_flag == -1) {
			return &fsRec[i];
			break;
		}
	}
	return NULL;
}

//---------------------------------------------------------------------------
fs_rec* fs_findFileSlotByName(const char* name) {
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (fsRec[i].file_flag >= 0 && strEqual(fsRec[i].fatdir.name, (unsigned char*) name) == 0) {
			return &fsRec[i];
		}
	}
	return NULL;
}

//---------------------------------------------------------------------------
static int _lock_sema_id = 0;

//---------------------------------------------------------------------------
int _fs_init_lock(void)
{
#ifndef WIN32
	iop_sema_t sp;

	sp.initial = 1;
	sp.max = 1;
	sp.option = 0;
	sp.attr = 0;
	if((_lock_sema_id = CreateSema(&sp)) < 0) { return(-1); }
#endif

	return(0);
}

//---------------------------------------------------------------------------
int _fs_lock(void)
{
#ifndef WIN32
    if(WaitSema(_lock_sema_id) != _lock_sema_id) { return(-1); }
#endif
    return(0);
}

//---------------------------------------------------------------------------
int _fs_unlock(void)
{
#ifndef WIN32
    SignalSema(_lock_sema_id);
#endif
    return(0);
}

//---------------------------------------------------------------------------
void fs_reset(void)
{
	int i;
	for (i = 0; i < MAX_FILES; i++) { fsRec[i].file_flag = -1; }
#ifndef WIN32
	if(_lock_sema_id >= 0) { DeleteSema(_lock_sema_id); }
#endif
	_fs_init_lock();
}

//---------------------------------------------------------------------------
int fs_inited = 0;

//---------------------------------------------------------------------------
int fs_dummy(void)
{
	return -5;
}

//---------------------------------------------------------------------------
int fs_init(iop_device_t *driver)
{
    if(fs_inited) { return(1); }

	fs_reset();

    fs_inited = 1;
	return 1;
}

//---------------------------------------------------------------------------
int fs_open(iop_file_t* fd, const char *name, int mode) {
	fat_driver* fatd;
	fs_rec* rec;
	fs_rec* rec2;
	int ret;
	unsigned int cluster;
	char escapeNotExist;

	_fs_lock();

	XPRINTF("USBHDFSD: fs_open called: %s mode=%X \n", name, mode) ;

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	//check if the file is already open
	rec2 = fs_findFileSlotByName(name);
	if (rec2 != NULL) {
		if ((mode & O_WRONLY) || //current file is opened for write
			(rec2->mode & O_WRONLY) ) {//other file is opened for write
			_fs_unlock();
			return 	-EACCES;
		}
	}

	//check if the slot is free
	rec = fs_findFreeFileSlot();
	if (rec == NULL) { _fs_unlock(); return -EMFILE; }

	if((mode & O_WRONLY))  { //dlanor: corrected bad test condition
		cluster = 0; //start from root

		escapeNotExist = 1;
		if (mode & O_CREAT) {
			XPRINTF("USBHDFSD: FAT I: O_CREAT detected!\n");
			escapeNotExist = 0;
		}

		rec->sfnSector = 0;
		rec->sfnOffset = 0;
		ret = fat_createFile(fatd, name, 0, escapeNotExist, &cluster, &rec->sfnSector, &rec->sfnOffset);
		if (ret < 0) {
		    _fs_unlock(); 
			return ret;
		}
		//the file already exist but mode is set to truncate
		if (ret == 2 && (mode & O_TRUNC)) {
			XPRINTF("USBHDFSD: FAT I: O_TRUNC detected!\n");
			fat_truncateFile(fatd, cluster, rec->sfnSector, rec->sfnOffset);
		}
	}

	//find the file
	cluster = 0; //allways start from root
	XPRINTF("USBHDFSD: Calling fat_getFileStartCluster from fs_open\n");
	ret = fat_getFileStartCluster(fatd, name, &cluster, &rec->fatdir);
	if (ret < 0) {
	    _fs_unlock(); 
		return ret;
	}
	if ((rec->fatdir.attr & FAT_ATTR_DIRECTORY) == FAT_ATTR_DIRECTORY) {
		// Can't open a directory with fioOpen
		_fs_unlock();
		return -EISDIR;
	}

	rec->file_flag = 1;
	rec->mode = mode;
	rec->filePos = 0;
	rec->sizeChange  = 0;

	if ((mode & O_APPEND) && (mode & O_WRONLY)) {
		XPRINTF("USBHDFSD: FAT I: O_APPEND detected!\n");
		rec->filePos = rec->fatdir.size;
	}

	//store the slot to user parameters
	fd->privdata = rec;

	_fs_unlock();
	return 1;
}

//---------------------------------------------------------------------------
int fs_close(iop_file_t* fd) {
	fat_driver* fatd;
	fs_rec* rec = (fs_rec*)fd->privdata;
    fd->privdata = 0;

    if (rec == 0)
        return -EBADF;

    _fs_lock();

	rec->file_flag = -1;

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	if ((rec->mode & O_WRONLY)) {
		//update direntry size and time
		if (rec->sizeChange) {
			fat_updateSfn(fatd, rec->fatdir.size, rec->sfnSector, rec->sfnOffset);
		}

		FLUSH_SECTORS(fatd);
	}

    _fs_unlock();
	return 0;
}

//---------------------------------------------------------------------------
int fs_lseek(iop_file_t* fd, unsigned long offset, int whence) {
	fat_driver* fatd;
	fs_rec* rec = (fs_rec*)fd->privdata;

    if (rec == 0)
        return -EBADF;

    _fs_lock();

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	switch(whence) {
		case SEEK_SET:
			rec->filePos = offset;
			break;
		case SEEK_CUR:
			rec->filePos += offset;
			break;
		case SEEK_END:
			rec->filePos = rec->fatdir.size + offset;
			break;
		default:
		    _fs_unlock();
			return -1;
	}
	if (rec->filePos < 0) {
		rec->filePos = 0;
	}
	if (rec->filePos > rec->fatdir.size) {
		rec->filePos = rec->fatdir.size;
	}

	_fs_unlock();
	return rec->filePos;
}

//---------------------------------------------------------------------------
int fs_write(iop_file_t* fd, void * buffer, int size )
{
	fat_driver* fatd;
	fs_rec* rec = (fs_rec*)fd->privdata;
	int result;
	int updateClusterIndices = 0;

    if (rec == 0)
        return -EBADF;

    _fs_lock();

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	if (rec->file_flag != 1) {
		_fs_unlock();
		return -EACCES;
	}

	if (!(rec->mode & O_WRONLY)) {
		_fs_unlock();
		return -EACCES;
	}

	if (size <= 0) { _fs_unlock(); return 0; }

	result = fat_writeFile(fatd, &rec->fatdir, &updateClusterIndices, rec->filePos, (unsigned char*) buffer, size);
	if (result > 0) { //write succesful
		rec->filePos += result;
		if (rec->filePos > rec->fatdir.size) {
			rec->fatdir.size = rec->filePos;
			rec->sizeChange = 1;
			//if new clusters allocated - then update file cluster indices
			if (updateClusterIndices) {
				fat_setFatDirChain(fatd, &rec->fatdir);
			}
		}
	}
	
	_fs_unlock();
	return result;
}

//---------------------------------------------------------------------------
int fs_read(iop_file_t* fd, void * buffer, int size ) {
	fat_driver* fatd;
	fs_rec* rec = (fs_rec*)fd->privdata;
	int result;

    if (rec == 0)
        return -EBADF;

    _fs_lock();

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	if (rec->file_flag != 1) {
		_fs_unlock();
		return -EACCES;
	}

	if (!(rec->mode & O_RDONLY)) {
		_fs_unlock();
		return -EACCES;
	}

	if (size<=0) {
		_fs_unlock();
		return 0;
	}

	if ((rec->filePos+size) > rec->fatdir.size) {
		size = rec->fatdir.size - rec->filePos;
	}

	result = fat_readFile(fatd, &rec->fatdir, rec->filePos, (unsigned char*) buffer, size);
	if (result > 0) { //read succesful
		rec->filePos += result;
	}

	_fs_unlock();
	return result;
}


//---------------------------------------------------------------------------
int getNameSignature(const char *name) {
	int ret;
	int i;
	ret = 0;

	for (i=0; name[i] != 0 ; i++) ret += (name[i]*i/2 + name[i]);
	return ret;
}

//---------------------------------------------------------------------------
int getMillis()
{
#ifdef WIN32
    return 0;
#else
	iop_sys_clock_t clock;
	u32 sec, usec;

	GetSystemTime(&clock);
	SysClock2USec(&clock, &sec, &usec);
	return   (sec*1000) + (usec/1000);
#endif
}

//---------------------------------------------------------------------------
int fs_remove (iop_file_t *fd, const char *name) {
	fat_driver* fatd;
	fs_rec* rec;
	int result;

    _fs_lock();

	fatd = fat_getData(fd->unit);
	if (fatd == NULL)
    {
		result = -ENODEV;
		removalResult = result;
        _fs_unlock();
 		return result;
	}

	rec = fs_findFileSlotByName(name);
	//store filename signature and time of removal
	nameSignature = getNameSignature(name);
	removalTime = getMillis();

	//file is opened - can't delete the file
	if (rec != NULL) {
		result = -EINVAL;
		removalResult = result;
        _fs_unlock();
		return result;
	}

	result = fat_deleteFile(fatd, name, 0);
	FLUSH_SECTORS(fatd);
	removalTime = getMillis(); //update removal time
	removalResult = result;

    _fs_unlock();
	return result;
}

//---------------------------------------------------------------------------
int fs_mkdir  (iop_file_t *fd, const char *name) {
	fat_driver* fatd;
	int ret;
	int sfnOffset;
	unsigned int sfnSector;
	unsigned int cluster;
	int sig, millis;

    _fs_lock();

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	XPRINTF("USBHDFSD: fs_mkdir: name=%s \n",name);
	//workaround for bug that invokes fioMkdir right after fioRemove
	sig = getNameSignature(name);
	millis = getMillis();
	if (sig == nameSignature && (millis - removalTime) < 1000) {
        _fs_unlock();
		return removalResult; //return the original return code from fs_remove
	}

	ret = fat_createFile(fatd, name, 1, 0, &cluster,  &sfnSector, &sfnOffset);

	//directory of the same name already exist
	if (ret == 2) {
		ret = -EEXIST;
	}
	FLUSH_SECTORS(fatd);

    _fs_unlock();
	return ret;
}

//---------------------------------------------------------------------------
// NOTE! you MUST call fioRmdir with device number in the name
// or else this fs_rmdir function is never called.
// example: fioRmdir("mass:/dir1");  //    <-- doesn't work (bug?)
//          fioRmdir("mass0:/dir1"); //    <-- works fine
int fs_rmdir  (iop_file_t *fd, const char *name) {
	fat_driver* fatd;
	int ret;

    _fs_lock();

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	ret = fat_deleteFile(fatd, name, 1);
	FLUSH_SECTORS(fatd);
    _fs_unlock();
	return ret;
}

//---------------------------------------------------------------------------
int fs_dopen  (iop_file_t *fd, const char *name)
{
	fat_driver* fatd;
	int is_root = 0;
	fs_dir* rec;

	_fs_lock();
    
    XPRINTF("USBHDFSD: fs_dopen called: unit %d name %s\n", fd->unit, name);

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	if( ((name[0] == '/') && (name[1] == '\0'))
		||((name[0] == '/') && (name[1] == '.') && (name[2] == '\0')))
	{
		name = "/";
		is_root = 1;
	}

	fd->privdata = (void*)malloc(sizeof(fs_dir));
	memset(fd->privdata, 0, sizeof(fs_dir)); //NB: also implies "file_flag = 0;"
	rec = (fs_dir *) fd->privdata;

	rec->status = fat_getFirstDirentry(fatd, (char*)name, &rec->fatdlist, &rec->fatdir);

	// root directory may have no entries, nothing else may.
	if(rec->status == 0 && !is_root)
		rec->status = -EFAULT;

	if (rec->status < 0)
		free(fd->privdata);

	_fs_unlock();
	return rec->status;
}

//---------------------------------------------------------------------------
int fs_dclose (iop_file_t *fd)
{
    if (fd->privdata == 0)
        return -EBADF;

	_fs_lock();
    XPRINTF("USBHDFSD: fs_dclose called: unit %d\n", fd->unit);
	free(fd->privdata);
    fd->privdata = 0;
	_fs_unlock();
	return 0;
}

//---------------------------------------------------------------------------
int fs_dread  (iop_file_t *fd, fio_dirent_t *buffer)
{
	fat_driver* fatd;
	int ret;
	fs_dir* rec = (fs_dir *) fd->privdata;

    if (rec == 0)
        return -EBADF;

	_fs_lock();
    
    XPRINTF("USBHDFSD: fs_dread called: unit %d\n", fd->unit);

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

    while (rec->status > 0
        && (rec->fatdir.attr & FAT_ATTR_VOLUME_LABEL
            || ((rec->fatdir.attr & (FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM)) == (FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM))))
        rec->status = fat_getNextDirentry(fatd, &rec->fatdlist, &rec->fatdir);

    ret = rec->status;
	if (rec->status >= 0)
    {
		memset(buffer, 0, sizeof(fio_dirent_t));
		fillStat(&buffer->stat, &rec->fatdir);
		strcpy(buffer->name, (const char*) rec->fatdir.name);
    }

	if (rec->status > 0)
		rec->status = fat_getNextDirentry(fatd, &rec->fatdlist, &rec->fatdir);
    
    _fs_unlock();
    return ret;
}

//---------------------------------------------------------------------------
int fs_getstat(iop_file_t *fd, const char *name, fio_stat_t *stat)
{
	fat_driver* fatd;
	int ret;
	unsigned int cluster = 0;
	fat_dir fatdir;

	_fs_lock();

    XPRINTF("USBHDFSD: fs_getstat called: unit %d name %s\n", fd->unit, name);

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	XPRINTF("USBHDFSD: Calling fat_getFileStartCluster from fs_getstat\n");
	ret = fat_getFileStartCluster(fatd, name, &cluster, &fatdir);
	if (ret < 0) {
		_fs_unlock();
		return ret;
	}

	memset(stat, 0, sizeof(fio_stat_t));
	fillStat(stat, &fatdir);

	_fs_unlock();
	return 0;
}

//---------------------------------------------------------------------------
int fs_chstat (iop_file_t *fd, const char *name, fio_stat_t *stat, unsigned int a)
{
	return fs_dummy();
}

//---------------------------------------------------------------------------
int fs_deinit (iop_device_t *fd)
{
	return fs_dummy();
}

//---------------------------------------------------------------------------
int fs_format (iop_file_t *fd)
{
	return fs_dummy();
}

//---------------------------------------------------------------------------
int fs_ioctl(iop_file_t *fd, unsigned long request, void *data)
{
	fat_driver* fatd;
	fs_dir* rec = (fs_dir *) fd->privdata;
    int ret;

    if (rec == 0)
        return -EBADF;

	_fs_lock();

	fatd = fat_getData(fd->unit);
	if (fatd == NULL) { _fs_unlock(); return -ENODEV; }

	switch (request) {
		case IOCTL_CKFREE:  //Request to calculate free space (ignore file/folder selected)
			ret = fs_dummy();
			break;
		case IOCTL_RENAME:  //Request to rename opened file/folder
			ret = fs_dummy();
			break;
		default:
			ret = fs_dummy();
	}

	_fs_unlock();
	return ret;
}

#ifndef WIN32
/* init file system driver */
int InitFS()
{
	fs_driver.name = "mass";
	fs_driver.type = IOP_DT_FS;
	fs_driver.version = 2;
	fs_driver.desc = "Usb mass storage driver";
	fs_driver.ops = &fs_functarray;

	fs_functarray.init    = fs_init;
	fs_functarray.deinit  = fs_deinit;
	fs_functarray.format  = fs_format;
	fs_functarray.open    = fs_open;
	fs_functarray.close   = fs_close;
	fs_functarray.read    = fs_read;
	fs_functarray.write   = fs_write;
	fs_functarray.lseek   = fs_lseek;
	fs_functarray.ioctl   = fs_ioctl;
	fs_functarray.remove  = fs_remove;
	fs_functarray.mkdir   = fs_mkdir;
	fs_functarray.rmdir   = fs_rmdir;
	fs_functarray.dopen   = fs_dopen;
	fs_functarray.dclose  = fs_dclose;
	fs_functarray.dread   = fs_dread;
	fs_functarray.getstat = fs_getstat;
	fs_functarray.chstat  = fs_chstat;

	DelDrv("mass");
	AddDrv(&fs_driver);

	// should return an error code if AddDrv fails...
	return(0);
}
#endif

//---------------------------------------------------------------------------
//End of file:  fs_driver.c
//---------------------------------------------------------------------------
