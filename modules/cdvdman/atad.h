/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: atad.h 629 2004-10-11 00:45:00Z mrbrown $
# ATA Device Driver definitions and imports.
*/

#ifndef IOP_ATAD_H
#define IOP_ATAD_H

#include "types.h"
#include "irx.h"

/* These are used with the dir parameter of ata_device_dma_transfer().  */
#define ATA_DIR_READ	0
#define ATA_DIR_WRITE	1

typedef struct _ata_devinfo {
	int	exists;		/* Was successfully probed.  */
	int	has_packet;	/* Supports the PACKET command set.  */
	u32	total_sectors;	/* Total number of user sectors.  */
	u32	security_status;/* Word 0x100 of the identify info.  */
} ata_devinfo_t;

int atad_start(void);
ata_devinfo_t * ata_get_devinfo(int device);
int ata_io_start(void *buf, u32 blkcount, u16 feature, u16 nsector, u16 sector,
		u16 lcyl, u16 hcyl, u16 select, u16 command);
int ata_io_finish(void);
int ata_get_error(void);
int ata_device_dma_transfer(int device, void *buf, u32 lba, u32 nsectors, int dir);

// APA Partition
#define APA_MAGIC			0x00415041	// 'APA\0'
#define APA_IDMAX			32
#define APA_MAXSUB			64			// Maximum # of sub-partitions
#define APA_PASSMAX			8
#define APA_FLAG_SUB		0x0001
#define APA_MBR_VERSION		2

typedef struct {
	u8	unused;
	u8	sec;
	u8	min;
	u8	hour;
	u8	day;
	u8	month;
	u16	year;
} ps2time;
	
typedef struct					// size = 1024
{
	u32		checksum;			// HDL uses 0xdeadfeed magic here
	u32		magic;				// APA_MAGIC
	u32		next;
	u32 	prev;
	char	id[APA_IDMAX];		// 16
	char	rpwd[APA_PASSMAX];	// 48
	char	fpwd[APA_PASSMAX];  // 56
	u32		start;				// 64
	u32		length;				// 68
	u16		type;				// 72
	u16		flags;				// 74
	u32		nsub;				// 76
	ps2time	created;			// 80
	u32		main;				// 88
	u32		number;				// 92
	u32		modver;				// 96
	u32		pading1[7];			// 100
	struct {					// 128
		u8  	pad[104];
		u32 	layer1_start;
		u32 	discType;
		int 	num_partitions;
		struct {
			u32 	part_offset; 	// in MB
			u32 	data_start; 	// in sectors
			u32 	part_size; 		// in KB				
		} part_specs[65];
	} game_specs;
} apa_header;

#endif /* IOP_ATAD_H */
