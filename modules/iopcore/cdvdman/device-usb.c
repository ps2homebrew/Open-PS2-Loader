/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "smsutils.h"
#include "mass_common.h"
#include "mass_stor.h"
#include "atad.h"
#include "ioplib_util.h"
#include "cdvdman.h"
#include "internal.h"
#include "cdvd_config.h"

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <ioman.h>
#include <thsemap.h>
#include <usbd.h>
#include <errno.h>
#include <io_common.h>
#include "ioman_add.h"

#include <errno.h>

#include "device.h"

extern struct cdvdman_settings_usb cdvdman_settings;

extern int usb_io_sema;

static void usbd_init(void);

// !!! usbd exports functions pointers !!!
int (*pUsbRegisterDriver)(UsbDriver *driver);                                                          // #4
void *(*pUsbGetDeviceStaticDescriptor)(int devId, void *data, u8 type);                                // #6
int (*pUsbSetDevicePrivateData)(int devId, void *data);                                                // #7
int (*pUsbOpenEndpoint)(int devId, UsbEndpointDescriptor *desc);                                       // #9
int (*pUsbCloseEndpoint)(int id);                                                                      // #10
int (*pUsbTransfer)(int id, void *data, u32 len, void *option, UsbCallbackProc callback, void *cbArg); // #11
int (*pUsbOpenEndpointAligned)(int devId, UsbEndpointDescriptor *desc);                                // #12

static void usbd_init(void)
{
    modinfo_t info;
    getModInfo("usbd\0\0\0\0", &info);

    // Set functions pointers here
    pUsbRegisterDriver = info.exports[4];
    pUsbGetDeviceStaticDescriptor = info.exports[6];
    pUsbSetDevicePrivateData = info.exports[7];
    pUsbOpenEndpoint = info.exports[9];
    pUsbCloseEndpoint = info.exports[10];
    pUsbTransfer = info.exports[11];
    pUsbOpenEndpointAligned = info.exports[12];
}

void DeviceInit(void)
{
}

void DeviceDeinit(void)
{
}

void DeviceStop(void)
{
    mass_stor_stop_unit();
}

void DeviceFSInit(void)
{
    // initialize usbd exports
    usbd_init();

    // initialize the mass driver
    mass_stor_init();

    // configure mass device
    while (mass_stor_configureDevice() <= 0)
        DelayThread(5000);
}

void DeviceLock(void)
{
    WaitSema(usb_io_sema);
}

void DeviceUnmount(void)
{
}

int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors)
{
    register u32 r, sectors_to_read, lbound, ubound, nlsn, offslsn;
    register int i, esc_flag = 0;
    u8 *p = (u8 *)buffer;

    lbound = 0;
    ubound = (cdvdman_settings.common.NumParts > 1) ? 0x80000 : 0xFFFFFFFF;
    offslsn = lsn;
    r = nlsn = 0;
    sectors_to_read = sectors;

    for (i = 0; i < cdvdman_settings.common.NumParts; i++, lbound = ubound, ubound += 0x80000, offslsn -= 0x80000) {

        if (lsn >= lbound && lsn < ubound) {
            if ((lsn + sectors) > (ubound - 1)) {
                sectors_to_read = ubound - lsn;
                sectors -= sectors_to_read;
                nlsn = ubound;
            } else
                esc_flag = 1;

            mass_stor_ReadCD(offslsn, sectors_to_read, &p[r], i);

            r += sectors_to_read << 11;
            offslsn += sectors_to_read;
            sectors_to_read = sectors;
            lsn = nlsn;
        }

        if (esc_flag)
            break;
    }

    return 0;
}
