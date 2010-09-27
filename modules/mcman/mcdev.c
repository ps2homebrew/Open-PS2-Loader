/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "mcman.h"

// mc driver vars
static int mcman_mc_port = 0;
static int mcman_mc_slot = 0;
static int mcman_io_sema = 0;

// mc driver ops functions prototypes
int mc_init(iop_device_t *iop_dev);
int mc_deinit(iop_device_t *dev);
int mc_open(iop_file_t *f, char *filename, int mode, int flags);
int mc_close(iop_file_t *f);
int mc_lseek(iop_file_t *f, int pos, int where);
int mc_read(iop_file_t *f, void *buf, int size);
int mc_write(iop_file_t *f, void *buf, int size);
int mc_format(iop_file_t *f, char *a1, char *a2, void *a3, int a4);
int mc_remove(iop_file_t *f, char *filename);
int mc_mkdir(iop_file_t *f, char *dirname);
int mc_rmdir(iop_file_t *f, char *dirname);
int mc_dopen(iop_file_t *f, char *dirname);
int mc_dclose(iop_file_t *f);
int mc_dread(iop_file_t *f, fio_dirent_t *dirent);
int mc_getstat(iop_file_t *f, char *filename, fio_stat_t *stat);
int mc_chstat(iop_file_t *f, char *filename, fio_stat_t *stat, u32 statmask);
int mc_ioctl(iop_file_t *f, int a1, void* a2);

// driver ops func tab
void *mcman_mcops[17] = {
	(void*)mc_init,
	(void*)mc_deinit,
	(void*)mc_format,
	(void*)mc_open,
	(void*)mc_close,
	(void*)mc_read,
	(void*)mc_write,
	(void*)mc_lseek,
	(void*)mc_ioctl,
	(void*)mc_remove,
	(void*)mc_mkdir,
	(void*)mc_rmdir,
	(void*)mc_dopen,
	(void*)mc_dclose,
	(void*)mc_dread,
	(void*)mc_getstat,
	(void*)mc_chstat
};

// driver descriptor
static iop_device_t mcman_mcdev = {
	"mc", 
	IOP_DT_FS,
	1,
	"Memory Card",
	(struct _iop_device_ops *)&mcman_mcops
};

//-------------------------------------------------------------- 
int mc_init(iop_device_t *dev)
{
	return 0;
}

//-------------------------------------------------------------- 
int mcman_ioerrcode(int errcode)
{
	register int r = errcode;

	if (r < 0) {
		switch (r + 8) {
			case 0:
				r = -EIO;
				break;
			case 1:
				r = -EMFILE;
				break;
			case 2:
				r = -EEXIST;
				break;
			case 3:
				r = -EACCES;
				break;
			case 4:
				r = -ENOENT;
				break;
			case 5:
				r = -ENOSPC;
				break;
			case 6:
				r = -EFORMAT;
				break;
			default:
				r = -ENXIO;
				break;
		}
	}
	return r;
}

//-------------------------------------------------------------- 
int mcman_modloadcb(char *filename, int *unit, u8 *arg3)
{
	register char *path = filename;
	register int   upos;

	if (*path == 0x20) {
		path++;
		while (*path == 0x20)
			path++;
	}

	if (((u8)path[0] | 0x20) != mcman_mcdev.name[0])
		return 0;
	if (((u8)path[1] | 0x20) != mcman_mcdev.name[1])
		return 0;

	if ((u32)strlen(path) < 2) return 2;

	upos = mcman_chrpos(path, ':');
	
	if (upos < 0)
		upos = strlen(path);

	if (unit) {
		upos --;
		if (((u8)path[upos] - 0x30) < 10)
			*unit = (u8)path[upos] - 0x30;
		else *unit = 0;
	}

	upos--;
	*unit += 2;

	if (arg3) {
		register int m, v;

		*arg3 = 0;
		for (m = 1; ((u8)path[upos --] - 0x30) < 10; m = v << 1) {
			v = m << 2;   
			*arg3 += m * (path[upos] - 0x30);
			v += m;
		}
	}

	return 1;
}

//-------------------------------------------------------------- 
void mcman_unit2card(u32 unit)
{
 	mcman_mc_port = unit & 1;
 	mcman_mc_slot = (unit >> 1) & (MCMAN_MAXSLOT - 1);	
	
	// original mcman/xmcman code below is silly and I doubt it
	// can support more than 2 units anyway...
	/*
	register u32 mask = 0xF0000000;

	while (!(unit & mask)) {
		mask >>= 4;
		if (mask < 0x10)
			break;
	}

 	mcman_mc_port = unit & 0xf;
 	mcman_mc_slot = 0;
 	
 	if (mask < 0xf) {  
		while (mask) {
			mcman_mc_slot = ((u32)(unit & mask)) / ((u32)(mask & (mask >> 0x3))) \
				+ (((mcman_mc_slot << 2) + mcman_mc_slot) << 1);
			mask >>= 4;
		}
	}
	*/
}

//-------------------------------------------------------------- 
int mcman_initdev(void)
{
	iop_sema_t smp;

	SetCheckKelfPathCallback(mcman_modloadcb);

	DelDrv(mcman_mcdev.name);
	if (AddDrv(&mcman_mcdev)) {
		McCloseAll();
		return 1;
	}

	smp.attr = 1;
	smp.initial = 1;
	smp.max = 1;
	smp.option = 0;
	mcman_io_sema = CreateSema(&smp);

	return 0;
}

//-------------------------------------------------------------- 
int mc_deinit(iop_device_t *dev)
{
	DeleteSema(mcman_io_sema);
	McCloseAll();
 
	return 0;
}

//-------------------------------------------------------------- 
int mc_open(iop_file_t *f, char *filename, int mode, int flags)
{
	register int r;

	WaitSema(mcman_io_sema);
	mcman_unit2card(f->unit);
	
	r = McDetectCard2(mcman_mc_port, mcman_mc_slot);
	if (r >= -1) {
		r = McOpen(mcman_mc_port, mcman_mc_slot, filename, mode);
		if (r >= 0)
			f->privdata = (void*)r;
	}
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_close(iop_file_t *f)
{
	register int r;

	WaitSema(mcman_io_sema);
	r = McClose((int)f->privdata);
	SignalSema(mcman_io_sema);

	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_lseek(iop_file_t *f, int pos, int where)
{
	register int r;

	WaitSema(mcman_io_sema);
	r = McSeek((int)f->privdata, pos, where);
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_read(iop_file_t *f, void *buf, int size)
{
	register int r;

	WaitSema(mcman_io_sema);
	r = McRead((int)f->privdata, buf, size);
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_write(iop_file_t *f, void *buf, int size)
{
	register int r;

	WaitSema(mcman_io_sema);
	r = McWrite((int)f->privdata, buf, size);
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_format(iop_file_t *f, char *a1, char *a2, void *a3, int a4)
{
	register int r;

	WaitSema(mcman_io_sema);
	mcman_unit2card(f->unit);
	
	r = McDetectCard2(mcman_mc_port, mcman_mc_slot);
	if (r >= -2) {
		r = McFormat(mcman_mc_port, mcman_mc_slot);
	}
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_remove(iop_file_t *f, char *filename)
{
	register int r;

	WaitSema(mcman_io_sema);
	mcman_unit2card(f->unit);

	r = McDetectCard2(mcman_mc_port, mcman_mc_slot);
	if (r >= -1) {
		r = McDelete(mcman_mc_port, mcman_mc_slot, filename, 0);
	}
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_mkdir(iop_file_t *f, char *dirname)
{
	register int r;

	WaitSema(mcman_io_sema);
	mcman_unit2card(f->unit);

	r = McDetectCard2(mcman_mc_port, mcman_mc_slot);
	if (r >= -1) {
		r = McOpen(mcman_mc_port, mcman_mc_slot, dirname, 0x40);
	}
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_rmdir(iop_file_t *f, char *dirname)
{
	register int r;

	WaitSema(mcman_io_sema);
	mcman_unit2card(f->unit);
	
	r = McDetectCard2(mcman_mc_port, mcman_mc_slot);
	if (r >= -1) {
		r = McDelete(mcman_mc_port, mcman_mc_slot, dirname, 0);
	}
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_dopen(iop_file_t *f, char *dirname)
{
	register int r;

	WaitSema(mcman_io_sema);
	mcman_unit2card(f->unit);
	
	r = McDetectCard2(mcman_mc_port, mcman_mc_slot);
	if (r >= -1) {
		r = McOpen(mcman_mc_port, mcman_mc_slot, dirname, 0);
		if (r >= 0)
			f->privdata = (void*)r;
	}
	
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_dclose(iop_file_t *f)
{
	register int r;

	WaitSema(mcman_io_sema);
	r = McClose((int)f->privdata);
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_dread(iop_file_t *f, fio_dirent_t *dirent)
{
	register int r;

	WaitSema(mcman_io_sema);
	r = mcman_dread((int)f->privdata, dirent);
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_getstat(iop_file_t *f, char *filename, fio_stat_t *stat)
{
	register int r;

	WaitSema(mcman_io_sema);
	mcman_unit2card(f->unit);
	
	r = McDetectCard2(mcman_mc_port, mcman_mc_slot);
	if (r >= -1) {
		r = mcman_getstat(mcman_mc_port, mcman_mc_slot, filename, stat);
	}
	
	SignalSema(mcman_io_sema);
	
	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_chstat(iop_file_t *f, char *filename, fio_stat_t *stat, u32 statmask)
{
	register int r, flags;
	sceMcTblGetDir mctbl;

	WaitSema(mcman_io_sema);
	mcman_unit2card(f->unit);
	
	r = McDetectCard2(mcman_mc_port, mcman_mc_slot);
	if (r >= -1) {
		if (statmask & SCE_CST_ATTR) {
			flags = 0x008;
			mctbl.Reserve2 = stat->attr;
		}
		else flags = 0x000;

		if (statmask & SCE_CST_MODE) {
			flags |= 0x200;
			if (stat->mode & SCE_STM_R) mctbl.AttrFile |= sceMcFileAttrReadable;
			else mctbl.AttrFile &= (unsigned short)~sceMcFileAttrReadable;

			if (stat->mode & SCE_STM_W) mctbl.AttrFile |= sceMcFileAttrWriteable;
			else mctbl.AttrFile &= (unsigned short)~sceMcFileAttrWriteable;

			if (stat->mode & SCE_STM_X) mctbl.AttrFile |= sceMcFileAttrExecutable;
			else mctbl.AttrFile &= (unsigned short)~sceMcFileAttrExecutable;

			if (stat->mode & SCE_STM_C) mctbl.AttrFile |= sceMcFileAttrDupProhibit;
			else mctbl.AttrFile &= (unsigned short)~sceMcFileAttrDupProhibit;

			if (stat->mode & sceMcFileAttrPS1) mctbl.AttrFile |= sceMcFileAttrPS1;
			else mctbl.AttrFile &= (unsigned short)~sceMcFileAttrPS1;

			if (stat->mode & sceMcFileAttrPDAExec) mctbl.AttrFile |= sceMcFileAttrPDAExec;
			else mctbl.AttrFile &= (unsigned short)~sceMcFileAttrPDAExec;
		}

		if (statmask & SCE_CST_CT) {
			flags |= 0x001;
			mctbl._Create = *((sceMcStDateTime*)&stat->ctime[0]);
		}

		if (statmask & SCE_CST_MT) {
			flags |= 0x002;
			mctbl._Modify = *((sceMcStDateTime*)&stat->mtime[0]);
		}
	
		r = McSetFileInfo(mcman_mc_port, mcman_mc_slot, filename, &mctbl, flags);
	}	
	SignalSema(mcman_io_sema);

	return mcman_ioerrcode(r);
}

//-------------------------------------------------------------- 
int mc_ioctl(iop_file_t *f, int a1, void* a2)
{
	WaitSema(mcman_io_sema);
	return 0;
}

//-------------------------------------------------------------- 

