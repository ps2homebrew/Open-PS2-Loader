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
#include "ds3usb.h"
#include "padmacro.h"

#define MODNAME "DS3USB"

#ifdef DEBUG
#define DPRINTF(format, args...) \
    printf(MODNAME ": " format, ##args)
#else
#define DPRINTF(args...)
#endif

#define REQ_USB_OUT (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE)
#define REQ_USB_IN  (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE)

#define MAX_PADS 4

static u8 output_01_report[] =
    {
        0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x02,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00};

static u8 led_patterns[][2] =
    {
        {0x1C, 0x02},
        {0x1A, 0x04},
        {0x16, 0x08},
        {0x0E, 0x10},
};

static u8 power_level[] =
    {
        0x00, 0x00, 0x02, 0x06, 0x0E, 0x1E};

static u8 usb_buf[MAX_BUFFER_SIZE + 32] __attribute((aligned(4))) = {0};

static int usb_probe(int devId);
static int usb_connect(int devId);
static int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

static UsbDriver usb_driver = {NULL, NULL, "ds3usb", usb_probe, usb_connect, usb_disconnect};

static void DS3USB_init(int pad);
static void readReport(u8 *data, int pad);
static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad);

static int ds3usb_get_model(struct pad_funcs *pf, int port);
static int ds3usb_get_data(struct pad_funcs *pf, u8 *dst, int size, int port);
static void ds3usb_set_rumble(struct pad_funcs *pf, u8 lrum, u8 rrum);
static void ds3usb_set_mode(struct pad_funcs *pf, int mode, int lock);

static ds3usb_device ds3pad[MAX_PADS];
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

    if (device->idVendor == DS34_VID && (device->idProduct == DS3_PID))
        return 1;

    return 0;
}

static int usb_connect(int devId)
{
    int pad, epCount;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;
    UsbEndpointDescriptor *endpoint;

    DPRINTF("connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds3pad[pad].devId == -1 && ds3pad[pad].enabled)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(ds3pad[pad].sema);

    ds3pad[pad].devId = devId;

    ds3pad[pad].status = DS3USB_STATE_AUTHORIZED;

    ds3pad[pad].controlEndp = sceUsbdOpenPipe(devId, NULL);

    device = (UsbDeviceDescriptor *)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)sceUsbdScanStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    if (device->idProduct == DS3_PID) {
        ds3pad[pad].type = DS3;
        epCount = interface->bNumEndpoints - 1;
    } else if (device->idProduct == GUITAR_HERO_PS3_PID) {
        ds3pad[pad].type = GUITAR_GH;
        epCount = interface->bNumEndpoints - 1;
    } else if (device->idProduct == ROCK_BAND_PS3_PID) {
        ds3pad[pad].type = GUITAR_RB;
        epCount = interface->bNumEndpoints - 1;
    } else {
        DPRINTF("Unknown device");
    }

    endpoint = (UsbEndpointDescriptor *)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && ds3pad[pad].interruptEndp < 0) {
                ds3pad[pad].interruptEndp = sceUsbdOpenPipe(devId, endpoint);
                DPRINTF("register Event endpoint id =%i addr=%02X packetSize=%i\n", ds3pad[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && ds3pad[pad].outEndp < 0) {
                ds3pad[pad].outEndp = sceUsbdOpenPipe(devId, endpoint);
                DPRINTF("register Output endpoint id =%i addr=%02X packetSize=%i\n", ds3pad[pad].outEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (ds3pad[pad].interruptEndp < 0 || ds3pad[pad].outEndp < 0) {
        usb_release(pad);
        return 1;
    }

    ds3pad[pad].status |= DS3USB_STATE_CONNECTED;

    sceUsbdSetConfiguration(ds3pad[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);
    SignalSema(ds3pad[pad].sema);

    return 0;
}

static int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds3pad[pad].devId == devId)
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
    PollSema(ds3pad[pad].sema);

    if (ds3pad[pad].interruptEndp >= 0)
        sceUsbdClosePipe(ds3pad[pad].interruptEndp);

    if (ds3pad[pad].outEndp >= 0)
        sceUsbdClosePipe(ds3pad[pad].outEndp);

    ds3pad[pad].controlEndp = -1;
    ds3pad[pad].interruptEndp = -1;
    ds3pad[pad].outEndp = -1;
    ds3pad[pad].devId = -1;
    ds3pad[pad].status = DS3USB_STATE_DISCONNECTED;

    SignalSema(ds3pad[pad].sema);
}

static int usb_resulCode;

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    DPRINTF("usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    usb_resulCode = resultCode;

    SignalSema(ds3pad[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    DPRINTF("usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(ds3pad[pad].cmd_sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;
    u8 led[4];

    PollSema(ds3pad[pad].sema);

    ds3pad[pad].status |= DS3USB_STATE_CONFIGURED;

    DS3USB_init(pad);
    DelayThread(10000);
    led[0] = led_patterns[pad][1];
    led[3] = 0;

    LEDRumble(led, 0, 0, pad);

    ds3pad[pad].status |= DS3USB_STATE_RUNNING;

    SignalSema(ds3pad[pad].sema);

    pademu_connect(&padf[pad]);
}

static void DS3USB_init(int pad)
{
    usb_buf[0] = 0x42;
    usb_buf[1] = 0x0c;
    usb_buf[2] = 0x00;
    usb_buf[3] = 0x00;

    sceUsbdControlTransfer(ds3pad[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0xF4, 0, 4, usb_buf, NULL, NULL);
}

#define MAX_DELAY 10

static void readReport(u8 *data, int pad_idx)
{
    ds3usb_device *pad = &ds3pad[pad_idx];
    if (pad->type == GUITAR_GH || pad->type == GUITAR_RB) {
        struct ds3guitarreport *report;

        report = (struct ds3guitarreport *)data;

        translate_pad_guitar(report, &pad->ds2, pad->type == GUITAR_GH);
        padMacroPerform(&pad->ds2, report->PSButton);
    }
    if (data[0]) {
        struct ds3report *report;

        report = (struct ds3report *)&data[2];

        if (report->RightStickX == 0 && report->RightStickY == 0) // ledrumble cmd causes null report sometime
            return;

        pad->data[0] = ~report->ButtonStateL;
        pad->data[1] = ~report->ButtonStateH;

        translate_pad_ds3(report, &pad->ds2, 0);
        padMacroPerform(&pad->ds2, report->PSButton);
        if (report->PSButton) {                                    // display battery level
            if (report->Select && (pad->btn_delay == MAX_DELAY)) { // PS + SELECT
                if (pad->analog_btn < 2)                           // unlocked mode
                    pad->analog_btn = !pad->analog_btn;

                pad->oldled[0] = led_patterns[pad_idx][(pad->analog_btn & 1)];
                pad->btn_delay = 1;
            } else {
                if (report->Power <= 0x05)
                    pad->oldled[0] = power_level[report->Power];

                if (pad->btn_delay < MAX_DELAY)
                    pad->btn_delay++;
            }
        } else {
            pad->oldled[0] = led_patterns[pad_idx][(pad->analog_btn & 1)];

            if (pad->btn_delay > 0)
                pad->btn_delay--;
        }

        if (report->Power == 0xEE) // charging
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

    PollSema(ds3pad[pad].cmd_sema);

    mips_memset(usb_buf, 0, sizeof(usb_buf));

    mips_memcpy(usb_buf, output_01_report, sizeof(output_01_report));

    usb_buf[1] = 0xFE; // rt
    usb_buf[2] = rrum; // rp
    usb_buf[3] = 0xFE; // lt
    usb_buf[4] = lrum; // lp

    usb_buf[9] = led[0] & 0x7F; // LED Conf

    if (led[3]) // means charging, so blink
    {
        usb_buf[13] = 0x32;
        usb_buf[18] = 0x32;
        usb_buf[23] = 0x32;
        usb_buf[28] = 0x32;
    }

    ret = sceUsbdControlTransfer(ds3pad[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_SET_REPORT_OUTPUT << 8) | 0x01, 0, sizeof(output_01_report), usb_buf, usb_cmd_cb, (void *)pad);


    ds3pad[pad].oldled[0] = led[0];
    ds3pad[pad].oldled[1] = led[1];
    ds3pad[pad].oldled[2] = led[2];
    ds3pad[pad].oldled[3] = led[3];

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

static void ds3usb_set_rumble(struct pad_funcs *pf, u8 lrum, u8 rrum)
{
    ds3usb_device *pad = pf->priv;
    WaitSema(pad->sema);

    if ((pad->lrum != lrum) || (pad->rrum != rrum)) {
        pad->lrum = lrum;
        pad->rrum = rrum;
        pad->update_rum = 1;
    }

    SignalSema(pad->sema);
}

static int ds3usb_get_data(struct pad_funcs *pf, u8 *dst, int size, int port)
{
    ds3usb_device *pad = pf->priv;
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
        DPRINTF("ds3usb_get_data usb transfer error %d\n", ret);
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

static void ds3usb_set_mode(struct pad_funcs *pf, int mode, int lock)
{
    ds3usb_device *pad = pf->priv;
    WaitSema(pad->sema);
    if (lock == 3)
        pad->analog_btn = 3;
    else
        pad->analog_btn = mode;
    SignalSema(pad->sema);
}

void ds3usb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

static int ds3usb_get_status(struct pad_funcs *pf)
{
    ds3usb_device *pad = pf->priv;
    int ret;

    WaitSema(pad->sema);

    ret = pad->status;

    SignalSema(pad->sema);

    return ret;
}

static int ds3usb_get_model(struct pad_funcs *pf, int port)
{
    (void)port;
    ds3usb_device *pad = pf->priv;
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

int ds3usb_init(u8 pads, u8 options)
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        ds3pad[pad].status = 0;
        ds3pad[pad].devId = -1;
        ds3pad[pad].oldled[0] = 0;
        ds3pad[pad].oldled[1] = 0;
        ds3pad[pad].oldled[2] = 0;
        ds3pad[pad].oldled[3] = 0;
        ds3pad[pad].lrum = 0;
        ds3pad[pad].rrum = 0;
        ds3pad[pad].update_rum = 1;
        ds3pad[pad].sema = -1;
        ds3pad[pad].cmd_sema = -1;
        ds3pad[pad].controlEndp = -1;
        ds3pad[pad].interruptEndp = -1;
        ds3pad[pad].enabled = (pads >> pad) & 1;
        ds3pad[pad].type = 0;

        ds3pad[pad].data[0] = 0xFF;
        ds3pad[pad].data[1] = 0xFF;
        ds3pad[pad].analog_btn = 0;

        mips_memset(&ds3pad[pad].data[2], 0x7F, 4);
        mips_memset(&ds3pad[pad].data[6], 0x00, 12);

        ds3pad[pad].sema = CreateMutex(IOP_MUTEX_UNLOCKED);
        ds3pad[pad].cmd_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

        if (ds3pad[pad].sema < 0 || ds3pad[pad].cmd_sema < 0) {
            DPRINTF("Failed to allocate I/O semaphore.\n");
            return 0;
        }
        padf[pad].priv = &ds3pad[pad];
        padf[pad].get_status = ds3usb_get_status;
        padf[pad].get_model = ds3usb_get_model;
        padf[pad].get_data = ds3usb_get_data;
        padf[pad].set_rumble = ds3usb_set_rumble;
        padf[pad].set_mode = ds3usb_set_mode;
    }

    if (sceUsbdRegisterLdd(&usb_driver) != USB_RC_OK) {
        DPRINTF("Error registering USB devices\n");
        return 0;
    }

    return 1;
}
