#include "types.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "thsemap.h"
#include "ds3usb.h"

//#define DPRINTF(x...) printf(x)
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

static u8 usb_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};

int usb_probe(int devId);
int usb_connect(int devId);
int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

UsbDriver usb_driver = {NULL, NULL, "ds3usb", usb_probe, usb_connect, usb_disconnect};

static void readReport(u8 *data, int pad);
static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad);

ds3usb_device ds3dev[MAX_PADS];

int usb_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("DS3USB: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("DS3USB: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->idVendor == SONY_VID && device->idProduct == DS3_PID)
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

    DPRINTF("DS3USB: connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds3dev[pad].usb_id == -1)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("DS3USB: Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(ds3dev[pad].sema);

    ds3dev[pad].dev.id = pad;
    ds3dev[pad].usb_id = devId;
    ds3dev[pad].controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);
    epCount = interface->bNumEndpoints - 1;
    endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && ds3dev[pad].interruptEndp < 0) {
                ds3dev[pad].interruptEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("DS3USB: register Event endpoint id =%i addr=%02X packetSize=%i\n", ds3dev[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (ds3dev[pad].interruptEndp < 0) {
        usb_release(pad);
        return 1;
    }

    UsbSetDeviceConfiguration(ds3dev[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);
    SignalSema(ds3dev[pad].sema);

    return 0;
}

int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("DS3USB: disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds3dev[pad].usb_id == devId) {
            pademu_disconnect(&ds3dev[pad].dev);
            break;
        }
    }

    if (pad < MAX_PADS)
        usb_release(pad);

    return 0;
}

static void usb_release(int pad)
{
    PollSema(ds3dev[pad].sema);
    
    if (ds3dev[pad].interruptEndp >= 0)
        UsbCloseEndpoint(ds3dev[pad].interruptEndp);

    ds3dev[pad].controlEndp = -1;
    ds3dev[pad].interruptEndp = -1;
    ds3dev[pad].usb_id = -1;
    
    SignalSema(ds3dev[pad].sema);
}

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("DS3USB: usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    ds3dev[pad].usb_resultcode = resultCode;

    SignalSema(ds3dev[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("DS3USB: usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(ds3dev[pad].cmd_sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;
    u8 led[2];

    PollSema(ds3dev[pad].sema);

    usb_buf[0] = 0x42;
    usb_buf[1] = 0x0C;
    usb_buf[2] = 0x00;
    usb_buf[3] = 0x00;

    UsbControlTransfer(ds3dev[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0xF4, 0, 4, usb_buf, NULL, NULL);
    DelayThread(10000);
    pademu_connect(&ds3dev[pad].dev);
    led[0] = led_patterns[ds3dev[pad].dev.id][1];
    led[1] = 0;

    LEDRumble(led, 0, 0, pad);
    SignalSema(ds3dev[pad].sema);
}

#define MAX_DELAY 10

static void readReport(u8 *data, int pad)
{
    ds3report_t *report = (ds3report_t *)data;
    if (report->ReportID == 0x01) {
        if (report->RightStickX == 0 && report->RightStickY == 0) // ledrumble cmd causes null report sometime
            return;

        ds3dev[pad].data[0] = ~report->ButtonStateL;
        ds3dev[pad].data[1] = ~report->ButtonStateH;

        ds3dev[pad].data[2] = report->RightStickX; //rx
        ds3dev[pad].data[3] = report->RightStickY; //ry
        ds3dev[pad].data[4] = report->LeftStickX;  //lx
        ds3dev[pad].data[5] = report->LeftStickY;  //ly

        ds3dev[pad].data[6] = report->PressureRight; //right
        ds3dev[pad].data[7] = report->PressureLeft;  //left
        ds3dev[pad].data[8] = report->PressureUp;    //up
        ds3dev[pad].data[9] = report->PressureDown;  //down

        ds3dev[pad].data[10] = report->PressureTriangle; //triangle
        ds3dev[pad].data[11] = report->PressureCircle;   //circle
        ds3dev[pad].data[12] = report->PressureCross;    //cross
        ds3dev[pad].data[13] = report->PressureSquare;   //square

        ds3dev[pad].data[14] = report->PressureL1; //L1
        ds3dev[pad].data[15] = report->PressureR1; //R1
        ds3dev[pad].data[16] = report->PressureL2; //L2
        ds3dev[pad].data[17] = report->PressureR2; //R2

        if (report->PSButtonState) {                                      //display battery level
            if (report->Select && (ds3dev[pad].btn_delay == MAX_DELAY)) { //PS + SELECT
                if (ds3dev[pad].analog_btn < 2)                           //unlocked mode
                    ds3dev[pad].analog_btn = !ds3dev[pad].analog_btn;

                ds3dev[pad].oldled[0] = led_patterns[pad][(ds3dev[pad].analog_btn & 1)];
                ds3dev[pad].btn_delay = 1;
            } else {
                if (report->Power != 0xEE)
                    ds3dev[pad].oldled[0] = power_level[report->Power];

                if (ds3dev[pad].btn_delay < MAX_DELAY)
                    ds3dev[pad].btn_delay++;
            }
        } else {
            ds3dev[pad].oldled[0] = led_patterns[pad][(ds3dev[pad].analog_btn & 1)];

            if (ds3dev[pad].btn_delay > 0)
                ds3dev[pad].btn_delay--;
        }

        if (report->Power == 0xEE) //charging
            ds3dev[pad].oldled[1] = 1;
        else
            ds3dev[pad].oldled[1] = 0;

        if (ds3dev[pad].btn_delay > 0) {
            ds3dev[pad].update_rum = 1;
        }
    }
}

static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad)
{
    PollSema(ds3dev[pad].cmd_sema);
    
    mips_memset(usb_buf, 0, sizeof(usb_buf));
    mips_memcpy(usb_buf, output_01_report, sizeof(output_01_report));

    usb_buf[1] = 0xFE; //rt
    usb_buf[2] = rrum; //rp
    usb_buf[3] = 0xFE; //lt
    usb_buf[4] = lrum; //lp
    usb_buf[9] = led[0] & 0x7F; //LED Conf

    if (led[1]) { //means charging, so blink
        usb_buf[13] = 0x32;
        usb_buf[18] = 0x32;
        usb_buf[23] = 0x32;
        usb_buf[28] = 0x32;
    }
        
	ds3dev[pad].oldled[0] = led[0];
    ds3dev[pad].oldled[1] = led[1];

    return UsbControlTransfer(ds3dev[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_SET_REPORT_OUTPUT << 8) | 0x01, 0, sizeof(output_01_report), usb_buf, usb_cmd_cb, (void *)pad);
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

void ds3usb_set_rumble(u8 lrum, u8 rrum, int port)
{
    WaitSema(ds3dev[port].sema);
    
    ds3dev[port].update_rum = 1;
    ds3dev[port].lrum = lrum;
    ds3dev[port].rrum = rrum;
        
    SignalSema(ds3dev[port].sema);
}

int ds3usb_get_data(u8 *dst, int size, int port)
{
    int ret = 0;

    WaitSema(ds3dev[port].sema);

    PollSema(ds3dev[port].sema);

    ret = UsbInterruptTransfer(ds3dev[port].interruptEndp, usb_buf, MAX_BUFFER_SIZE, usb_data_cb, (void *)port);

    if (ret == USB_RC_OK) {
        TransferWait(ds3dev[port].sema);
        if (!ds3dev[port].usb_resultcode)
            readReport(usb_buf, port);

        ds3dev[port].usb_resultcode = 1;
    } else {
        DPRINTF("DS3USB: DS3USB_get_data usb transfer error %d\n", ret);
    }

    mips_memcpy(dst, ds3dev[port].data, size);
    ret = ds3dev[port].analog_btn & 1;
        
    if (ds3dev[port].update_rum) {
        ret = LEDRumble(ds3dev[port].oldled, ds3dev[port].lrum, ds3dev[port].rrum, port);
        if (ret == USB_RC_OK)
            TransferWait(ds3dev[port].cmd_sema);
        else
            DPRINTF("DS3USB: LEDRumble usb transfer error %d\n", ret);

        ds3dev[port].update_rum = 0;
    }

    SignalSema(ds3dev[port].sema);

    return ret;
}

void ds3usb_set_mode(int mode, int lock, int port)
{
    if (lock == 3)
        ds3dev[port].analog_btn = 3;
    else
        ds3dev[port].analog_btn = mode;
}

void ds3usb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

int _start(int argc, char *argv[])
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        ds3dev[pad].usb_id = -1;
		ds3dev[pad].dev.id = -1;
        ds3dev[pad].dev.pad_get_data = ds3usb_get_data;
        ds3dev[pad].dev.pad_set_rumble = ds3usb_set_rumble;
        ds3dev[pad].dev.pad_set_mode = ds3usb_set_mode;

        ds3dev[pad].oldled[0] = 0;
        ds3dev[pad].oldled[1] = 0;
        ds3dev[pad].lrum = 0;
        ds3dev[pad].rrum = 0;
        ds3dev[pad].update_rum = 1;
        ds3dev[pad].sema = -1;
        ds3dev[pad].cmd_sema = -1;
        ds3dev[pad].controlEndp = -1;
        ds3dev[pad].interruptEndp = -1;

        ds3dev[pad].data[0] = 0xFF;
        ds3dev[pad].data[1] = 0xFF;
        ds3dev[pad].analog_btn = 0;

        mips_memset(&ds3dev[pad].data[2], 0x7F, 4);
        mips_memset(&ds3dev[pad].data[6], 0x00, 12);

        ds3dev[pad].sema = CreateMutex(IOP_MUTEX_UNLOCKED);
        ds3dev[pad].cmd_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

        if (ds3dev[pad].sema < 0 || ds3dev[pad].cmd_sema < 0) {
            DPRINTF("DS3USB: Failed to allocate I/O semaphore.\n");
            return MODULE_NO_RESIDENT_END;
        }
    }

    if (UsbRegisterDriver(&usb_driver) != USB_RC_OK) {
        DPRINTF("DS3USB: Error registering USB devices\n");
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}
