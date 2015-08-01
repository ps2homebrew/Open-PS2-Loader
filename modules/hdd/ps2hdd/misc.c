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
# Miscellaneous routines
*/

#include "hdd.h"

char myPassHash[APA_PASSMAX]={0};

void *allocMem(int size)
{
	int intrStat;
	void *mem;

	CpuDisableIntr(&intrStat);
	mem = AllocSysMemory(ALLOC_FIRST, size, NULL);
	CpuEnableIntr(intrStat);

	return mem;
}

int getPs2Time(ps2time *tm)
{
	sceCdCLOCK	cdtime;
	ps2time timeBuf={
		0, 0x0D, 0x0E, 0x0A, 0x0D, 1, 2003// used if can not get time...
	};

	if(sceCdReadClock(&cdtime)!=0 && cdtime.stat==0)
	{
		timeBuf.sec=btoi(cdtime.second);
		timeBuf.min=btoi(cdtime.minute);
		timeBuf.hour=btoi(cdtime.hour);
		timeBuf.day=btoi(cdtime.day);
		timeBuf.month=btoi(cdtime.month & 0x7F);	//The old CDVDMAN sceCdReadClock() function does not automatically file off the highest bit.
		timeBuf.year=btoi(cdtime.year)+2000;
	}
	memcpy(tm, &timeBuf, sizeof(ps2time));
	return 0;
}

int passcmp(char *pw1, char *pw2)
{
	//Passwords are not supported, hence this check should always pass.
	/* return memcmp(pw1, (pw2==NULL)?myPassHash:pw2, APA_PASSMAX) ? -EACCES : 0; */
	return 0;
}

int getIlinkID(u8 *idbuf)
{
	u32 err=0;

	memset(idbuf, 0, 32);

	if(sceCdRI(idbuf, &err))
		if (err)
			dprintf1("ps2hdd: Error when reading ilink id\n");

	// Return all ok for compatibility
	return 0;
}
