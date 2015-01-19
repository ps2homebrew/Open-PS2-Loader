/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
*/

#ifndef _PFS_FIO_H
#define _PFS_FIO_H

#define	FDIRO		(0x0008)  /* internal use for dopen */

///////////////////////////////////////////////////////////////////////////////
//   Function declerations

int checkForLastError(pfs_mount_t *pfsMount, int rv);
int checkFileSlot(pfs_file_slot_t *fileSlot);
void fioStatFiller(pfs_cache_t *clink, iox_stat_t *stat);
int mountDevice(block_device *blockDev, int fd, int unit, int flag);
void _sync();
void inodeFill(pfs_cache_t *ci, pfs_blockinfo *bi, u16 mode, u16 uid, u16 gid);
pfs_mount_t *fioGetMountedUnit(int unit);
void closeFileSlot(pfs_file_slot_t *fileSlot);
pfs_cache_t *createNewFile(pfs_cache_t *clink, u16 mode, u16 uid, u16 gid, int *result);
void bitmapFreeBlockSegment(pfs_mount_t *pfsMount, pfs_blockinfo *bi);
int searchFreeZone(pfs_mount_t *pfsMount, pfs_blockinfo *bi, u32 max_count);
u32 syncBlockPos(pfs_blockpos_t *blockpos, u64 position);
pfs_blockinfo* getCurrentBlock(pfs_blockpos_t *blockpos);

///////////////////////////////////////////////////////////////////////////////
//   Functions which hook into IOMAN

int	pfsInit(iop_device_t *f);
int	pfsDeinit(iop_device_t *f);
int	pfsFormat(iop_file_t *, const char *dev, const char *blockdev, void *arg, size_t arglen);
int	pfsOpen(iop_file_t *f, const char *name, int flags, int mode);
int	pfsClose(iop_file_t *f);
int	pfsRead(iop_file_t *f, void *buf, int size);
int	pfsWrite(iop_file_t *f, void *buf, int size);
int	pfsLseek(iop_file_t *f, unsigned long pos, int whence);
int	pfsRemove(iop_file_t *f, const char *name);
int	pfsMkdir(iop_file_t *f, const char *path, int mode);
int	pfsRmdir(iop_file_t *f, const char *path);
int	pfsDopen(iop_file_t *f, const char *name);
int	pfsDclose(iop_file_t *f);
int	pfsDread(iop_file_t *f, iox_dirent_t *buf);
int	pfsGetstat(iop_file_t *f, const char *name, iox_stat_t *stat);
int	pfsChstat(iop_file_t *f, const char *name, iox_stat_t *stat, unsigned int statmask);
int pfsRename(iop_file_t *f, const char *old, const char *new);
int pfsChdir(iop_file_t *f, const char *name);
int pfsSync(iop_file_t *f, const char *dev, int flag);
int pfsMount(iop_file_t *f, const char *fsname, const char *devname, int flag, void *arg, size_t arglen);
int pfsUmount(iop_file_t *f, const char *fsname);
s64 pfsLseek64(iop_file_t *f, s64 offset, int whence);
int pfsSymlink(iop_file_t *f, const char *old, const char *new);
int pfsReadlink(iop_file_t *f, const char *path, char *buf, int buflen);

int fioUnsupported();

#endif /* _PFS_FIO_H */
