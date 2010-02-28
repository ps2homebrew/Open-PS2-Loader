/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open-ps2-loader README & LICENSE files for further details.
*/

#include <stdio.h>
#include <loadcore.h>
#include <ioman.h>
#include <io_common.h>
#include <thsemap.h>
#include <sysclib.h>
#include <errno.h>

#define MODNAME "cddev"
IRX_ID(MODNAME, 1, 1);

typedef struct {
	u32	lsn;
	u32	size;
	char	name[16];
	u8	date[8];
	u32	flag;
} cdl_file_t;

typedef struct {
	u8	trycount;
	u8	spindlctrl;
	u8	datapattern;
	u8	pad;
} cd_read_mode_t;

// cdvdman imports
int sceCdInit(int init_mode);						// #4
int sceCdRead(u32 lsn, u32 sectors, void *buf, cd_read_mode_t *mode); 	// #6
int sceCdSync(int mode); 						// #11
int sceCdDiskReady(int mode); 						// #13
int sceCdLayerSearchFile(cdl_file_t *fp, const char *name, int layer);	// #84

// driver ops protypes
int cddev_dummy(void);
int cddev_init(iop_device_t *dev);
int cddev_deinit(iop_device_t *dev);
int cddev_open(iop_file_t *f, char *filename, int mode);
int cddev_close(iop_file_t *f);
int cddev_read(iop_file_t *f, void *buf, u32 size);
int cddev_lseek(iop_file_t *f, u32 offset, int where);

// driver ops func tab
void *cddev_ops[17] = {
	(void*)cddev_init,
	(void*)cddev_deinit,
	(void*)cddev_dummy,
	(void*)cddev_open,
	(void*)cddev_close,
	(void*)cddev_read,
	(void*)cddev_dummy,
	(void*)cddev_lseek,
	(void*)cddev_dummy,
	(void*)cddev_dummy,
	(void*)cddev_dummy,
	(void*)cddev_dummy,
	(void*)cddev_dummy,
	(void*)cddev_dummy,
	(void*)cddev_dummy,
	(void*)cddev_dummy,
	(void*)cddev_dummy
};

// driver descriptor
static iop_device_t cddev_dev = {
	"cddev", 
	IOP_DT_FS,
	1,
	"cddev",
	(struct _iop_device_ops *)&cddev_ops
};

typedef struct {
	iop_file_t *f;
	u32	lsn;
	u32	filesize;
	u32	position;
} FHANDLE;

#define MAX_FDHANDLES 		64
FHANDLE cddev_fdhandles[MAX_FDHANDLES];

static int cddev_io_sema;

#define CDDEV_FS_SECTORS	30
#define CDDEV_FS_BUFSIZE	CDDEV_FS_SECTORS * 2048
static u8 cddev_fs_buf[CDDEV_FS_BUFSIZE + 2*2048] __attribute__((aligned(64)));

//-------------------------------------------------------------- 
int _start(int argc, char** argv)
{
	DelDrv("cddev");
	AddDrv((iop_device_t *)&cddev_dev);

	sceCdInit(0);

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------- 
int cddev_dummy(void)
{
	return -EPERM;
}

//-------------------------------------------------------------- 
int cddev_init(iop_device_t *dev)
{
	iop_sema_t smp;

	smp.initial = 1;
	smp.max = 1;
	smp.attr = 1;
	smp.option = 0;

	cddev_io_sema = CreateSema(&smp);

	return 0;
}

//-------------------------------------------------------------- 
int cddev_deinit(iop_device_t *dev)
{
	DeleteSema(cddev_io_sema);

	return 0;
}

//-------------------------------------------------------------- 
FHANDLE *cddev_getfilefreeslot(void)
{
	register int i;
	FHANDLE *fh;

	for (i=0; i<MAX_FDHANDLES; i++) {
		fh = (FHANDLE *)&cddev_fdhandles[i];
		if (fh->f == NULL)
			return fh;
	}

	return 0;
}

//-------------------------------------------------------------- 
int cddev_open(iop_file_t *f, char *filename, int mode)
{
	register int r = 0;
	FHANDLE *fh;
	cdl_file_t cdfile;

	if (!filename)
		return -ENOENT;

	WaitSema(cddev_io_sema);

	fh = cddev_getfilefreeslot();
	if (fh) {
		sceCdDiskReady(0);
		r = sceCdLayerSearchFile(&cdfile, filename, 0);
		if (r != 1) {
			r = sceCdLayerSearchFile(&cdfile, filename, 1);
			if (r != 1) {
				r = -ENOENT;
				SignalSema(cddev_io_sema);
				return r;
			}
		}
		f->privdata = fh;
		fh->f = f;
		fh->lsn = cdfile.lsn;
		fh->filesize = cdfile.size;
		fh->position = 0;
		r = 0;
	}
	else
		r = -EMFILE;

	SignalSema(cddev_io_sema);

	return r;
}

//-------------------------------------------------------------- 
int cddev_close(iop_file_t *f)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;

	WaitSema(cddev_io_sema);

	if (fh)
		memset(fh, 0, sizeof(FHANDLE));

	SignalSema(cddev_io_sema);

	return 0;
}

//-------------------------------------------------------------- 
int cddev_read(iop_file_t *f, void *buf, u32 size)
{
	FHANDLE *fh = (FHANDLE *)f->privdata;
	register int rpos, sectorpos;
	register u32 nsectors, nbytes;

	rpos = 0;

	if ((fh->position + size) > fh->filesize)
		size = fh->filesize - fh->position;

	WaitSema(cddev_io_sema);

	sceCdDiskReady(0);

	while (size) {
		nbytes = CDDEV_FS_BUFSIZE;
		if (size < nbytes)
			nbytes = size;

		nsectors = nbytes >> 11;
		sectorpos = fh->position & 2047;

		if (sectorpos)
			nsectors++;

		if (nbytes & 2047)
			nsectors++;

		sceCdRead(fh->lsn + ((fh->position & -2048) >> 11), nsectors, cddev_fs_buf, NULL);
		sceCdSync(0);
		memcpy(buf, &cddev_fs_buf[sectorpos], nbytes);

		rpos += nbytes;
		buf += nbytes;
		size -= nbytes;
		fh->position += nbytes;
	}

	SignalSema(cddev_io_sema);

	return rpos;	
}

//-------------------------------------------------------------- 
int cddev_lseek(iop_file_t *f, u32 offset, int where)
{
	register int r;
	FHANDLE *fh = (FHANDLE *)f->privdata;

	WaitSema(cddev_io_sema);

	switch (where) {
		case SEEK_CUR:
			r = fh->position + offset;
			if (r > fh->filesize) {
				r = -EINVAL;
				goto ssema;
			}
			break;
		case SEEK_SET:
			r = offset;
			if (fh->filesize < offset) {
				r = -EINVAL;
				goto ssema;
			}
			break;
		case SEEK_END:
			r = fh->filesize;
			break;
		default:
			r = -EINVAL;
			goto ssema;
	}

	fh->position = r;

ssema:
	SignalSema(cddev_io_sema);

	return r;
}

//-------------------------------------------------------------------------
DECLARE_IMPORT_TABLE(cdvdman, 1, 1)
DECLARE_IMPORT(4, sceCdInit)
DECLARE_IMPORT(6, sceCdRead)
DECLARE_IMPORT(11, sceCdSync)
DECLARE_IMPORT(13, sceCdDiskReady)
DECLARE_IMPORT(84, sceCdLayerSearchFile)
END_IMPORT_TABLE
