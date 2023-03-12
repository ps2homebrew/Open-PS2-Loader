/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

#include <bdm.h>
#include <bd_defrag.h>

#include "device.h"

#ifdef USE_BDM_ATA
#include "atad.h"

char lba_48bit = 0;
#endif

#define U64_2XU32(val)  ((u32*)val)[1], ((u32*)val)[0]

extern struct cdvdman_settings_bdm cdvdman_settings;
static struct block_device *g_bd = NULL;
static u32 g_bd_sectors_per_sector = 4;
static int bdm_io_sema;

#ifndef USE_BDM_ATA
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

#else

// Event flags for sleeping while waiting for the block device fs mounting operations to complete.
int bdm_ata_ef;

// Note: When running in bdm ata mode we "delay load" the imports for bdm.irx. Cdvdman.irx can't import from bdm.irx without
// changing the IOP reset procedure to make sure bdm.irx is loaded first (otherwise cdvdman.irx fails to resolve the imports and
// thus fails to load). Making that change is more work than just delay loading the import addresses.

void BdmDeviceSetBlockDevice(struct block_device *bd)
{
    static void (*bdm_connect_bd_imp)(struct block_device *bd) = NULL;

    if (bdm_connect_bd_imp == NULL)
    {
        // Get the module info for bdm and resolve the import address.
        modinfo_t bdmInfo;
        if (getModInfo("bdm\0\0\0\0\0", &bdmInfo) == 1)
        {
            bdm_connect_bd_imp = (void(*)(struct block_device*))bdmInfo.exports[4];
        }
        else
        {
            DPRINTF("bdm_connect_bd: failed to delay load import address!!\n");
            return;
        }
    }
    
    bdm_connect_bd_imp(bd);
}

int BdmFindTargetBlockDevice(int devNum, int partNum)
{
    struct block_device* pBlockDevices[10] = { 0 };

    static void (*bdm_get_bd_imp)(struct block_device **pbd, unsigned int count) = NULL;

    if (bdm_get_bd_imp == NULL)
    {
        // Get the module info for bdm and resolve the import address.
        modinfo_t bdmInfo;
        if (getModInfo("bdm\0\0\0\0\0", &bdmInfo) == 1)
        {
            bdm_get_bd_imp = (void(*)(struct block_device**, unsigned int))bdmInfo.exports[8];
        }
        else
        {
            DPRINTF("BdmFindTargetBlockDevice: failed to delay load import address!!\n");
            return 0;
        }
    }

    // Get a list of block devices from bdm and find the target device the game ISO is on.
    bdm_get_bd_imp(pBlockDevices, 10);
    for (int i = 0; i < 10; i++)
    {
        if (pBlockDevices[i] != NULL && pBlockDevices[i]->devNr == devNum && pBlockDevices[i]->parNr == partNum)
        {
            DPRINTF("BdmFindTargetBlockDevice: using device %s%dp%d\n", pBlockDevices[i]->name, pBlockDevices[i]->devNr, pBlockDevices[i]->parNr);
            
            g_bd = pBlockDevices[i];
            g_bd_sectors_per_sector = (2048 / pBlockDevices[i]->sectorSize);
            // Free usage of block device
            SignalSema(bdm_io_sema);
            return 1;
        }
    }

    // Target device not found.
    return 0;
}

unsigned int BdmFindDeviceSleepCallback(void *arg)
{
    // Signal the main thread to wake up and check if fs init has completed.
    iSetEventFlag(bdm_ata_ef, 1);
    return 0;
}
#endif

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

#ifndef USE_BDM_ATA
    RegisterLibraryEntries(&_exp_bdm);
#else

    // Initialize the event flags for waiting on bdm fs mounting operations.
    iop_event_t event;

    event.attr = EA_MULTI;
    event.option = 0;
    event.bits = 0;

    bdm_ata_ef = CreateEventFlag(&event);

#endif
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
    DPRINTF("DeviceFSInit [BDM]\n");

#ifdef USE_BDM_ATA
    lba_48bit = 1; //cdvdman_settings.common.media;

    // Atad cannot be initialized in DeviceInit because bdm.irx is not yet loaded and it will fail to register the HDD as
    // a block device. Now that bdm.irx has been loaded, run atad init.
    atad_start();

    DPRINTF("After atad_start()\n");

    // Scan the block devices mounted and find the one the game ISO is on.
    while (BdmFindTargetBlockDevice(0, 2) == 0)
    {
        iop_sys_clock_t TargetTime;
        TargetTime.lo = 100000;

        // Sleep for 1000 (msec?) and then check if the fs init operations have completed.
        ClearEventFlag(bdm_ata_ef, 1);
        SetAlarm(&TargetTime, &BdmFindDeviceSleepCallback, NULL);

        DPRINTF("DeviceFSInit: fs init not ready yet...\n");
        WaitEventFlag(bdm_ata_ef, 1, WEF_AND, NULL);
    }

    // TODO: there's more cdvdman init stuff after this in device-hdd.c...
    DPRINTF("DiskType=%d Layer1Start=0x%08x\n", cdvdman_settings.common.media, cdvdman_settings.common.layer1_start);
#endif

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

int DeviceReadSectors(u64 lsn, void *buffer, unsigned int sectors)
{
    int rv = SCECdErNO;

    DPRINTF("%s(0x%08x%08x, 0x%p, %u)\n", __func__, U64_2XU32(&lsn), buffer, sectors);

    if (g_bd == NULL)
        return SCECdErTRMOPN;

    WaitSema(bdm_io_sema);
    if (bd_defrag(g_bd, cdvdman_settings.fragfile[0].frag_count, &cdvdman_settings.frags[cdvdman_settings.fragfile[0].frag_start], lsn * 4, buffer, sectors * 4) != (sectors * 4))
        rv = SCECdErREAD;
    SignalSema(bdm_io_sema);

    return rv;
}

//
// oplutils exported function, used by MCEMU
//

void bdm_readSector(u64 lba, unsigned short int nsectors, unsigned char *buffer)
{
    DPRINTF("%s\n", __func__);

    WaitSema(bdm_io_sema);
    g_bd->read(g_bd, lba, buffer, nsectors);
    SignalSema(bdm_io_sema);
}

void bdm_writeSector(u64 lba, unsigned short int nsectors, const unsigned char *buffer)
{
    DPRINTF("%s\n", __func__);

    WaitSema(bdm_io_sema);
    g_bd->write(g_bd, lba, buffer, nsectors);
    SignalSema(bdm_io_sema);
}
