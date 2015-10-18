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
# Main APA Header file
*/

#ifndef _HDD_H
#define _HDD_H

typedef struct
{
	u32 totalLBA;
	u32 partitionMaxSize;
	u32 format;
	u32 status;
} hdd_device_t;

typedef struct
{
	iop_file_t	*f;				// used to see if open...
	u32			post;			// offset/post....
	u16			nsub;
	u16			type;
	char		id[APA_IDMAX];
	apa_sub_t parts[APA_MAXSUB+1];	// Partition data (0 = main partition, 1+ = sub-partition)
} hdd_file_slot_t;

///////////////////////////////////////////////////////////////////////////////
//	Function declerations
int hddCheckPartitionMax(s32 device, s32 size);
apa_cache_t *hddAddPartitionHere(s32 device, apa_params_t *params, u32 *EmptyBlocks, u32 sector, int *err);
int hddGetFreeSectors(s32 device, u32 *free, hdd_device_t *deviceinfo);

#endif
