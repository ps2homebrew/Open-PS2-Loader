/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS superblock manipulation routines
*/

#include <errno.h>
#include <stdio.h>
#include <sysclib.h>
#include <hdd-ioctl.h>

#include "pfs-opt.h"
#include "libpfs.h"

int pfsBlockSize = 1;// block size(in sectors(512) )
u32 pfsMetaSize = 1024; // size of each metadata structure

int pfsCheckZoneSize(u32 zone_size)
{
	if((zone_size & (zone_size - 1)) || (zone_size < (2 * 1024)) || (zone_size > (128 * 1024)))
	{
		PFS_PRINTF(PFS_DRV_NAME": Error: Invalid zone size\n");
		return 0;
	}

	return 1;
}

// Returns the number of sectors (512 byte units) which will be used
// for bitmaps, given the zone size and partition size
u32 pfsGetBitmapSizeSectors(int zoneScale, u32 partSize)
{
	int w, zones = partSize / (1 << zoneScale);

	w = (zones & 7);
	zones = zones / 8 + w;

	w = (zones & 511);
	return zones / 512 + w;
}

// Returns the number of blocks/zones which will be used for bitmaps
u32 pfsGetBitmapSizeBlocks(int scale, u32 mainsize)
{
	u32 a=pfsGetBitmapSizeSectors(scale, mainsize);
	return a / (1<<scale) + ((a % (1<<scale))>0);
}
