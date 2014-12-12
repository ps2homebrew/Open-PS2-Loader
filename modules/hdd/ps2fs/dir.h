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

#ifndef _DIR_H
#define _DIR_H

pfs_cache_t *getDentry(pfs_cache_t *clink, char *path, pfs_dentry **dentry, u32 *size, int option);
pfs_cache_t *getDentriesChunk(pfs_blockpos_t *position, int *result);
int getNextDentry(pfs_cache_t *clink, pfs_blockpos_t *blockpos, u32 *position, char *name, pfs_blockinfo *bi);
pfs_cache_t *getDentriesAtPos(pfs_cache_t *clink, u64 position, int *offset, int *result);
pfs_cache_t *fillInDentry(pfs_cache_t *clink, pfs_dentry *dentry, char *path1, pfs_blockinfo *bi, u32 len, u16 mode);
pfs_cache_t *dirAddEntry(pfs_cache_t *dir, char *filename, pfs_blockinfo *bi, u16 mode, int *result);
pfs_cache_t *dirRemoveEntry(pfs_cache_t *clink, char *path);
int checkDirForFiles(pfs_cache_t *clink);
void fillSelfAndParentDentries(pfs_cache_t *clink, pfs_blockinfo *self, pfs_blockinfo *parent);
pfs_cache_t* setParent(pfs_cache_t *clink, pfs_blockinfo *bi, int *result);

#endif /* _DIR_H */
