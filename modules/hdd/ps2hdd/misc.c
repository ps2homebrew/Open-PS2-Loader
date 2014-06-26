/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: misc.c 1370 2007-01-17 02:28:14Z jbit $
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
	s32		tmp;
	ps2time timeBuf={
		0, 0x0D, 0x0E, 0x0A, 0x0D, 1, 2003// used if can not get time... 
	};

	if(sceCdReadClock(&cdtime)!=0 && cdtime.stat==0)
	{
		tmp=cdtime.second>>4;
		timeBuf.sec=(u32)(((tmp<<2)+tmp)<<1)+(cdtime.second&0x0F);
		tmp=cdtime.minute>>4;
		timeBuf.min=(((tmp<<2)+tmp)<<1)+(cdtime.minute&0x0F);
		tmp=cdtime.hour>>4;
		timeBuf.hour=(((tmp<<2)+tmp)<<1)+(cdtime.hour&0x0F);
		tmp=cdtime.day>>4;
		timeBuf.day=(((tmp<<2)+tmp)<<1)+(cdtime.day&0x0F);
		tmp=(cdtime.month>>4)&0x7F;
		timeBuf.month=(((tmp<<2)+tmp)<<1)+(cdtime.month&0x0F);
		tmp=cdtime.year>>4;
		timeBuf.year=(((tmp<<2)+tmp)<<1)+(cdtime.year&0xF)+2000;
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

	// v12/v13 EELOADCNF fix: do not check iLinkID
	//if(sceCdRI(idbuf, &err))
	//	if(err==0)
	//		return 0;
	//dprintf1("ps2hdd: Error: cannot get ilink id\n");
	//return -EIO;
	sceCdRI(idbuf, &err);
	return 0;
}
