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
# Start Up routines
*/

#include "hdd.h"

IRX_ID("hdd_driver", 1, 1);

iop_device_ops_t hddOps={
	hddInit,
	hddDeinit,
	hddFormat,
	hddOpen,
	hddClose,
	hddRead,
	hddWrite,
	hddLseek,
	(void*)fioUnsupported,
	hddRemove,
	(void*)fioUnsupported,
	(void*)fioUnsupported,
	hddDopen,
	hddClose,
	hddDread,
	hddGetStat,
	(void*)fioUnsupported,
	hddReName,
	(void*)fioUnsupported,
	(void*)fioUnsupported,
	(void*)fioUnsupported,
	(void*)fioUnsupported,
	(void*)fioUnsupported,
	hddDevctl,
	(void*)fioUnsupported,
	(void*)fioUnsupported,
	hddIoctl2,
};
iop_device_t hddFioDev={
	"hdd",
	IOP_DT_BLOCK | IOP_DT_FSEXT,
	1,
	"HDD",
	(struct _iop_device_ops *)&hddOps,
};

u32 maxOpen=1;
hdd_device_t hddDeviceBuf[2]={
	{0, 0, 0, 3},
	{0, 0, 0, 3}
};

char mbrMagic[0x20]={
	0x2B, 0x17, 0x16, 0x01, 0x58, 0x3B, 0x17, 0x15,
	0x08, 0x0D, 0x0C, 0x1D, 0x0A, 0x58, 0x3D, 0x16,
	0x0C, 0x1D, 0x0A, 0x0C, 0x19, 0x11, 0x16, 0x15,
	0x1D, 0x16, 0x0C, 0x58, 0x31, 0x16, 0x1B, 0x56
};



int inputError(char *input)
{
	printf("ps2hdd: Error: Usage: %s [-o <maxOpen>] [-n <maxcache>]\n", input);
	return 1;
}

void printStartup(void)
{
	printf("ps2hdd: PS2 APA Driver v1.1 (c) 2003 Vector\n");
	return;
}

int unlockDrive(u32 device)
{
	u8 id[32];
	int rv;
	if((rv=getIlinkID(id))==0)
		return ata_device_sce_sec_unlock(device, id);
	return rv;
}

int _start(int argc, char **argv)
{
	int 	i;
	char	*input;
	int		cacheSize=3;
	ps2time tm;
	ata_devinfo_t *hddInfo;

	printStartup();
	// decode MBR Magic
	for(i=0;i!=0x20;i++)
		mbrMagic[i]^='x';

	if((input=strrchr(argv[0], '/')))
		input++;
	else
		input=argv[0];

	argc--; argv++;
	while(argc)
	{
		if(argv[0][0] != '-')
			break;
		if(strcmp("-o", argv[0])==0){
			argc--; argv++;
			if(!argc)
				return inputError(input);
			i=strtol(argv[0], 0, 10);
			if(i-1<32)
				maxOpen=i;
		}
		else if(strcmp("-n", argv[0])==0){
			argc--; argv++;
			if(!argc)
				return inputError(input);
			i=strtol(*argv, 0, 10);
			if(cacheSize<i)
				cacheSize=i;
		}
		argc--; argv++;
	}

	printf("ps2hdd: max open = %ld, %d buffers\n", maxOpen, cacheSize);
	getPs2Time(&tm);
	printf("ps2hdd: %02d:%02d:%02d %02d/%02d/%d\n",
		tm.hour, tm.min, tm.sec, tm.month, tm.day, tm.year);
	for(i=0;i < 2;i++)
	{
		if(!(hddInfo=ata_get_devinfo(i))){
			printf("ps2hdd: Error: ata initialization failed.\n");
			return 0;
		}
		if(hddInfo->exists!=0 && hddInfo->has_packet==0){
				hddDeviceBuf[i].status--;
				hddDeviceBuf[i].totalLBA=hddInfo->total_sectors;
				hddDeviceBuf[i].partitionMaxSize=apaGetPartitionMax(hddInfo->total_sectors);
				if(unlockDrive(i)==0)
					hddDeviceBuf[i].status--;
				printf("ps2hdd: disk%d: 0x%08lx sectors, max 0x%08lx\n", i,
					hddDeviceBuf[i].totalLBA, hddDeviceBuf[i].partitionMaxSize);
		}
	}
	fileSlots=allocMem(maxOpen*sizeof(hdd_file_slot_t));
	if(fileSlots)
		memset(fileSlots, 0, maxOpen*sizeof(hdd_file_slot_t));

	cacheInit(cacheSize);
	for(i=0;i < 2;i++)
	{
		if(hddDeviceBuf[i].status<2){
			if(journalResetore(i)!=0)
				return 1;
			if(apaGetFormat(i, (int *)&hddDeviceBuf[i].format))
				hddDeviceBuf[i].status--;
			printf("ps2hdd: drive status %ld, format version %08lx\n",
				hddDeviceBuf[i].status, hddDeviceBuf[i].format);
		}
	}
	DelDrv("hdd");
	if(AddDrv(&hddFioDev)==0)
	{
		printf("ps2hdd: driver start.\n");
		return 0;
	}
	return 1;
}
