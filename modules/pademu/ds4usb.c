/* based on https://github.com/IonAgorria/Arduino-PSRemote */
/* and https://github.com/felis/USB_Host_Shield_2.0 */

#include "types.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "thsemap.h"
#include "sys_utils.h"
#include "pademu.h"
#include "pademu_common.h"
#include "ds4usb.h"
#include "padmacro.h"

#define MODNAME "DS4USB"

#ifdef DEBUG
#define DPRINTF(format, args...) \
    printf(MODNAME ": " format, ##args)
#else
#define DPRINTF(args...)
#endif

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

static int usb_probe(int devId);
static int usb_connect(int devId);
static int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

static UsbDriver usb_driver = {NULL, NULL, "ds4usb", usb_probe, usb_connect, usb_disconnect};

static void readReport(u8 *data, int pad);
static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad);

static int ds4usb_get_model(struct pad_funcs *pf);
static int ds4usb_get_data(struct pad_funcs *pf, u8 *dst, int size, int port);
static void ds4usb_set_rumble(struct pad_funcs *pf, u8 lrum, u8 rrum);
static void ds4usb_set_mode(struct pad_funcs *pf, int mode, int lock);

static ds4usb_device ds4pad[MAX_PADS];
static struct pad_funcs padf[MAX_PADS];

static int usb_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->idVendor == SONY_VID && (device->idProduct == GUITAR_HERO_PS3_PID || device->idProduct == ROCK_BAND_PS3_PID)) {
        return 1;
    }

    if (device->idVendor == DS_VID && (device->idProduct == DS4_PID || device->idProduct == DS4_PID_SLIM))
        return 1;

    return 0;
}

static int usb_connect(int devId)
{
    int pad, epCount;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbEndpointDescriptor *endpoint;

    DPRINTF("connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds4pad[pad].devId == -1 && ds4pad[pad].enabled)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(ds4pad[pad].sema);

    ds4pad[pad].devId = devId;

    ds4pad[pad].status = DS4USB_STATE_AUTHORIZED;

    ds4pad[pad].controlEndp = sceUsbdOpenPipe(devId, NULL);

    device = (UsbDeviceDescriptor *)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)sceUsbdScanStaticDescriptor(devId, device, USB_DT_CONFIG);

    ds4pad[pad].type = DS4;
    epCount = 20; // ds4 v2 returns interface->bNumEndpoints as 0

    endpoint = (UsbEndpointDescriptor *)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && ds4pad[pad].interruptEndp < 0) {
                ds4pad[pad].interruptEndp = sceUsbdOpenPipe(devId, endpoint);
                DPRINTF("register Event endpoint id =%i addr=%02X packetSize=%i\n", ds4pad[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && ds4pad[pad].outEndp < 0) {
                ds4pad[pad].outEndp = sceUsbdOpenPipe(devId, endpoint);
                DPRINTF("register Output endpoint id =%i addr=%02X packetSize=%i\n", ds4pad[pad].outEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (ds4pad[pad].interruptEndp < 0 || ds4pad[pad].outEndp < 0) {
        usb_release(pad);
        return 1;
    }

    ds4pad[pad].status |= DS4USB_STATE_CONNECTED;

    sceUsbdSetConfiguration(ds4pad[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);
    SignalSema(ds4pad[pad].sema);

    return 0;
}

static int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds4pad[pad].devId == devId)
            break;
    }

    if (pad < MAX_PADS) {
        usb_release(pad);
        pademu_disconnect(&padf[pad]);
    }

    return 0;
}

static void usb_release(int pad)
{
    PollSema(ds4pad[pad].sema);

    if (ds4pad[pad].interruptEndp >= 0)
        sceUsbdClosePipe(ds4pad[pad].interruptEndp);

    if (ds4pad[pad].outEndp >= 0)
        sceUsbdClosePipe(ds4pad[pad].outEndp);

    ds4pad[pad].controlEndp = -1;
    ds4pad[pad].interruptEndp = -1;
    ds4pad[pad].outEndp = -1;
    ds4pad[pad].devId = -1;
    ds4pad[pad].status = DS4USB_STATE_DISCONNECTED;

    SignalSema(ds4pad[pad].sema);
}

static int usb_resulCode;

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    DPRINTF("usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    usb_resulCode = resultCode;

    SignalSema(ds4pad[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    DPRINTF("usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(ds4pad[pad].cmd_sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;
    u8 led[4];

    PollSema(ds4pad[pad].sema);

    ds4pad[pad].status |= DS4USB_STATE_CONFIGURED;

    led[0] = rgbled_patterns[pad][1][0];
    led[1] = rgbled_patterns[pad][1][1];
    led[2] = rgbled_patterns[pad][1][2];
    led[3] = 0;

    LEDRumble(led, 0, 0, pad);

    ds4pad[pad].status |= DS4USB_STATE_RUNNING;

    SignalSema(ds4pad[pad].sema);

    pademu_connect(&padf[pad]);
}

#define MAX_DELAY 10

static void readReport(u8 *data, int pad_idx)
{
    ds4usb_device *pad = &ds4pad[pad_idx];
    if (pad->type == GUITAR_GH || pad->type == GUITAR_RB) {
        struct ds3guitarreport *report;

        report = (struct ds3guitarreport *)data;

        translate_pad_guitar(report, &pad->ds2, pad->type == GUITAR_GH);
        padMacroPerform(&pad->ds2, report->PSButton);
    }
    if (data[0]) {
        struct ds4report *report;
        report = (struct ds4report *)data;
        translate_pad_ds4(report, &pad->ds2, 1);
        padMacroPerform(&pad->ds2, report->PSButton);

        if (report->PSButton) {                                   // display battery level
            if (report->Share && (pad->btn_delay == MAX_DELAY)) { // PS + Share
                if (pad->analog_btn < 2)                          // unlocked mode
                    pad->analog_btn = !pad->analog_btn;

                pad->oldled[0] = rgbled_patterns[pad_idx][(pad->analog_btn & 1)][0];
                pad->oldled[1] = rgbled_patterns[pad_idx][(pad->analog_btn & 1)][1];
                pad->oldled[2] = rgbled_patterns[pad_idx][(pad->analog_btn & 1)][2];
                pad->btn_delay = 1;
            } else {
                pad->oldled[0] = report->Battery;
                pad->oldled[1] = 0;
                pad->oldled[2] = 0;

                if (pad->btn_delay < MAX_DELAY)
                    pad->btn_delay++;
            }
        } else {
            pad->oldled[0] = rgbled_patterns[pad_idx][(pad->analog_btn & 1)][0];
            pad->oldled[1] = rgbled_patterns[pad_idx][(pad->analog_btn & 1)][1];
            pad->oldled[2] = rgbled_patterns[pad_idx][(pad->analog_btn & 1)][2];

            if (pad->btn_delay > 0)
                pad->btn_delay--;
        }

        if (report->Power != 0xB && report->Usb_plugged) // charging
            pad->oldled[3] = 1;
        else
            pad->oldled[3] = 0;

        if (pad->btn_delay > 0) {
            pad->update_rum = 1;
        }
    }
}

static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad)
{
    int ret = 0;

    PollSema(ds4pad[pad].cmd_sema);

    mips_memset(usb_buf, 0, sizeof(usb_buf));

    usb_buf[0] = 0x05;
    usb_buf[1] = 0xFF;

    usb_buf[4] = rrum * 255; // ds4 has full control
    usb_buf[5] = lrum;

    usb_buf[6] = led[0]; // r
    usb_buf[7] = led[1]; // g
    usb_buf[8] = led[2]; // b

    if (led[3]) // means charging, so blink
    {
        usb_buf[9] = 0x80;  // Time to flash bright (255 = 2.5 seconds)
        usb_buf[10] = 0x80; // Time to flash dark (255 = 2.5 seconds)
    }

    ret = sceUsbdInterruptTransfer(ds4pad[pad].outEndp, usb_buf, 32, usb_cmd_cb, (void *)pad);

    ds4pad[pad].oldled[0] = led[0];
    ds4pad[pad].oldled[1] = led[1];
    ds4pad[pad].oldled[2] = led[2];
    ds4pad[pad].oldled[3] = led[3];

    return ret;
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

static void ds4usb_set_rumble(struct pad_funcs *pf, u8 lrum, u8 rrum)
{
    ds4usb_device *pad = pf->priv;
    WaitSema(pad->sema);

    if ((pad->lrum != lrum) || (pad->rrum != rrum)) {
        pad->lrum = lrum;
        pad->rrum = rrum;
        pad->update_rum = 1;
    }

    SignalSema(pad->sema);
}

static int ds4usb_get_data(struct pad_funcs *pf, u8 *dst, int size, int port)
{
    ds4usb_device *pad = pf->priv;
    int ret = 0;

    WaitSema(pad->sema);

    PollSema(pad->sema);

    ret = sceUsbdInterruptTransfer(pad->interruptEndp, usb_buf, MAX_BUFFER_SIZE, usb_data_cb, (void *)port);

    if (ret == USB_RC_OK) {
        TransferWait(pad->sema);
        if (!usb_resulCode)
            readReport(usb_buf, port);

        usb_resulCode = 1;
    } else {
        DPRINTF("ds4usb_get_data usb transfer error %d\n", ret);
    }

    mips_memcpy(dst, pad->data, size);
    ret = pad->analog_btn & 1;

    if (pad->update_rum) {
        ret = LEDRumble(pad->oldled, pad->lrum, pad->rrum, port);
        if (ret == USB_RC_OK)
            TransferWait(pad->cmd_sema);
        else
            DPRINTF("LEDRumble usb transfer error %d\n", ret);

        pad->update_rum = 0;
    }

    SignalSema(pad->sema);

    return ret;
}

static void ds4usb_set_mode(struct pad_funcs *pf, int mode, int lock)
{
    ds4usb_device *pad = pf->priv;
    WaitSema(pad->sema);
    if (lock == 3)
        pad->analog_btn = 3;
    else
        pad->analog_btn = mode;
    SignalSema(pad->sema);
}

void ds4usb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

static int ds4usb_get_status(struct pad_funcs *pf)
{
    ds4usb_device *pad = pf->priv;
    int ret;

    WaitSema(pad->sema);

    ret = pad->status;

    SignalSema(pad->sema);

    return ret;
}

static int ds4usb_get_model(struct pad_funcs *pf)
{
    ds4usb_device *pad = pf->priv;
    int ret;

    WaitSema(pad->sema);
    if (pad->type == GUITAR_GH || pad->type == GUITAR_RB) {
        ret = MODEL_GUITAR;
    } else {
        ret = MODEL_PS2;
    }
    SignalSema(pad->sema);

    return ret;
}

int ds4usb_init(u8 pads, u8 options)
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        ds4pad[pad].status = 0;
        ds4pad[pad].devId = -1;
        ds4pad[pad].oldled[0] = 0;
        ds4pad[pad].oldled[1] = 0;
        ds4pad[pad].oldled[2] = 0;
        ds4pad[pad].oldled[3] = 0;
        ds4pad[pad].lrum = 0;
        ds4pad[pad].rrum = 0;
        ds4pad[pad].update_rum = 1;
        ds4pad[pad].sema = -1;
        ds4pad[pad].cmd_sema = -1;
        ds4pad[pad].controlEndp = -1;
        ds4pad[pad].interruptEndp = -1;
        ds4pad[pad].enabled = (pads >> pad) & 1;
        ds4pad[pad].type = 0;

        ds4pad[pad].data[0] = 0xFF;
        ds4pad[pad].data[1] = 0xFF;
        ds4pad[pad].analog_btn = 0;

        mips_memset(&ds4pad[pad].data[2], 0x7F, 4);
        mips_memset(&ds4pad[pad].data[6], 0x00, 12);

        ds4pad[pad].sema = CreateMutex(IOP_MUTEX_UNLOCKED);
        ds4pad[pad].cmd_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

        if (ds4pad[pad].sema < 0 || ds4pad[pad].cmd_sema < 0) {
            DPRINTF("Failed to allocate I/O semaphore.\n");
            return 0;
        }
        padf[pad].priv = &ds4pad[pad];
        padf[pad].get_status = ds4usb_get_status;
        padf[pad].get_model = ds4usb_get_model;
        padf[pad].get_data = ds4usb_get_data;
        padf[pad].set_rumble = ds4usb_set_rumble;
        padf[pad].set_mode = ds4usb_set_mode;
    }

    if (sceUsbdRegisterLdd(&usb_driver) != USB_RC_OK) {
        DPRINTF("Error registering USB devices\n");
        return 0;
    }

    return 1;
}
