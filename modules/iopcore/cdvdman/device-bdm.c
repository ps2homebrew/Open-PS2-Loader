/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

#include <bdm.h>

#include "device.h"

extern struct cdvdman_settings_bdm cdvdman_settings;
static struct block_device *g_bd = NULL;
static u32 g_bd_sectors_per_sector = 4;
static int bdm_io_sema;

extern struct irx_export_table _exp_bdm;

//
// BDM exported functions
//

void bdm_connect_bd(struct block_device *bd)
{
    DPRINTF("connecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

    if (g_bd == NULL) {
        g_bd = bd;
        g_bd_sectors_per_sector = (2048 / bd->sectorSize);
        // Free usage of block device
        SignalSema(bdm_io_sema);
    }
}

void bdm_disconnect_bd(struct block_device *bd)
{
    DPRINTF("disconnecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

    // Lock usage of block device
    WaitSema(bdm_io_sema);
    if (g_bd == bd)
        g_bd = NULL;
}

//
// cdvdman "Device" functions
//

void DeviceInit(void)
{
    iop_sema_t smp;

    DPRINTF("%s\n", __func__);

    // Create semaphore, initially locked
    smp.initial = 0;
    smp.max = 1;
    smp.option = 0;
    smp.attr = SA_THPRI;
    bdm_io_sema = CreateSema(&smp);

    RegisterLibraryEntries(&_exp_bdm);
}

void DeviceDeinit(void)
{
    DPRINTF("%s\n", __func__);
}

int DeviceReady(void)
{
    // DPRINTF("%s\n", __func__);

    return (g_bd == NULL) ? SCECdNotReady : SCECdComplete;
}

void DeviceStop(void)
{
    DPRINTF("%s\n", __func__);

    if (g_bd != NULL)
        g_bd->stop(g_bd);
}

void DeviceFSInit(void)
{
    int i;
    DPRINTF("USB: NumParts = %d\n", cdvdman_settings.common.NumParts);
    for (i = 0; i < cdvdman_settings.common.NumParts; i++)
        DPRINTF("USB: LBAs[%d] = %lu\n", i, cdvdman_settings.LBAs[i]);

    DPRINTF("Waiting for device...\n");
    WaitSema(bdm_io_sema);
    DPRINTF("Waiting for device...done!\n");
    SignalSema(bdm_io_sema);
}

void DeviceLock(void)
{
    DPRINTF("%s\n", __func__);

    WaitSema(bdm_io_sema);
}

void DeviceUnmount(void)
{
    DPRINTF("%s\n", __func__);
}

u8 DeviceGetCacheSize()
{
    return cdvdman_settings.common.bdm_cache;
}

void DeviceSetupZSO(u8 *buffer)
{
}

int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors)
{
    u32 sector;
    u16 count;
    register u32 r, sectors_to_read, lbound, ubound, nlsn, offslsn;
    register int i, esc_flag = 0;
    u8 *p = (u8 *)buffer;
    int rv = SCECdErNO;

    // DPRINTF("%s(%u, 0x%p, %u)\n", __func__, (unsigned int)lsn, buffer, sectors);

    if (g_bd == NULL)
        return SCECdErTRMOPN;

    lbound = 0;
    ubound = (cdvdman_settings.common.NumParts > 1) ? 0x80000 : 0xFFFFFFFF;
    offslsn = lsn;
    r = nlsn = 0;
    sectors_to_read = sectors;

    WaitSema(bdm_io_sema);
    for (i = 0; i < cdvdman_settings.common.NumParts; i++, lbound = ubound, ubound += 0x80000, offslsn -= 0x80000) {

        if (lsn >= lbound && lsn < ubound) {
            if ((lsn + sectors) > (ubound - 1)) {
                sectors_to_read = ubound - lsn;
                sectors -= sectors_to_read;
                nlsn = ubound;
            } else
                esc_flag = 1;

            sector = cdvdman_settings.LBAs[i] + (offslsn * g_bd_sectors_per_sector);
            count = sectors_to_read * g_bd_sectors_per_sector;
            if (g_bd->read(g_bd, sector, &p[r], count) != count) {
                rv = SCECdErREAD;
                break;
            }

            r += sectors_to_read * 2048;
            offslsn += sectors_to_read;
            sectors_to_read = sectors;
            lsn = nlsn;
        }

        if (esc_flag)
            break;
    }
    SignalSema(bdm_io_sema);

    return rv;
}

//
// oplutils exported function, used by MCEMU
//

void bdm_readSector(unsigned int lba, unsigned short int nsectors, unsigned char *buffer)
{
    DPRINTF("%s\n", __func__);

    WaitSema(bdm_io_sema);
    g_bd->read(g_bd, lba, buffer, nsectors);
    SignalSema(bdm_io_sema);
}

void bdm_writeSector(unsigned int lba, unsigned short int nsectors, const unsigned char *buffer)
{
    DPRINTF("%s\n", __func__);

    WaitSema(bdm_io_sema);
    g_bd->write(g_bd, lba, buffer, nsectors);
    SignalSema(bdm_io_sema);
}
