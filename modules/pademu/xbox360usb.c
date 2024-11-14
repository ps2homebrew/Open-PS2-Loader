#include "types.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "thsemap.h"
#include "xbox360usb.h"
#include "padmacro.h"
#include "pademu.h"
#include "ds34.h"

#define MODNAME "XBOX360USB: "

#ifdef DEBUG
#define DPRINTF(format, args...) \
    printf(MODNAME ": " format, ##args)
#else
#define DPRINTF(args...)
#endif

#define REQ_USB_OUT (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE)
#define REQ_USB_IN  (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE)

#define MAX_PADS 4

static u8 usb_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};

static int usb_probe(int devId);
static int usb_connect(int devId);
static int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

static UsbDriver usb_driver = {NULL, NULL, "xbox360usb", usb_probe, usb_connect, usb_disconnect};

static void readReport(u8 *data, int pad);
static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad);

xbox360usb_device xbox360dev[MAX_PADS];
static struct pad_funcs padf[MAX_PADS];

static int xbox360usb_get_model(struct pad_funcs *pf, int port);
static int xbox360usb_get_data(struct pad_funcs *pf, u8 *dst, int size, int port);
static void xbox360usb_set_rumble(struct pad_funcs *pf, u8 lrum, u8 rrum);
static void xbox360usb_set_mode(struct pad_funcs *pf, int mode, int lock);
static int xbox360usb_get_status(struct pad_funcs *pf);

static int usb_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("Error - Couldn't get device descriptor\n");
        return 0;
    }

    if ((device->idVendor == XBOX_VID || device->idVendor == MADCATZ_VID || device->idVendor == JOYTECH_VID || device->idVendor == GAMESTOP_VID) &&
        (device->idProduct == XBOX_WIRED_PID || device->idProduct == MADCATZ_WIRED_PID || device->idProduct == GAMESTOP_WIRED_PID ||
         device->idProduct == AFTERGLOW_WIRED_PID || device->idProduct == JOYTECH_WIRED_PID))
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
        if (xbox360dev[pad].usb_id == -1)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(xbox360dev[pad].sema);

    xbox360dev[pad].usb_id = devId;
    xbox360dev[pad].controlEndp = sceUsbdOpenPipe(devId, NULL);

    device = (UsbDeviceDescriptor *)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)sceUsbdScanStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);
    epCount = interface->bNumEndpoints - 1;
    endpoint = (UsbEndpointDescriptor *)sceUsbdScanStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && xbox360dev[pad].interruptEndp < 0) {
                xbox360dev[pad].interruptEndp = sceUsbdOpenPipeAligned(devId, endpoint);
                DPRINTF("register Event endpoint id =%i addr=%02X packetSize=%i\n", xbox360dev[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (xbox360dev[pad].interruptEndp < 0) {
        usb_release(pad);
        return 1;
    }

    sceUsbdSetConfiguration(xbox360dev[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);
    SignalSema(xbox360dev[pad].sema);

    return 0;
}

static int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (xbox360dev[pad].usb_id == devId) {
            break;
        }
    }

    if (pad < MAX_PADS) {
        usb_release(pad);
        pademu_disconnect(&padf[pad]);
    }
    return 0;
}

static void usb_release(int pad)
{
    PollSema(xbox360dev[pad].sema);

    if (xbox360dev[pad].interruptEndp >= 0)
        sceUsbdClosePipe(xbox360dev[pad].interruptEndp);

    xbox360dev[pad].controlEndp = -1;
    xbox360dev[pad].interruptEndp = -1;
    xbox360dev[pad].usb_id = -1;

    SignalSema(xbox360dev[pad].sema);
}

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    DPRINTF("usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    xbox360dev[pad].usb_resultcode = resultCode;

    SignalSema(xbox360dev[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    DPRINTF("usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(xbox360dev[pad].cmd_sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;
    u8 led[2];

    PollSema(xbox360dev[pad].sema);

    led[0] = 0x02 + pad;
    led[1] = 0;

    LEDRumble(led, 0, 0, pad);
    SignalSema(xbox360dev[pad].sema);

    pademu_connect(&padf[pad]);
}

#define MAX_DELAY 10

static void readReport(u8 *data, int pad)
{
    xbox360report_t *report = (xbox360report_t *)data;
    if (report->ReportID == 0x00 && report->Length == 0x14) {
        xbox360dev[pad].data[0] = ~(report->Back | report->LS << 1 | report->RS << 2 | report->Start << 3 | report->Up << 4 | report->Right << 5 | report->Down << 6 | report->Left << 7);
        xbox360dev[pad].data[1] = ~((report->LeftTrigger != 0) | (report->RightTrigger != 0) << 1 | report->LB << 2 | report->RB << 3 | report->Y << 4 | report->B << 5 | report->A << 6 | report->X << 7);

        xbox360dev[pad].data[2] = report->RightStickXH + 128;    // rx
        xbox360dev[pad].data[3] = ~(report->RightStickYH + 128); // ry
        xbox360dev[pad].data[4] = report->LeftStickXH + 128;     // lx
        xbox360dev[pad].data[5] = ~(report->LeftStickYH + 128);  // ly

        xbox360dev[pad].data[6] = report->Right * 255; // right
        xbox360dev[pad].data[7] = report->Left * 255;  // left
        xbox360dev[pad].data[8] = report->Up * 255;    // up
        xbox360dev[pad].data[9] = report->Down * 255;  // down

        xbox360dev[pad].data[10] = report->Y * 255; // triangle
        xbox360dev[pad].data[11] = report->B * 255; // circle
        xbox360dev[pad].data[12] = report->A * 255; // cross
        xbox360dev[pad].data[13] = report->X * 255; // square

        xbox360dev[pad].data[14] = report->LB * 255;     // L1
        xbox360dev[pad].data[15] = report->RB * 255;     // R1
        xbox360dev[pad].data[16] = report->LeftTrigger;  // L2
        xbox360dev[pad].data[17] = report->RightTrigger; // R2

        if (report->XBOX) {                                                 // display battery level
            if (report->Back && (xbox360dev[pad].btn_delay == MAX_DELAY)) { // XBOX + BACK
                if (xbox360dev[pad].analog_btn < 2)                         // unlocked mode
                    xbox360dev[pad].analog_btn = !xbox360dev[pad].analog_btn;

                // xbox360dev[pad].oldled[0] = led_patterns[pad][(xbox360dev[pad].analog_btn & 1)];
                xbox360dev[pad].btn_delay = 1;
            } else {
                // if (report->Power != 0xEE)
                //     xbox360dev[pad].oldled[0] = power_level[report->Power];

                if (xbox360dev[pad].btn_delay < MAX_DELAY)
                    xbox360dev[pad].btn_delay++;
            }
        } else {
            // xbox360dev[pad].oldled[0] = led_patterns[pad][(xbox360dev[pad].analog_btn & 1)];

            if (xbox360dev[pad].btn_delay > 0)
                xbox360dev[pad].btn_delay--;
        }
    }
}

static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad)
{
    int ret;
    PollSema(xbox360dev[pad].cmd_sema);

    usb_buf[0] = 0x00;
    usb_buf[1] = 0x08;
    usb_buf[2] = 0x00;
    usb_buf[3] = lrum; // big weight
    usb_buf[4] = rrum; // small weight
    usb_buf[5] = 0x00;
    usb_buf[6] = 0x00;
    usb_buf[7] = 0x00;

    ret = sceUsbdControlTransfer(xbox360dev[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_SET_REPORT_OUTPUT << 8) | 0x01, 0, 8, usb_buf, usb_cmd_cb, (void *)pad);
    if (ret == USB_RC_OK) {
        usb_buf[0] = 0x01;
        usb_buf[1] = 0x03;
        usb_buf[2] = led[0];
        ret = sceUsbdControlTransfer(xbox360dev[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_SET_REPORT_OUTPUT << 8) | 0x01, 0, 3, usb_buf, usb_cmd_cb, (void *)pad);
    }

    xbox360dev[pad].oldled[0] = led[0];
    xbox360dev[pad].oldled[1] = led[1];

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

static void xbox360usb_set_rumble(struct pad_funcs *pf, u8 lrum, u8 rrum)
{
    xbox360usb_device *pad = pf->priv;
    WaitSema(pad->sema);

    pad->update_rum = 1;
    pad->lrum = lrum;
    pad->rrum = rrum;

    SignalSema(pad->sema);
}

static int xbox360usb_get_data(struct pad_funcs *pf, u8 *dst, int size, int port)
{
    xbox360usb_device *pad = pf->priv;
    int ret = 0;

    WaitSema(pad->sema);

    PollSema(pad->sema);

    ret = sceUsbdInterruptTransfer(pad->interruptEndp, usb_buf, MAX_BUFFER_SIZE, usb_data_cb, (void *)port);

    if (ret == USB_RC_OK) {
        TransferWait(pad->sema);
        if (!pad->usb_resultcode)
            readReport(usb_buf, port);

        pad->usb_resultcode = 1;
    } else {
        DPRINTF("XBOX360USB_get_data usb transfer error %d\n", ret);
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

static void xbox360usb_set_mode(struct pad_funcs *pf, int mode, int lock)
{
    xbox360usb_device *pad = pf->priv;
    if (lock == 3)
        pad->analog_btn = 3;
    else
        pad->analog_btn = mode;
}

static int xbox360usb_get_status(struct pad_funcs *pf)
{
    (void)pf;

    return 0;
}

static int xbox360usb_get_model(struct pad_funcs *pf, int port)
{
    (void)port;
    (void)pf;

    return 0;
}

void xbox360usb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

int xbox360usb_init(u8 pads, u8 options)
{
    int pad = 0;

    for (pad = 0; pad < MAX_PADS; pad++) {
        xbox360dev[pad].usb_id = -1;

        xbox360dev[pad].oldled[0] = 0;
        xbox360dev[pad].oldled[1] = 0;
        xbox360dev[pad].lrum = 0;
        xbox360dev[pad].rrum = 0;
        xbox360dev[pad].update_rum = 1;
        xbox360dev[pad].sema = -1;
        xbox360dev[pad].cmd_sema = -1;
        xbox360dev[pad].controlEndp = -1;
        xbox360dev[pad].interruptEndp = -1;

        xbox360dev[pad].data[0] = 0xFF;
        xbox360dev[pad].data[1] = 0xFF;
        xbox360dev[pad].analog_btn = 0;

        mips_memset(&xbox360dev[pad].data[2], 0x7F, 4);
        mips_memset(&xbox360dev[pad].data[6], 0x00, 12);

        xbox360dev[pad].sema = CreateMutex(IOP_MUTEX_UNLOCKED);
        xbox360dev[pad].cmd_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

        if (xbox360dev[pad].sema < 0 || xbox360dev[pad].cmd_sema < 0) {
            DPRINTF("Failed to allocate I/O semaphore.\n");
            return MODULE_NO_RESIDENT_END;
        }
        padf[pad].priv = &xbox360dev[pad];
        padf[pad].get_status = xbox360usb_get_status;
        padf[pad].get_model = xbox360usb_get_model;
        padf[pad].get_data = xbox360usb_get_data;
        padf[pad].set_rumble = xbox360usb_set_rumble;
        padf[pad].set_mode = xbox360usb_set_mode;
    }

    if (sceUsbdRegisterLdd(&usb_driver) != USB_RC_OK) {
        DPRINTF("Error registering USB devices\n");
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}
