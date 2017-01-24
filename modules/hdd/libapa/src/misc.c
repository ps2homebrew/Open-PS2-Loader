/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Miscellaneous routines
*/

#include <errno.h>
#include <intrman.h>
#include <iomanX.h>
#include <cdvdman.h>
#include <sysclib.h>
#include <stdio.h>
#include <sysmem.h>
#include <hdd-ioctl.h>

#include "apa-opt.h"
#include "libapa.h"

void *apaAllocMem(int size)
{
    int intrStat;
    void *mem;

    CpuSuspendIntr(&intrStat);
    mem = AllocSysMemory(ALLOC_FIRST, size, NULL);
    CpuResumeIntr(intrStat);

    return mem;
}

void apaFreeMem(void *ptr)
{
    int intrStat;

    CpuSuspendIntr(&intrStat);
    FreeSysMemory(ptr);
    CpuResumeIntr(intrStat);
}

int apaGetTime(apa_ps2time_t *tm)
{
    sceCdCLOCK cdtime;
    apa_ps2time_t timeBuf = {0, 7, 6, 5, 4, 3, 2000};

    if (sceCdReadClock(&cdtime) != 0 && cdtime.stat == 0) {
        timeBuf.sec = btoi(cdtime.second);
        timeBuf.min = btoi(cdtime.minute);
        timeBuf.hour = btoi(cdtime.hour);
        timeBuf.day = btoi(cdtime.day);
        timeBuf.month = btoi(cdtime.month & 0x7F); //The old CDVDMAN sceCdReadClock() function does not automatically file off the highest bit.
        timeBuf.year = btoi(cdtime.year) + 2000;
    }
    memcpy(tm, &timeBuf, sizeof(apa_ps2time_t));
    return 0;
}

int apaGetIlinkID(u8 *idbuf)
{
    u32 err = 0;

    memset(idbuf, 0, 32);
    if (sceCdRI(idbuf, &err))
        if (err == 0)
            return 0;
    APA_PRINTF(APA_DRV_NAME ": Error: cannot get ilink id\n");
    return -EIO;
}
