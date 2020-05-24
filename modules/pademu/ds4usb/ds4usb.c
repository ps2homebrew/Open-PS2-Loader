#include "types.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "thsemap.h"
#include "ds4usb.h"

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

#define REQ_USB_OUT (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE)
#define REQ_USB_IN  (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE)

#define MAX_PADS 4

static u8 rgbled_patterns[][2][3] =
    {
        {{0x00, 0x00, 0x10}, {0x00, 0x00, 0x7F}}, // light blue/blue
        {{0x00, 0x10, 0x00}, {0x00, 0x7F, 0x00}}, // light green/green
        {{0x10, 0x10, 0x00}, {0x7F, 0x7F, 0x00}}, // light yellow/yellow
        {{0x00, 0x10, 0x10}, {0x00, 0x7F, 0x7F}}, // light cyan/cyan
};

static u8 usb_buf[MAX_BUFFER_SIZE + 32] __attribute((aligned(4))) = {0};

int usb_probe(int devId);
int usb_connect(int devId);
int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

UsbDriver usb_driver = {NULL, NULL, "ds4usb", usb_probe, usb_connect, usb_disconnect};

static void readReport(u8 *data, int pad);
static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad);

ds4usb_device ds4dev[MAX_PADS];

int usb_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("DS4USB: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("DS4USB: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->idVendor == SONY_VID && (device->idProduct == DS4_PID || device->idProduct == DS4_PID_SLIM))
        return 1;

    return 0;
}

int usb_connect(int devId)
{
    int pad, epCount;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;
    UsbEndpointDescriptor *endpoint;

    DPRINTF("DS4USB: connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds4dev[pad].usb_id == -1)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("DS4USB: Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(ds4dev[pad].sema);

    ds4dev[pad].dev.id = pad;
    ds4dev[pad].usb_id = devId;
    ds4dev[pad].controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);
    epCount = interface->bNumEndpoints - 1;
    
	if (device->idProduct == DS4_PID_SLIM) {
		epCount = 20; // ds4 v2 returns interface->bNumEndpoints as 0
	}
	
	endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && ds4dev[pad].interruptEndp < 0) {
                ds4dev[pad].interruptEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("DS4USB: register Event endpoint id =%i addr=%02X packetSize=%i\n", ds4dev[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && ds4dev[pad].outEndp < 0) {
                ds4dev[pad].outEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("DS34USB: register Output endpoint id =%i addr=%02X packetSize=%i\n", ds4pad[pad].outEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (ds4dev[pad].interruptEndp < 0 || ds4dev[pad].outEndp < 0) {
        usb_release(pad);
        return 1;
    }

    UsbSetDeviceConfiguration(ds4dev[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);
    SignalSema(ds4dev[pad].sema);

    return 0;
}

int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("DS4USB: disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds4dev[pad].usb_id == devId) {
            pademu_disconnect(&ds4dev[pad].dev);
            break;
        }
    }

    if (pad < MAX_PADS)
        usb_release(pad);

    return 0;
}

static void usb_release(int pad)
{
    PollSema(ds4dev[pad].sema);
    
    if (ds4dev[pad].interruptEndp >= 0)
        UsbCloseEndpoint(ds4dev[pad].interruptEndp);

	if (ds4dev[pad].outEndp >= 0)
        UsbCloseEndpoint(ds4dev[pad].outEndp);

    ds4dev[pad].controlEndp = -1;
    ds4dev[pad].interruptEndp = -1;
    ds4dev[pad].outEndp = -1;
    ds4dev[pad].usb_id = -1;
    
    SignalSema(ds4dev[pad].sema);
}

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("DS4USB: usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    ds4dev[pad].usb_resultcode = resultCode;

    SignalSema(ds4dev[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("DS4USB: usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(ds4dev[pad].cmd_sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;
    u8 led[4];

    PollSema(ds4dev[pad].sema);

	pademu_connect(&ds4dev[pad].dev);

    led[0] = rgbled_patterns[ds4dev[pad].dev.id][1][0];
    led[1] = rgbled_patterns[ds4dev[pad].dev.id][1][1];
    led[2] = rgbled_patterns[ds4dev[pad].dev.id][1][2];
    led[3] = 0;

    LEDRumble(led, 0, 0, pad);
    SignalSema(ds4dev[pad].sema);
}

#define MAX_DELAY 10

static void readReport(u8 *data, int pad)
{
    ds4report_t *report = (ds4report_t *)data;
    if (report->ReportID == 0x01) {
        u8 up = 0, down = 0, left = 0, right = 0;

        switch (report->Dpad) {
            case 0:
                up = 1;
                break;
            case 1:
                up = 1;
                right = 1;
                break;
            case 2:
                right = 1;
                break;
            case 3:
                down = 1;
                right = 1;
                break;
            case 4:
                down = 1;
                break;
            case 5:
                down = 1;
                left = 1;
                break;
            case 6:
                left = 1;
                break;
            case 7:
                up = 1;
                left = 1;
                break;
            case 8:
                up = 0;
                down = 0;
                left = 0;
                right = 0;
                break;
        }

        if (report->TPad) {
            if (!report->Finger1Active) {
                if (report->Finger1X < 960)
                    report->Share = 1;
                else
                    report->Option = 1;
            }

            if (!report->Finger2Active) {
                if (report->Finger2X < 960)
                    report->Share = 1;
                else
                    report->Option = 1;
            }
        }

        ds4dev[pad].data[0] = ~(report->Share | report->L3 << 1 | report->R3 << 2 | report->Option << 3 | up << 4 | right << 5 | down << 6 | left << 7);
        ds4dev[pad].data[1] = ~(report->L2 | report->R2 << 1 | report->L1 << 2 | report->R1 << 3 | report->Triangle << 4 | report->Circle << 5 | report->Cross << 6 | report->Square << 7);

        ds4dev[pad].data[2] = report->RightStickX; //rx
        ds4dev[pad].data[3] = report->RightStickY; //ry
        ds4dev[pad].data[4] = report->LeftStickX;  //lx
        ds4dev[pad].data[5] = report->LeftStickY;  //ly

        ds4dev[pad].data[6] = right * 255; //right
        ds4dev[pad].data[7] = left * 255;  //left
        ds4dev[pad].data[8] = up * 255;    //up
        ds4dev[pad].data[9] = down * 255;  //down

        ds4dev[pad].data[10] = report->Triangle * 255; //triangle
        ds4dev[pad].data[11] = report->Circle * 255;   //circle
        ds4dev[pad].data[12] = report->Cross * 255;    //cross
        ds4dev[pad].data[13] = report->Square * 255;   //square

        ds4dev[pad].data[14] = report->L1 * 255;   //L1
        ds4dev[pad].data[15] = report->R1 * 255;   //R1
        ds4dev[pad].data[16] = report->PressureL2; //L2
        ds4dev[pad].data[17] = report->PressureR2; //R2

        if (report->PSButton) {                                           //display battery level
            if (report->Share && (ds4dev[pad].btn_delay == MAX_DELAY)) { //PS + Share
                if (ds4dev[pad].analog_btn < 2)                          //unlocked mode
                    ds4dev[pad].analog_btn = !ds4dev[pad].analog_btn;

                ds4dev[pad].oldled[0] = rgbled_patterns[pad][(ds4dev[pad].analog_btn & 1)][0];
                ds4dev[pad].oldled[1] = rgbled_patterns[pad][(ds4dev[pad].analog_btn & 1)][1];
                ds4dev[pad].oldled[2] = rgbled_patterns[pad][(ds4dev[pad].analog_btn & 1)][2];
                ds4dev[pad].btn_delay = 1;
            } else {
                ds4dev[pad].oldled[0] = report->Battery;
                ds4dev[pad].oldled[1] = 0;
                ds4dev[pad].oldled[2] = 0;

                if (ds4dev[pad].btn_delay < MAX_DELAY)
                    ds4dev[pad].btn_delay++;
            }
        } else {
            ds4dev[pad].oldled[0] = rgbled_patterns[pad][(ds4dev[pad].analog_btn & 1)][0];
            ds4dev[pad].oldled[1] = rgbled_patterns[pad][(ds4dev[pad].analog_btn & 1)][1];
            ds4dev[pad].oldled[2] = rgbled_patterns[pad][(ds4dev[pad].analog_btn & 1)][2];

            if (ds4dev[pad].btn_delay > 0)
                ds4dev[pad].btn_delay--;
        }

        if (report->Power != 0xB && report->Usb_plugged) //charging
            ds4dev[pad].oldled[3] = 1;
        else
            ds4dev[pad].oldled[3] = 0;

		if (ds4dev[pad].btn_delay > 0) {
            ds4dev[pad].update_rum = 1;
        }
    }
}

static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad)
{
    PollSema(ds4dev[pad].cmd_sema);
    
    usb_buf[0] = 0x05;
    usb_buf[1] = 0xFF;

    usb_buf[4] = rrum * 255; //ds4 has full control
    usb_buf[5] = lrum;

    usb_buf[6] = led[0]; //r
    usb_buf[7] = led[1]; //g
    usb_buf[8] = led[2]; //b

    if (led[3]) //means charging, so blink
    {
        usb_buf[9] = 0x80;  // Time to flash bright (255 = 2.5 seconds)
        usb_buf[10] = 0x80; // Time to flash dark (255 = 2.5 seconds)
    }

	ds4dev[pad].oldled[0] = led[0];
    ds4dev[pad].oldled[1] = led[1];
    ds4dev[pad].oldled[2] = led[2];
    ds4dev[pad].oldled[3] = led[3];
	
	return UsbInterruptTransfer(ds4dev[pad].outEndp, usb_buf, 32, usb_cmd_cb, (void *)pad);
}

static unsigned int timeout(void *arg)
{
    int sema = (int)arg;
    iSignalSema(sema);
    return 0;
}

static void TransferWait(int sema)
{
    iop_sys_clock_t cmd_timeout;

    cmd_timeout.lo = 200000;
    cmd_timeout.hi = 0;

    if (SetAlarm(&cmd_timeout, timeout, (void *)sema) == 0) {
        WaitSema(sema);
        CancelAlarm(timeout, NULL);
    }
}

void ds4usb_set_rumble(u8 lrum, u8 rrum, int port)
{
    WaitSema(ds4dev[port].sema);
    
    ds4dev[port].update_rum = 1;
    ds4dev[port].lrum = lrum;
    ds4dev[port].rrum = rrum;
        
    SignalSema(ds4dev[port].sema);
}

int ds4usb_get_data(u8 *dst, int size, int port)
{
    int ret = 0;

    WaitSema(ds4dev[port].sema);

    PollSema(ds4dev[port].sema);

    ret = UsbInterruptTransfer(ds4dev[port].interruptEndp, usb_buf, MAX_BUFFER_SIZE, usb_data_cb, (void *)port);

    if (ret == USB_RC_OK) {
        TransferWait(ds4dev[port].sema);
        if (!ds4dev[port].usb_resultcode)
            readReport(usb_buf, port);

        ds4dev[port].usb_resultcode = 1;
    } else {
        DPRINTF("DS4USB: DS4USB_get_data usb transfer error %d\n", ret);
    }

    mips_memcpy(dst, ds4dev[port].data, size);
    ret = ds4dev[port].analog_btn & 1;
        
    if (ds4dev[port].update_rum) {
        ret = LEDRumble(ds4dev[port].oldled, ds4dev[port].lrum, ds4dev[port].rrum, port);
        if (ret == USB_RC_OK)
            TransferWait(ds4dev[port].cmd_sema);
        else
            DPRINTF("DS4USB: LEDRumble usb transfer error %d\n", ret);

        ds4dev[port].update_rum = 0;
    }

    SignalSema(ds4dev[port].sema);

    return ret;
}

void ds4usb_set_mode(int mode, int lock, int port)
{
    if (lock == 3)
        ds4dev[port].analog_btn = 3;
    else
        ds4dev[port].analog_btn = mode;
}

void ds4usb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

int _start(int argc, char *argv[])
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        ds4dev[pad].usb_id = -1;
		ds4dev[pad].dev.id = -1;
        ds4dev[pad].dev.pad_get_data = ds4usb_get_data;
        ds4dev[pad].dev.pad_set_rumble = ds4usb_set_rumble;
        ds4dev[pad].dev.pad_set_mode = ds4usb_set_mode;

        ds4dev[pad].oldled[0] = 0;
        ds4dev[pad].oldled[1] = 0;
        ds4dev[pad].lrum = 0;
        ds4dev[pad].rrum = 0;
        ds4dev[pad].update_rum = 1;
        ds4dev[pad].sema = -1;
        ds4dev[pad].cmd_sema = -1;
        ds4dev[pad].controlEndp = -1;
        ds4dev[pad].interruptEndp = -1;
        ds4dev[pad].outEndp = -1;

        ds4dev[pad].data[0] = 0xFF;
        ds4dev[pad].data[1] = 0xFF;
        ds4dev[pad].analog_btn = 0;

        mips_memset(&ds4dev[pad].data[2], 0x7F, 4);
        mips_memset(&ds4dev[pad].data[6], 0x00, 12);

        ds4dev[pad].sema = CreateMutex(IOP_MUTEX_UNLOCKED);
        ds4dev[pad].cmd_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

        if (ds4dev[pad].sema < 0 || ds4dev[pad].cmd_sema < 0) {
            DPRINTF("DS4USB: Failed to allocate I/O semaphore.\n");
            return MODULE_NO_RESIDENT_END;
        }
    }

    if (UsbRegisterDriver(&usb_driver) != USB_RC_OK) {
        DPRINTF("DS4USB: Error registering USB devices\n");
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}
