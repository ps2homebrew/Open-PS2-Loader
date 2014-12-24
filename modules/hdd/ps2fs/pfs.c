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
# PFS startup and misc code
*/

#include "pfs.h"

IRX_ID("pfs_driver", 1, 1);

///////////////////////////////////////////////////////////////////////////////
//   Globals

iop_device_ops_t pfsOps = {
	pfsInit,
	pfsDeinit,
	pfsFormat,
	pfsOpen,
	pfsClose,
	pfsRead,
	pfsWrite,
	pfsLseek,
	pfsIoctl,
	pfsRemove,
	pfsMkdir,
	pfsRmdir,
	pfsDopen,
	pfsClose,
	pfsDread,
	pfsGetstat,
	pfsChstat,
	pfsRename,
	pfsChdir,
	pfsSync,
	pfsMount,
	pfsUmount,
	(void*)fioUnsupported /*pfsLseek64*/,
	pfsDevctl,
	fioUnsupported /*pfsSymlink*/,
	fioUnsupported /*pfsReadlink*/,
	pfsIoctl2
};

iop_device_t pfsFioDev = {
	"pfs",
	(IOP_DT_FS | IOP_DT_FSEXT),
	1,
	"PFS",
	&pfsOps
};

pfs_config_t pfsConfig = { 1, 2 };
pfs_mount_t *pfsMountBuf;
char *pfsFilename = NULL;
u32 metaSize = 1024; // size of each metadata structure
int pfsDebug = 0;

///////////////////////////////////////////////////////////////////////////////
//   Local function declerations

void printInfo();
int printPfsArgError();
int allocateMountBuffer(int size);
int allocateBuffers(u32 numBuf, u32 bufSize);

///////////////////////////////////////////////////////////////////////////////
//   Function defenitions

void printInfo()
{
	printf("ps2fs: Playstation Filesystem Driver v%d.%d\nps2fs: (c) 2003 Sjeep, Vector and Florin Sasu\n", PFS_MAJOR, PFS_MINOR);
}

int printPfsArgError()
{
	printf("ps2fs: ERROR: Usage: %s [-m <maxmount>] [-o <maxopen>] [-n <numbuffer>]\n", pfsFilename);

	return MODULE_NO_RESIDENT_END;
}

int allocateMountBuffer(int size)
{
	int tsize = size * sizeof(pfs_mount_t);

	pfsMountBuf = allocMem(tsize);
	if(!pfsMountBuf)
		return -ENOMEM;

	memset(pfsMountBuf, 0, tsize);

	return 0;
}


void clearMount(pfs_mount_t *pfsMount)
{
	memset(pfsMount, 0, sizeof(pfs_mount_t));
}

pfs_mount_t * getMountedUnit(u32 unit)
{	// get mounted unit
	if(unit>=pfsConfig.maxMount)
		return NULL;

	if(!(pfsMountBuf[unit].flags & MOUNT_BUSY))
		return NULL;

	return &pfsMountBuf[unit];
}

int _start(int argc, char *argv[])
{
	char *filename;
	int number;
	int numBuf = 8;
	int reqBuf;
	int size;

	printInfo();

	// Get filename of IRX
	filename = strrchr(argv[0], '/');
	if(filename++)
		pfsFilename = filename;
	else
		pfsFilename = argv[0];

	argc--;
	argv++;

	// Parse arguments
	while(argc > 0)
	{
		if(argv[0][0] != '-')
			break;

		if(!strcmp(argv[0], "-m"))
		{
			if(--argc <= 0)
				return printPfsArgError();
			argv++;

			number = strtol(argv[0], 0, 10);

			if(number <= 32)
				pfsConfig.maxMount = number;
		}
		else if(!strcmp(argv[0], "-o"))
		{
			if(--argc <= 0)
				return printPfsArgError();
			argv++;

			number = strtol(argv[0], 0, 10);

			if(number <= 32)
				pfsConfig.maxOpen = number;
		}
		else if(!strcmp(argv[0], "-n"))
		{
			if(--argc <= 0)
				return printPfsArgError();
			argv++;

			number = strtol(argv[0], 0, 10);

			if(number > numBuf)
				numBuf = number;

			if(numBuf > 127) {
				printf("ps2fs: ERROR: Number of buffers is larger than 127!\n");
				return -EINVAL;
			}
		}
		else if(!strcmp(argv[0], "-debug"))
			pfsDebug = 1;
		else
			return printPfsArgError();

		argc--;
		argv++;
	}

	printf("ps2fs: Max mount: %ld, Max open: %ld, Number of buffers: %d\n", pfsConfig.maxMount,
			pfsConfig.maxOpen, numBuf);

	// Do we have enough buffers ?
	reqBuf = (pfsConfig.maxOpen * 2) + 8;
	if(numBuf < reqBuf)
		printf("ps2fs: Warning: %d buffers may be needed, but only %d buffers are allocated\n", reqBuf, numBuf);

	if(allocateMountBuffer(pfsConfig.maxMount) < 0)
		return MODULE_NO_RESIDENT_END;

	// Allocate and zero memory for file slots
	size = pfsConfig.maxOpen * sizeof(pfs_file_slot_t);
	fileSlots = allocMem(size);
	if(!fileSlots) {
		printf("ps2fs: Error: Failed to allocate memory!\n");
		return MODULE_NO_RESIDENT_END;
	}

	memset(fileSlots, 0, size);

	if(cacheInit(numBuf, metaSize) < 0)
		return MODULE_NO_RESIDENT_END;

	DelDrv("pfs");
	AddDrv(&pfsFioDev);

	printf("ps2fs: Driver start.\n");

	return MODULE_RESIDENT_END;
}
