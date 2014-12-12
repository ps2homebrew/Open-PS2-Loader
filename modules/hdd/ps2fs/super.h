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

#ifndef _SUPER_H
#define _SUPER_H

#define PFS_SUPER_SECTOR			8192
#define PFS_SUPER_BACKUP_SECTOR		8193

int checkZoneSize(u32 zone_size);
u32 getBitmapSectors(int zoneScale, u32 partSize);
u32 getBitmapBocks(int scale, u32 mainsize);
int formatSub(block_device *blockDev, int fd, u32 sub, u32 reserved, u32 scale, u32 fragment);
int _format(block_device *blockDev, int fd, int zonesize, int fragment);
int updateSuperBlock(pfs_mount_t *pfsMount, pfs_super_block *superblock, u32 sub);
int mountSuperBlock(pfs_mount_t *pfsMount);

#endif /* _SUPER_H */
