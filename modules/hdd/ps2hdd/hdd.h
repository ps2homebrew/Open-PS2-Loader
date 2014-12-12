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

#include <types.h>
#include <defs.h>
#include <irx.h>
#include "atad.h"
#include <dev9.h>
#include <loadcore.h>
#include <poweroff.h>
#include <sysmem.h>
#include <stdio.h>
#include <sysclib.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <iomanX.h>
#include <thbase.h>
#include <thsemap.h>
#include <intrman.h>
#include <cdvdman.h>

#define DEBUG_LEVEL1
//#define DEBUG_LEVEL2

#ifdef DEBUG_LEVEL1
#define dprintf1 printf
#else
#define dprintf1 printf(format, args...)
#endif

#ifdef DEBUG_LEVEL2
#define dprintf2 printf
#else
#define dprintf2(format, args...)
#endif

typedef struct
{
	u32 totalLBA;
	u32 partitionMaxSize;
	u32 format;
	u32 status;
} hdd_device_t;

// modes for cacheGetHeader
#define THEADER_MODE_READ	0x00
#define THEADER_MODE_WRITE	0x01


typedef struct {
	u8	unused;
	u8	sec;
	u8	min;
	u8	hour;
	u8	day;
	u8	month;
	u16	year;
} ps2time;

//
// MAIN APA defines/struct
//
typedef struct {	// for a hack :P
	u32 start;		// Sector address
	u32 length;		// Sector count
} apa_subs;

// Sectors for this and that ;)
#define APA_SECTOR_MBR				0
#define APA_SECTOR_SECTOR_ERROR		6// use for last sector that had a error...
#define APA_SECTOR_PART_ERROR		7// use for last partition that had a error...
#define APA_SECTOR_APAL				8
#define APA_SECTOR_APAL_HEADERS		10	// 10-262

// APA Partition
#define APA_MAGIC			0x00415041	// 'APA\0'
#define APA_IDMAX			32
#define APA_MAXSUB			64			// Maximum # of sub-partitions
#define APA_PASSMAX			8
#define APA_FLAG_SUB		0x0001
#define APA_MBR_VERSION		2
//   format/types
#define APA_TYPE_FREE		0x0000
#define APA_TYPE_MBR		0x0001		// Master Boot Record
#define APA_TYPE_EXT2SWAP	0x0082
#define APA_TYPE_EXT2		0x0083
#define APA_TYPE_REISER		0x0088
#define APA_TYPE_PFS		0x0100
#define APA_TYPE_CFS		0x0101

#define APA_MODVER			0x0201

typedef struct
{
	u32		checksum;
	u32		magic;				// APA_MAGIC
	u32		next;
	u32 	prev;
	char	id[APA_IDMAX];
	char	rpwd[APA_PASSMAX];
	char	fpwd[APA_PASSMAX];
	u32		start;
	u32		length;
	u16		type;
	u16		flags;
	u32		nsub;
	ps2time	created;
	u32		main;
	u32		number;
	u32		modver;
	u32		pading1[7];
	char	pading2[128];
	struct {
		char 	magic[32];
		u32 	version;
		u32		nsector;
		ps2time	created;
		u32		osdStart;
		u32		osdSize;
		char	pading3[200];
	} mbr;
	struct {
		u32 start;
		u32 length;
	} subs[APA_MAXSUB];
} apa_header;


typedef struct
{
	char	id[APA_IDMAX];
	u32		size;
	u16		type;
	u16		flags;
	u32		main;
	u32		number;
} input_param;

typedef struct
{
	iop_file_t	*f;				// used to see if open...
	u32			post;			// offset/post....
	u16			nsub;
	u16			type;
	char		id[APA_IDMAX];
	struct {					// Partition data (0 = main partition, 1+ = sub-partition)
		u32		start;			// Sector address
		u32		length;			// Sector count
	} parts[APA_MAXSUB+1];
} hdd_file_slot_t;


///////////////////////////////////////////////////////////////////////////////
//   Externs
extern hdd_device_t hddDeviceBuf[2];
extern u32 maxOpen;
extern hdd_file_slot_t	*fileSlots;
extern char mbrMagic[0x20];
extern char myPassHash[APA_PASSMAX];
///////////////////////////////////////////////////////////////////////////////
//   Function declerations
int inputError(char *input);
int unlockDrive(u32 device);

#include "cache.h"
#include "apa.h"
#include "misc.h"
#include "journal.h"
#include "hdd_fio.h"
#endif
