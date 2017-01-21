/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#ifndef _HDD_FIO_H
#define _HDD_FIO_H

// I/O functions
int hddInit(iop_device_t *f);
int hddDeinit(iop_device_t *f);
int hddFormat(iop_file_t *f, const char *dev, const char *blockdev, void *arg, int arglen);
int hddOpen(iop_file_t *f, const char *name, int flags, int mode);
int hddClose(iop_file_t *f);
int hddRead(iop_file_t *f, void *buf, int size);
int hddWrite(iop_file_t *f, void *buf, int size);
int hddLseek(iop_file_t *f, int post, int whence);
int hddIoctl2(iop_file_t *f, int request, void *argp, unsigned int arglen, void *bufp, unsigned intbuflen);
int hddRemove(iop_file_t *f, const char *name);
int hddDopen(iop_file_t *f, const char *name);
int hddDread(iop_file_t *f, iox_dirent_t *dirent);
int hddGetStat(iop_file_t *f, const char *name, iox_stat_t *stat);
int hddReName(iop_file_t *f, const char *oldname, const char *newname);
int hddDevctl(iop_file_t *f, const char *devname, int cmd, void *arg, unsigned int arglen, void *bufp, unsigned int buflen);

int hddUnsupported(iop_file_t *f);

#endif /* _HDD_FIO_H */
