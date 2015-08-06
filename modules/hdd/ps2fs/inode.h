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

#ifndef _INODE_H
#define _INODE_H

int inodeCheckSum(pfs_inode *inode);
void inodePrint(pfs_inode *inode);
void inodeUpdateTime(pfs_cache_t *clink);
pfs_cache_t *inodeGetData(pfs_mount_t *pfsMount, u16 sub, u32 inode, int *result);
pfs_cache_t *inodeGetFileInDir(pfs_cache_t *dirInode, char *path, int *result);
int inodeSync(pfs_blockpos_t *blockpos, u64 size, u32 used_segments);
pfs_cache_t *inodeGetFile(pfs_mount_t *pfsMount, pfs_cache_t *clink, const char *name, int *result);
int inodeRemove(pfs_cache_t *parent, pfs_cache_t *inode, char *path);
pfs_cache_t *inodeGetParent(pfs_mount_t *pfsMount, pfs_cache_t *clink, const char *filename,
							char *path, int *result);
pfs_cache_t *inodeCreateNewFile(pfs_cache_t *clink, u16 mode, u16 uid, u16 gid, int *result);

#endif /* _INODE_H */
