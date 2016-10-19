#include <stdio.h>
#include <loadcore.h>
#include <usbhdfsd.h>
#include <sifcmd.h>

#include <irx.h>

IRX_ID("usbhdfsd_for_EE", 1, 1);

static void usbmass_cb(int cause)
{
    static SifCmdHeader_t EventCmdData;

    EventCmdData.opt = cause;
    sceSifSendCmd(0, &EventCmdData, sizeof(EventCmdData), NULL, NULL, 0);
}

int _start(int argc, char *argv[])
{
    UsbMassRegisterCallback(0, &usbmass_cb);

    return MODULE_RESIDENT_END;
}
