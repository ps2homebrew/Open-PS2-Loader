/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#ifndef _PFS_FIO_H
#define _PFS_FIO_H

#define PFS_FDIRO (0x0008) /* internal use for dopen */

///////////////////////////////////////////////////////////////////////////////
//	Function declarations

int pfsFioCheckForLastError(pfs_mount_t *pfsMount, int rv);
int pfsFioCheckFileSlot(pfs_file_slot_t *fileSlot);
pfs_mount_t *pfsFioGetMountedUnit(int unit);
void pfsFioCloseFileSlot(pfs_file_slot_t *fileSlot);

///////////////////////////////////////////////////////////////////////////////
//	I/O functions

int pfsFioInit(iop_device_t *f);
int pfsFioDeinit(iop_device_t *f);
int pfsFioFormat(iop_file_t *, const char *dev, const char *blockdev, void *arg, int arglen);
int pfsFioOpen(iop_file_t *f, const char *name, int flags, int mode);
int pfsFioClose(iop_file_t *f);
int pfsFioRead(iop_file_t *f, void *buf, int size);
int pfsFioWrite(iop_file_t *f, void *buf, int size);
int pfsFioLseek(iop_file_t *f, int pos, int whence);
int pfsFioRemove(iop_file_t *f, const char *name);
int pfsFioMkdir(iop_file_t *f, const char *path, int mode);
int pfsFioRmdir(iop_file_t *f, const char *path);
int pfsFioDopen(iop_file_t *f, const char *name);
int pfsFioDclose(iop_file_t *f);
int pfsFioDread(iop_file_t *f, iox_dirent_t *buf);
int pfsFioGetstat(iop_file_t *f, const char *name, iox_stat_t *stat);
int pfsFioChstat(iop_file_t *f, const char *name, iox_stat_t *stat, unsigned int statmask);
int pfsFioRename(iop_file_t *f, const char *old, const char *new);
int pfsFioChdir(iop_file_t *f, const char *name);
int pfsFioSync(iop_file_t *f, const char *dev, int flag);
int pfsFioMount(iop_file_t *f, const char *fsname, const char *devname, int flag, void *arg, int arglen);
int pfsFioUmount(iop_file_t *f, const char *fsname);
s64 pfsFioLseek64(iop_file_t *f, s64 offset, int whence);
int pfsFioSymlink(iop_file_t *f, const char *old, const char *new);
int pfsFioReadlink(iop_file_t *f, const char *path, char *buf, int buflen);

int pfsFioUnsupported(void);

#endif /* _PFS_FIO_H */
