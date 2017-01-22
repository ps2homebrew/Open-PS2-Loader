/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# PFS startup and misc code
*/

#include <stdio.h>
#include <sysclib.h>
#include <errno.h>
#include <iomanX.h>
#include <thsemap.h>
#include <poweroff.h>
#include <irx.h>
#include <loadcore.h>

#include "pfs-opt.h"
#include "libpfs.h"
#include "pfs.h"
#include "pfs_fio.h"
#include "pfs_fioctl.h"

IRX_ID("pfs_driver", PFS_MAJOR, PFS_MINOR);

///////////////////////////////////////////////////////////////////////////////
//	Globals

iop_device_ops_t pfsOps = {
	pfsFioInit,
	pfsFioDeinit,
	pfsFioFormat,
	pfsFioOpen,
	pfsFioClose,
	pfsFioRead,
	pfsFioWrite,
	pfsFioLseek,
	pfsFioIoctl,
	pfsFioRemove,
	pfsFioMkdir,
	pfsFioRmdir,
	pfsFioDopen,
	pfsFioClose,
	pfsFioDread,
	pfsFioGetstat,
	pfsFioChstat,
	pfsFioRename,
	pfsFioChdir,
	pfsFioSync,
	pfsFioMount,
	pfsFioUmount,
	(void*)pfsFioUnsupported /*pfsFioLseek64*/,
	pfsFioDevctl,
	(void*)pfsFioUnsupported /*pfsFioSymlink*/,
	(void*)pfsFioUnsupported /*pfsFioReadlink*/,
	pfsFioIoctl2
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

extern u32 pfsMetaSize;
extern pfs_file_slot_t *pfsFileSlots;
extern pfs_config_t pfsConfig;

///////////////////////////////////////////////////////////////////////////////
//   Local function declerations

static int printPfsArgError(void);
static int allocateMountBuffer(int size);

///////////////////////////////////////////////////////////////////////////////
//   Function defenitions

static int printPfsArgError(void)
{
	PFS_PRINTF(PFS_DRV_NAME" ERROR: Usage: %s [-m <maxmount>] [-o <maxopen>] [-n <numbuffer>]\n", pfsFilename);

	return MODULE_NO_RESIDENT_END;
}

static int allocateMountBuffer(int size)
{
	int tsize = size * sizeof(pfs_mount_t);

	pfsMountBuf = pfsAllocMem(tsize);
	if(!pfsMountBuf)
		return -ENOMEM;

	memset(pfsMountBuf, 0, tsize);

	return 0;
}

void pfsClearMount(pfs_mount_t *pfsMount)
{
	memset(pfsMount, 0, sizeof(pfs_mount_t));
}

pfs_mount_t *pfsGetMountedUnit(s32 unit)
{	// get mounted unit
	if(unit>=pfsConfig.maxMount)
		return NULL;

	if(!(pfsMountBuf[unit].flags & PFS_MOUNT_BUSY))
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

	PFS_PRINTF(PFS_DRV_NAME" Playstation Filesystem Driver v%d.%d\nps2fs: (c) 2003 Sjeep, Vector and Florin Sasu\n", PFS_MAJOR, PFS_MINOR);

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

			number = strtol(argv[0], NULL, 10);

			if(number <= 32)
				pfsConfig.maxOpen = number;
		}
		else if(!strcmp(argv[0], "-n"))
		{
			if(--argc <= 0)
				return printPfsArgError();
			argv++;

			number = strtol(argv[0], NULL, 10);

			if(number > numBuf)
				numBuf = number;

			if(numBuf > 127) {
				PFS_PRINTF(PFS_DRV_NAME" ERROR: Number of buffers is larger than 127!\n");
				return -EINVAL;
			}
		}
		else
			return printPfsArgError();

		argc--;
		argv++;
	}

	PFS_PRINTF(PFS_DRV_NAME" Max mount: %ld, Max open: %ld, Number of buffers: %d\n", pfsConfig.maxMount,
			pfsConfig.maxOpen, numBuf);

	// Do we have enough buffers ?
	reqBuf = (pfsConfig.maxOpen * 2) + 8;
	if(numBuf < reqBuf)
		PFS_PRINTF(PFS_DRV_NAME" Warning: %d buffers may be needed, but only %d buffers are allocated\n", reqBuf, numBuf);

	if(allocateMountBuffer(pfsConfig.maxMount) < 0)
		return MODULE_NO_RESIDENT_END;

	// Allocate and zero memory for file slots
	size = pfsConfig.maxOpen * sizeof(pfs_file_slot_t);
	pfsFileSlots = pfsAllocMem(size);
	if(!pfsFileSlots) {
		PFS_PRINTF(PFS_DRV_NAME" Error: Failed to allocate memory!\n");
		return MODULE_NO_RESIDENT_END;
	}

	memset(pfsFileSlots, 0, size);

	if(pfsCacheInit(numBuf, pfsMetaSize) < 0)
		return MODULE_NO_RESIDENT_END;

	DelDrv("pfs");
	AddDrv(&pfsFioDev);

	PFS_PRINTF(PFS_DRV_NAME" Driver start.\n");

	return MODULE_RESIDENT_END;
}
