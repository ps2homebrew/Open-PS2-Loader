#ifndef _APA_OPT_H
#define _APA_OPT_H

#define APA_PRINTF(format, ...) printf(format, ##__VA_ARGS__)
#define APA_DRV_NAME "hdd"

/*	Define (in your Makefile!) to build an OSD version, which will:
	1. When formatting, do not create any partitions other than __mbr.
	2. __mbr will be formatted with its password.
	3. All partitions can be accessed, even without the right password.
	4. The starting LBA of the partition will be returned in
		the private_5 field of the stat structure (returned by getstat and dread). */
//#define APA_OSD_VER	1

#ifdef APA_OSD_VER
#define APA_STAT_RETURN_PART_LBA 1
#define APA_FORMAT_LOCK_MBR 1
#else
#define APA_ENABLE_PASSWORDS 1
#define APA_FORMAT_MAKE_PARTITIONS 1
#endif

#endif
