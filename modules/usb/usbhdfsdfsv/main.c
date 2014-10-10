#include <stdio.h>
#include <loadcore.h>
#include <usbhdfsd.h>
#include <sifcmd.h>

#include <irx.h>

IRX_ID("usbhdfsd_for_EE", 1, 0);

static void usbmass_cb(int cause)
{
	static SifCmdHeader_t EventCmdData;

	EventCmdData.unknown = cause;
	sceSifSendCmd(12, &EventCmdData, sizeof(EventCmdData), NULL, NULL, 0);
}

int _start(int argc, char *argv[])
{
	UsbMassRegisterCallback(0, &usbmass_cb);

	return MODULE_RESIDENT_END;
}
