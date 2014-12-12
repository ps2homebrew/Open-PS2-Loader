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

#ifndef _JOURNAL_H
#define _JOURNAL_H

int journalCheckSum(void *header);
void journalWrite(pfs_mount_t *pfsMount, pfs_cache_t *clink, u32 numBuffers);
int journalReset(pfs_mount_t *pfsMount);
int journalFlush(pfs_mount_t *pfsMount);
int journalResetore(pfs_mount_t *pfsMount);
int journalResetThis(block_device *blockDev, int fd, u32 sector);

#endif /* _JOURNAL_H */

