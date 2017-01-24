/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Free space calculation routines
*/

#include <errno.h>
#include <iomanX.h>
#include <sysclib.h>
#include <stdio.h>
#include <atad.h>
#include <hdd-ioctl.h>

#include "apa-opt.h"
#include "libapa.h"

static void apaCalculateFreeSpace(u32 *free, u32 sectors)
{
    if (0x1FFFFF < sectors) {
        *free += sectors;
        return;
    }

    if ((*free & sectors) == 0) {
        *free |= sectors;
        return;
    }

    for (sectors /= 2; 0x3FFFF < sectors; sectors /= 2)
        *free |= sectors;
}

int apaGetFreeSectors(s32 device, u32 *free, apa_device_t *deviceinfo)
{
    u32 sectors, partMax;
    int rv;
    apa_cache_t *clink;

    sectors = 0;
    *free = 0;
    if ((clink = apaCacheGetHeader(device, 0, APA_IO_MODE_READ, &rv)) != NULL) {
        do {
            if (clink->header->type == 0)
                apaCalculateFreeSpace(free, clink->header->length);
            sectors += clink->header->length;
        } while ((clink = apaGetNextHeader(clink, &rv)) != NULL);
    }

    if (rv == 0) {
        for (partMax = deviceinfo[device].partitionMaxSize; 0x0003FFFF < partMax; partMax = deviceinfo[device].partitionMaxSize) { //As weird as it looks, this was how it was done in the original HDD.IRX.
            for (; 0x0003FFFF < partMax; partMax /= 2) {
                //Non-SONY: Perform 64-bit arithmetic here to avoid overflows when dealing with large disks.
                if ((sectors % partMax == 0) && ((u64)sectors + partMax < deviceinfo[device].totalLBA)) {
                    apaCalculateFreeSpace(free, partMax);
                    sectors += partMax;
                    break;
                }
            }

            if (0x0003FFFF >= partMax)
                break;
        }

        APA_PRINTF(APA_DRV_NAME ": total = %08lx sectors, installable = %08lx sectors.\n", sectors, *free);
    }

    return rv;
}
