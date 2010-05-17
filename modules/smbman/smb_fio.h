/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#ifndef __SMB_FIO_H__
#define __SMB_FIO_H__

#include <ioman.h>
#include "ioman_add.h"

// smb driver ops functions prototypes
int smb_initdev(void);
int smb_dummy(void);
int smb_init(iop_device_t *iop_dev);
int smb_deinit(iop_device_t *dev);
int smb_open(iop_file_t *f, const char *filename, int mode, int flags);
int smb_close(iop_file_t *f);
int smb_lseek(iop_file_t *f, u32 pos, int where);
int smb_read(iop_file_t *f, void *buf, int size);
int smb_write(iop_file_t *f, void *buf, int size);
int smb_remove(iop_file_t *f, const char *filename);
int smb_mkdir(iop_file_t *f, const char *dirname);
int smb_rmdir(iop_file_t *f, const char *dirname);
int smb_dopen(iop_file_t *f, const char *dirname);
int smb_dclose(iop_file_t *f);
int smb_dread(iop_file_t *f, iox_dirent_t *dirent);
int smb_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat);
int smb_rename(iop_file_t *f, const char *oldname, const char *newname);
int smb_chdir(iop_file_t *f, const char *dirname);
s64 smb_lseek64(iop_file_t *f, s64 pos, int where);
int smb_devctl(iop_file_t *f, const char *devname, int cmd, void *arg, u32 arglen, void *bufp, u32 buflen);

#endif
