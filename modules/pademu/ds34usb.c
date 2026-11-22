#include "types.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "thsemap.h"
#include "ds34usb.h"
#include "sys_utils.h"
#include "padmacro.h"

// #define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

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
#ifdef X360
static void TransferWait(int sema);
#endif

UsbDriver usb_driver = {NULL, NULL, "ds34usb", usb_probe, usb_connect, usb_disconnect};

static void DS3USB_init(int pad);
static void readReport(u8 *data, int pad);
static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad);

ds34usb_device ds34pad[MAX_PADS];

#ifdef X360
// Xbox 360 (XInput) controllers expose a vendor specific interface with class/subclass/protocol FF/5D/01.
// Wireless receivers (protocol 0x81) are not matched.
static int is_x360_xinput(int devId, UsbDeviceDescriptor *device)
{
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;

    // 8BitDo Ultimate 2C 2.4G dongle (known to work in the OPL menu driver): match by VID/PID, no descriptor parsing needed
    if (device->idVendor == 0x2DC8 && device->idProduct == 0x310A)
        return 1;

    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (config == NULL)
        return 0;

    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    return interface->bDescriptorType == USB_DT_INTERFACE && interface->bInterfaceClass == 0xFF && interface->bInterfaceSubClass == 0x5D && interface->bInterfaceProtocol == 0x01;
}

// XInput pads only send a report when something changes, so one interrupt transfer can stay pending for a long
// time. The PS2 usbd refuses a new request on the same endpoint while one is pending (USB_RC_IOREQ), so keep a
// single request in flight per pad and only resubmit it after it completes.
static u8 x360_buf[MAX_PADS][32] __attribute((aligned(4)));
static volatile int x360_pending[MAX_PADS];
// The data callback stores each report here and immediately queues the next read, so the pad is always being
// listened to. A sequence counter (odd while being written) lets the reader take a consistent copy without locking.
static u8 x360_latest[MAX_PADS][20];
static volatile unsigned int x360_seq[MAX_PADS];
static unsigned int x360_seen[MAX_PADS];

// Rumble is sent asynchronously and only when the wanted value changes, so reading the pad never blocks on it.
// (The PS2 usbd also refuses a second request on the same endpoint while one is pending, hence the pending flag.)
static u8 x360_out_buf[MAX_PADS][8] __attribute((aligned(4)));
static volatile int x360_out_pending[MAX_PADS];
static int x360_out_age[MAX_PADS];
static u8 x360_sent_l[MAX_PADS];
static u8 x360_sent_r[MAX_PADS];
#endif

int usb_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("DS34USB: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("DS34USB: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->idVendor == SONY_VID && (device->idProduct == GUITAR_HERO_PS3_PID || device->idProduct == ROCK_BAND_PS3_PID)) {
        return 1;
    }

    if (device->idVendor == DS34_VID && (device->idProduct == DS3_PID || device->idProduct == DS4_PID || device->idProduct == DS4_PID_SLIM))
        return 1;

#ifdef X360
    if (is_x360_xinput(devId, device))
        return 1;
#endif

    return 0;
}

int usb_connect(int devId)
{
    int pad, epCount, is_x360;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;
    UsbEndpointDescriptor *endpoint;

    DPRINTF("DS34USB: connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds34pad[pad].devId == -1 && ds34pad[pad].enabled)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("DS34USB: Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(ds34pad[pad].sema);

    ds34pad[pad].devId = devId;

    ds34pad[pad].status = DS34USB_STATE_AUTHORIZED;

    ds34pad[pad].controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    is_x360 = 0;
#ifdef X360
    x360_pending[pad] = 0;
    x360_seq[pad] = 0;
    x360_seen[pad] = 0;
    x360_out_pending[pad] = 0;
    x360_out_age[pad] = 0;
    x360_sent_l[pad] = 0;
    x360_sent_r[pad] = 0;

    is_x360 = is_x360_xinput(devId, device);
#endif

    if (is_x360) {
        ds34pad[pad].type = X360_XINPUT_USB;
        epCount = interface->bNumEndpoints - 1; // only interface 0 (the pad) is used
    } else if (device->idProduct == DS3_PID) {
        ds34pad[pad].type = DS3;
        epCount = interface->bNumEndpoints - 1;
    } else if (device->idProduct == GUITAR_HERO_PS3_PID) {
        ds34pad[pad].type = GUITAR_GH;
        epCount = interface->bNumEndpoints - 1;
    } else if (device->idProduct == ROCK_BAND_PS3_PID) {
        ds34pad[pad].type = GUITAR_RB;
        epCount = interface->bNumEndpoints - 1;
    } else {
        ds34pad[pad].type = DS4;
        epCount = 20; // ds4 v2 returns interface->bNumEndpoints as 0
    }

    endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && ds34pad[pad].interruptEndp < 0) {
                ds34pad[pad].interruptEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("DS34USB: register Event endpoint id =%i addr=%02X packetSize=%i\n", ds34pad[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && ds34pad[pad].outEndp < 0) {
                ds34pad[pad].outEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("DS34USB: register Output endpoint id =%i addr=%02X packetSize=%i\n", ds34pad[pad].outEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (ds34pad[pad].interruptEndp < 0 || ds34pad[pad].outEndp < 0) {
        usb_release(pad);
        return 1;
    }

    ds34pad[pad].status |= DS34USB_STATE_CONNECTED;

    UsbSetDeviceConfiguration(ds34pad[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);
    SignalSema(ds34pad[pad].sema);

    return 0;
}

int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("DS34USB: disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds34pad[pad].devId == devId)
            break;
    }

    if (pad < MAX_PADS)
        usb_release(pad);

    return 0;
}

static void usb_release(int pad)
{
    PollSema(ds34pad[pad].sema);

    if (ds34pad[pad].interruptEndp >= 0)
        UsbCloseEndpoint(ds34pad[pad].interruptEndp);

    if (ds34pad[pad].outEndp >= 0)
        UsbCloseEndpoint(ds34pad[pad].outEndp);

    ds34pad[pad].controlEndp = -1;
    ds34pad[pad].interruptEndp = -1;
    ds34pad[pad].outEndp = -1;
    ds34pad[pad].devId = -1;
    ds34pad[pad].status = DS34USB_STATE_DISCONNECTED;
#ifdef X360
    x360_pending[pad] = 0;
    x360_out_pending[pad] = 0;
#endif

    SignalSema(ds34pad[pad].sema);
}

static int usb_resulCode;

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    // DPRINTF("DS34USB: usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    usb_resulCode = resultCode;

    SignalSema(ds34pad[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    // DPRINTF("DS34USB: usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(ds34pad[pad].cmd_sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;
    u8 led[4];

    PollSema(ds34pad[pad].sema);

    ds34pad[pad].status |= DS34USB_STATE_CONFIGURED;

    if (ds34pad[pad].type == DS3) {
        DS3USB_init(pad);
        DelayThread(10000);
        led[0] = led_patterns[pad][1];
        led[3] = 0;
    } else if (ds34pad[pad].type == DS4) {
        led[0] = rgbled_patterns[pad][1][0];
        led[1] = rgbled_patterns[pad][1][1];
        led[2] = rgbled_patterns[pad][1][2];
        led[3] = 0;
    }

#ifdef X360
    if (ds34pad[pad].type == X360_XINPUT_USB) {
        // Same packet the Linux xpad driver sends to Xbox 360 pads: set LED pattern. Some pads (8BitDo dongle)
        // do not send any report before receiving it.
        static u8 x360_led[4] __attribute((aligned(4))) = {0x01, 0x03, 0x06, 0x00};

        PollSema(ds34pad[pad].cmd_sema);
        if (UsbInterruptTransfer(ds34pad[pad].outEndp, x360_led, 3, usb_cmd_cb, (void *)pad) == USB_RC_OK)
            TransferWait(ds34pad[pad].cmd_sema);
    } else
#endif
    {
        LEDRumble(led, 0, 0, pad);
    }

    ds34pad[pad].status |= DS34USB_STATE_RUNNING;

    SignalSema(ds34pad[pad].sema);
}

static void DS3USB_init(int pad)
{
    usb_buf[0] = 0x42;
    usb_buf[1] = 0x0c;
    usb_buf[2] = 0x00;
    usb_buf[3] = 0x00;

    UsbControlTransfer(ds34pad[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0xF4, 0, 4, usb_buf, NULL, NULL);
}

#define MAX_DELAY 10

static void readReport(u8 *data, int pad_idx)
{
    ds34usb_device *pad = &ds34pad[pad_idx];
    if (pad->type == GUITAR_GH || pad->type == GUITAR_RB) {
        struct ds3guitarreport *report;

        report = (struct ds3guitarreport *)data;

        translate_pad_guitar(report, &pad->ds2, pad->type == GUITAR_GH);
        padMacroPerform(&pad->ds2, report->PSButton);
    }
#ifdef X360
    if (pad->type == X360_XINPUT_USB) {
        struct x360report *report = (struct x360report *)data;

        if (report->Type != 0x00 || report->Length != 0x14)
            return;

        translate_pad_x360(report, &pad->ds2);
        padMacroPerform(&pad->ds2, report->Guide);

        // Guide + Back toggles analog mode (like PS + Select on the DS3)
        if (report->Guide) {
            if (report->Back && (pad->btn_delay == MAX_DELAY)) {
                if (pad->analog_btn < 2) // unlocked mode
                    pad->analog_btn = !pad->analog_btn;

                pad->btn_delay = 1;
            } else if (pad->btn_delay < MAX_DELAY) {
                pad->btn_delay++;
            }
        } else if (pad->btn_delay > 0) {
            pad->btn_delay--;
        }

        return;
    }
#endif

    if (data[0]) {

        if (pad->type == DS3) {
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

        } else if (pad->type == DS4) {
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
        }
        if (pad->btn_delay > 0) {
            pad->update_rum = 1;
        }
    }
}

static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad)
{
    int ret = 0;

    PollSema(ds34pad[pad].cmd_sema);

    mips_memset(usb_buf, 0, sizeof(usb_buf));

    if (ds34pad[pad].type == DS3) {
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

        ret = UsbControlTransfer(ds34pad[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_SET_REPORT_OUTPUT << 8) | 0x01, 0, sizeof(output_01_report), usb_buf, usb_cmd_cb, (void *)pad);
    } else if (ds34pad[pad].type == DS4) {
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

        ret = UsbInterruptTransfer(ds34pad[pad].outEndp, usb_buf, 32, usb_cmd_cb, (void *)pad);
    }
#ifdef X360
    else if (ds34pad[pad].type == X360_XINPUT_USB) {
        usb_buf[0] = 0x00;
        usb_buf[1] = 0x08; // rumble packet length
        usb_buf[2] = 0x00;
        usb_buf[3] = lrum;       // large motor
        usb_buf[4] = rrum * 255; // small motor (on/off on the PS2)

        ret = UsbInterruptTransfer(ds34pad[pad].outEndp, usb_buf, 8, usb_cmd_cb, (void *)pad);
    }
#endif

    ds34pad[pad].oldled[0] = led[0];
    ds34pad[pad].oldled[1] = led[1];
    ds34pad[pad].oldled[2] = led[2];
    ds34pad[pad].oldled[3] = led[3];

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

void ds34usb_set_rumble(u8 lrum, u8 rrum, int port)
{
    WaitSema(ds34pad[port].sema);

    ds34pad[port].update_rum = 1;
    ds34pad[port].lrum = lrum;
    ds34pad[port].rrum = rrum;

    SignalSema(ds34pad[port].sema);
}

#ifdef X360
// No SignalSema here: reading never waits for these transfers, so the pad semaphore must not be touched.
static void x360_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    if (resultCode == USB_RC_OK) {
        x360_seq[pad]++; // odd: being written
        mips_memcpy(x360_latest[pad], x360_buf[pad], 20);
        x360_seq[pad]++; // even: consistent

        // keep listening: queue the next read right away instead of waiting for the next pad poll
        if (ds34pad[pad].interruptEndp >= 0 && UsbInterruptTransfer(ds34pad[pad].interruptEndp, x360_buf[pad], 32, x360_data_cb, arg) == USB_RC_OK)
            return;
    }

    // on an error the read is restarted by the next pad poll (this also avoids spinning on a dead endpoint)
    x360_pending[pad] = 0;
}

static void x360_out_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    x360_out_pending[pad] = 0;
}

// Send the wanted rumble state if it differs from the last one that was sent (never blocks).
static void x360_update_rumble(int pad)
{
    u8 l = ds34pad[pad].lrum;       // large motor
    u8 r = ds34pad[pad].rrum * 255; // small motor (on/off on the PS2)

    if (x360_out_pending[pad]) {
        // a lost completion must not block rumble forever (this also guarantees that "stop" is eventually sent)
        if (++x360_out_age[pad] < 120)
            return;

        x360_out_pending[pad] = 0;
    }

    if (l == x360_sent_l[pad] && r == x360_sent_r[pad])
        return;

    x360_out_buf[pad][0] = 0x00;
    x360_out_buf[pad][1] = 0x08; // rumble packet length
    x360_out_buf[pad][2] = 0x00;
    x360_out_buf[pad][3] = l;
    x360_out_buf[pad][4] = r;
    x360_out_buf[pad][5] = 0x00;
    x360_out_buf[pad][6] = 0x00;
    x360_out_buf[pad][7] = 0x00;

    x360_out_pending[pad] = 1;
    x360_out_age[pad] = 0;

    if (UsbInterruptTransfer(ds34pad[pad].outEndp, x360_out_buf[pad], 8, x360_out_cb, (void *)pad) == USB_RC_OK) {
        x360_sent_l[pad] = l;
        x360_sent_r[pad] = r;
    } else {
        x360_out_pending[pad] = 0; // try again on the next poll
    }
}
#endif

int ds34usb_get_data(u8 *dst, int size, int port)
{
    int ret = 0;

    WaitSema(ds34pad[port].sema);

    PollSema(ds34pad[port].sema);

#ifdef X360
    if (ds34pad[port].type == X360_XINPUT_USB) {
        if (!x360_pending[port]) {
            x360_pending[port] = 1;
            if (UsbInterruptTransfer(ds34pad[port].interruptEndp, x360_buf[port], 32, x360_data_cb, (void *)port) != USB_RC_OK)
                x360_pending[port] = 0;
        }

        // never wait here: take the latest stored report (a few tries in case the callback is writing it)
        {
            u8 report[20];
            unsigned int seq1, seq2;
            int tries;

            for (tries = 0; tries < 4; tries++) {
                seq1 = x360_seq[port];
                mips_memcpy(report, x360_latest[port], 20);
                seq2 = x360_seq[port];

                if (seq1 == seq2 && !(seq1 & 1)) {
                    if (seq1 != x360_seen[port]) {
                        x360_seen[port] = seq1;
                        readReport(report, port);
                    }
                    break;
                }
            }
        }

        x360_update_rumble(port);
        ds34pad[port].update_rum = 0;
    } else
#endif
    {
        ret = UsbInterruptTransfer(ds34pad[port].interruptEndp, usb_buf, MAX_BUFFER_SIZE, usb_data_cb, (void *)port);

        if (ret == USB_RC_OK) {
            TransferWait(ds34pad[port].sema);
            if (!usb_resulCode)
                readReport(usb_buf, port);

            usb_resulCode = 1;
        } else {
            DPRINTF("DS34USB: ds34usb_get_data usb transfer error %d\n", ret);
        }
    }

    mips_memcpy(dst, ds34pad[port].data, size);
    ret = ds34pad[port].analog_btn & 1;

    if (ds34pad[port].update_rum) {
        ret = LEDRumble(ds34pad[port].oldled, ds34pad[port].lrum, ds34pad[port].rrum, port);
        if (ret == USB_RC_OK)
            TransferWait(ds34pad[port].cmd_sema);
        else
            DPRINTF("DS34USB: LEDRumble usb transfer error %d\n", ret);

        ds34pad[port].update_rum = 0;
    }

    SignalSema(ds34pad[port].sema);

    return ret;
}

void ds34usb_set_mode(int mode, int lock, int port)
{
    if (lock == 3)
        ds34pad[port].analog_btn = 3;
    else
        ds34pad[port].analog_btn = mode;
}

void ds34usb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

int ds34usb_get_status(int port)
{
    int ret;

    WaitSema(ds34pad[port].sema);
    ret = ds34pad[port].status;
    SignalSema(ds34pad[port].sema);

    return ret;
}

int ds34usb_get_model(int port)
{
    int ret;

    WaitSema(ds34pad[port].sema);
    if (ds34pad[port].type == GUITAR_GH || ds34pad[port].type == GUITAR_RB) {
        ret = MODEL_GUITAR;
    } else {
        ret = MODEL_PS2;
    }
    SignalSema(ds34pad[port].sema);

    return ret;
}

int ds34usb_init(u8 pads, u8 options)
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        ds34pad[pad].status = 0;
        ds34pad[pad].devId = -1;
        ds34pad[pad].oldled[0] = 0;
        ds34pad[pad].oldled[1] = 0;
        ds34pad[pad].oldled[2] = 0;
        ds34pad[pad].oldled[3] = 0;
        ds34pad[pad].lrum = 0;
        ds34pad[pad].rrum = 0;
        ds34pad[pad].update_rum = 1;
        ds34pad[pad].sema = -1;
        ds34pad[pad].cmd_sema = -1;
        ds34pad[pad].controlEndp = -1;
        ds34pad[pad].interruptEndp = -1;
        ds34pad[pad].outEndp = -1;
        ds34pad[pad].enabled = (pads >> pad) & 1;
        ds34pad[pad].type = 0;

        ds34pad[pad].data[0] = 0xFF;
        ds34pad[pad].data[1] = 0xFF;
        ds34pad[pad].analog_btn = 0;

        mips_memset(&ds34pad[pad].data[2], 0x7F, 4);
        mips_memset(&ds34pad[pad].data[6], 0x00, 12);

        ds34pad[pad].sema = CreateMutex(IOP_MUTEX_UNLOCKED);
        ds34pad[pad].cmd_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

        if (ds34pad[pad].sema < 0 || ds34pad[pad].cmd_sema < 0) {
            DPRINTF("DS34USB: Failed to allocate I/O semaphore.\n");
            return 0;
        }
    }

    if (UsbRegisterDriver(&usb_driver) != USB_RC_OK) {
        DPRINTF("DS34USB: Error registering USB devices\n");
        return 0;
    }

    return 1;
}
