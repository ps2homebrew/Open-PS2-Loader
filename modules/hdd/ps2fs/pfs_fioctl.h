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

#ifndef _PFS_FIOCTL_H
#define _PFS_FIOCTL_H

///////////////////////////////////////////////////////////////////////////////
//   Command macros

// PFS IOCTL2 commands
#define PFS_IOCTL2_MALLOC			0x7001
#define PFS_IOCTL2_FREE				0x7002
#define PFS_IOCTL2_ATTR_ADD			0x7003
#define PFS_IOCTL2_ATTR_DEL			0x7004
#define PFS_IOCTL2_ATTR_LOOKUP		0x7005
#define PFS_IOCTL2_ATTR_READ		0x7006
#define PFS_IOCTL2_GET_INODE		0x7007

// PFS DEVCTL commands
#define PFS_DEVCTL_GET_ZONE_SIZE	0x5001
#define PFS_DEVCTL_GET_ZONE_FREE	0x5002
#define PFS_DEVCTL_CLOSE_ALL		0x5003
#define PFS_DEVCTL_GET_STAT			0x5004
#define PFS_DEVCTL_CLEAR_STAT		0x5005

#define PFS_DEVCTL_SET_UID			0x5032
#define PFS_DEVCTL_SET_GID			0x5033

#define PFS_DEVCTL_SHOW_BITMAP		0xFF

///////////////////////////////////////////////////////////////////////////////
//   Function declerations

int	pfsIoctl(iop_file_t *f, unsigned long arg, void *param);
int pfsIoctl2(iop_file_t *f, int cmd, void *arg, size_t arglen, void *buf, size_t buflen);
int pfsDevctl(iop_file_t *f, const char *name, int cmd, void *arg, size_t arglen, void *buf, size_t buflen);

void devctlCloseAll();
int devctlFsckStat(pfs_mount_t *pfsMount, int mode);

int ioctl2Attr(pfs_cache_t *clink, int cmd, void *arg, void *outbuf, u32 *offset);
pfs_aentry_t *getAentry(pfs_cache_t *clink, char *key, char *value, int mode);
int ioctl2AttrAdd(pfs_cache_t *clink, pfs_ioctl2attr_t *attr);
int ioctl2AttrDelete(pfs_cache_t *clink, void *arg);
int ioctl2AttrLoopUp(pfs_cache_t *clink, char *key, char *value);
int ioctl2AttrRead(pfs_cache_t *clink, pfs_ioctl2attr_t *attr, u32 *unkbuf);
int ioctl2_0x7032(pfs_cache_t *clink);

int ioctl2Alloc(pfs_cache_t *clink, int msize, int mode);
void ioctl2Free(pfs_cache_t *pfree);



#endif /* _PFS_FIOCTL_H */
