/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

#include "device.h"

#include "../../isofs/zso.h"

extern struct cdvdman_settings_hdd cdvdman_settings;

extern struct irx_export_table _exp_atad;

char lba_48bit = 0;
char atad_inited = 0;
static unsigned char CurrentPart = 0;
static unsigned char NumParts;

static hdl_partspecs_t cdvdman_partspecs[HDL_NUM_PART_SPECS];

#ifdef HD_PRO
extern int ata_device_set_write_cache(int device, int enable);
#endif

extern int ata_io_sema;

static int cdvdman_get_part_specs(u32 lsn)
{
    register int i;
    hdl_partspecs_t *ps;

    for (ps = cdvdman_partspecs, i = 0; i < NumParts; i++, ps++) {
        if ((lsn >= ps->part_offset) && (lsn < (ps->part_offset + (ps->part_size / 2048)))) {
            CurrentPart = i;
            break;
        }
    }

    if (i >= NumParts)
        return -ENXIO;

    return 0;
}

void DeviceInit(void)
{
    RegisterLibraryEntries(&_exp_atad);

    atad_start();
    atad_inited = 1;

    lba_48bit = cdvdman_settings.common.media;

    hdl_apa_header apaHeader;
    int r;

    DPRINTF("DeviceInit: apa header LBA = %lu\n", cdvdman_settings.lba_start);

#ifdef HD_PRO
    // For HDPro, as its custom ATAD module does not export ata_io_start() and ata_io_finish(). And it also resets the ATA bus.
    if (cdvdman_settings.common.flags & IOPCORE_ENABLE_POFF) {
        // If IGR is enabled (the poweroff function here is disabled), we can tell when to flush the cache. Hence if IGR is disabled, then we should disable the write cache.
        ata_device_set_write_cache(0, 0);
    }
#endif

    while ((r = ata_device_sector_io(0, &apaHeader, cdvdman_settings.lba_start, 2, ATA_DIR_READ)) != 0) {
        DPRINTF("DeviceInit: failed to read apa header %d\n", r);
        DelayThread(2000);
    }

    memcpy(cdvdman_partspecs, apaHeader.part_specs, sizeof(cdvdman_partspecs));

    cdvdman_settings.common.media = apaHeader.discType;
    if (cdvdman_settings.common.layer1_start == 0) // layer1 start not set, read it from APA header
        cdvdman_settings.common.layer1_start = apaHeader.layer1_start;
    NumParts = apaHeader.num_partitions;
}

void DeviceDeinit(void)
{
}

int DeviceReady(void)
{
    return SCECdComplete;
}

void DeviceFSInit(void)
{
}

void DeviceLock(void)
{
    WaitSema(ata_io_sema);
}

void DeviceUnmount(void)
{
    ata_device_flush_cache(0);
}

void DeviceStop(void)
{
    // This will be handled by ATAD.
}

int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors)
{
    u32 offset = 0;
    while (sectors) {
        if (!((lsn >= cdvdman_partspecs[CurrentPart].part_offset) && (lsn < (cdvdman_partspecs[CurrentPart].part_offset + (cdvdman_partspecs[CurrentPart].part_size / 2048)))))
            if (cdvdman_get_part_specs(lsn) != 0)
                return SCECdErTRMOPN;

        u32 nsectors = (cdvdman_partspecs[CurrentPart].part_offset + (cdvdman_partspecs[CurrentPart].part_size / 2048)) - lsn;
        if (sectors < nsectors)
            nsectors = sectors;

        u32 lba = cdvdman_partspecs[CurrentPart].data_start + ((lsn - cdvdman_partspecs[CurrentPart].part_offset) << 2);
        if (ata_device_sector_io(0, (void *)((u8 *)buffer + offset), lba, nsectors << 2, ATA_DIR_READ) != 0) {
            return SCECdErREAD;
        }
        offset += nsectors * 2048;
        sectors -= nsectors;
        lsn += nsectors;
    }

    return SCECdErNO;
}
