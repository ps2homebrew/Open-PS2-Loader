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

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

#define REQ_USB_OUT (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE)
#define REQ_USB_IN  (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE)

#define MAX_PADS 4

static u8 usb_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};

int usb_probe(int devId);
int usb_connect(int devId);
int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

UsbDriver usb_driver = {NULL, NULL, "xbox360usb", usb_probe, usb_connect, usb_disconnect};

static void readReport(u8 *data, int pad);
static int LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad);

xbox360usb_device xbox360dev[MAX_PADS];

int usb_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("XBOX360USB: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("XBOX360USB: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if ((device->idVendor == XBOX_VID || device->idVendor == MADCATZ_VID || device->idVendor == JOYTECH_VID || device->idVendor == GAMESTOP_VID) &&
        (device->idProduct == XBOX_WIRED_PID || device->idProduct == MADCATZ_WIRED_PID || device->idProduct == GAMESTOP_WIRED_PID ||
		 device->idProduct == AFTERGLOW_WIRED_PID || device->idProduct == JOYTECH_WIRED_PID))
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

    DPRINTF("XBOX360USB: connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (xbox360dev[pad].usb_id == -1)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("XBOX360USB: Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(xbox360dev[pad].sema);

    xbox360dev[pad].dev.id = pad;
    xbox360dev[pad].usb_id = devId;
    xbox360dev[pad].controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);
    epCount = interface->bNumEndpoints - 1;
	endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && xbox360dev[pad].interruptEndp < 0) {
                xbox360dev[pad].interruptEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("XBOX360USB: register Event endpoint id =%i addr=%02X packetSize=%i\n", xbox360dev[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (xbox360dev[pad].interruptEndp < 0) {
        usb_release(pad);
        return 1;
    }

    UsbSetDeviceConfiguration(xbox360dev[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);
    SignalSema(xbox360dev[pad].sema);

    return 0;
}

int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("XBOX360USB: disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (xbox360dev[pad].usb_id == devId) {
            pademu_disconnect(&xbox360dev[pad].dev);
            break;
        }
    }

    if (pad < MAX_PADS)
        usb_release(pad);

    return 0;
}

static void usb_release(int pad)
{
    PollSema(xbox360dev[pad].sema);
    
    if (xbox360dev[pad].interruptEndp >= 0)
        UsbCloseEndpoint(xbox360dev[pad].interruptEndp);

    xbox360dev[pad].controlEndp = -1;
    xbox360dev[pad].interruptEndp = -1;
    xbox360dev[pad].usb_id = -1;
    
    SignalSema(xbox360dev[pad].sema);
}

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("XBOX360USB: usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    xbox360dev[pad].usb_resultcode = resultCode;

    SignalSema(xbox360dev[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("XBOX360USB: usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

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

	pademu_connect(&xbox360dev[pad].dev);
}

#define MAX_DELAY 10

static void readReport(u8 *data, int pad)
{
    xbox360report_t *report = (xbox360report_t *)data;
    if (report->ReportID == 0x00 && report->Length == 0x14) {
        xbox360dev[pad].data[0] = ~(report->Back | report->LS << 1 | report->RS << 2 | report->Start << 3 | report->Up << 4 | report->Right << 5 | report->Down << 6 | report->Left << 7);
        xbox360dev[pad].data[1] = ~((report->LeftTrigger != 0) | (report->RightTrigger != 0) << 1 | report->LB << 2 | report->RB << 3 | report->Y << 4 | report->B << 5 | report->A << 6 | report->X << 7);

        xbox360dev[pad].data[2] = report->RightStickXH + 128;  //rx
        xbox360dev[pad].data[3] = ~(report->RightStickYH + 128); //ry
        xbox360dev[pad].data[4] = report->LeftStickXH + 128;  //lx
        xbox360dev[pad].data[5] = ~(report->LeftStickYH + 128);  //ly

        xbox360dev[pad].data[6] = report->Right * 255; //right
        xbox360dev[pad].data[7] = report->Left * 255;  //left
        xbox360dev[pad].data[8] = report->Up * 255;    //up
        xbox360dev[pad].data[9] = report->Down * 255;  //down

        xbox360dev[pad].data[10] = report->Y * 255; //triangle
        xbox360dev[pad].data[11] = report->B * 255; //circle
        xbox360dev[pad].data[12] = report->A * 255; //cross
        xbox360dev[pad].data[13] = report->X * 255; //square

        xbox360dev[pad].data[14] = report->LB * 255;     //L1
        xbox360dev[pad].data[15] = report->RB * 255;     //R1
        xbox360dev[pad].data[16] = report->LeftTrigger;  //L2
        xbox360dev[pad].data[17] = report->RightTrigger; //R2

        if (report->XBOX) {                                              //display battery level
            if (report->Back && (xbox360dev[pad].btn_delay == MAX_DELAY)) { //XBOX + BACK
                if (xbox360dev[pad].analog_btn < 2)                         //unlocked mode
                    xbox360dev[pad].analog_btn = !xbox360dev[pad].analog_btn;

                //xbox360dev[pad].oldled[0] = led_patterns[pad][(xbox360dev[pad].analog_btn & 1)];
                xbox360dev[pad].btn_delay = 1;
            } else {
                //if (report->Power != 0xEE)
                //    xbox360dev[pad].oldled[0] = power_level[report->Power];

                if (xbox360dev[pad].btn_delay < MAX_DELAY)
                    xbox360dev[pad].btn_delay++;
            }
        } else {
            //xbox360dev[pad].oldled[0] = led_patterns[pad][(xbox360dev[pad].analog_btn & 1)];

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

    ret = UsbControlTransfer(xbox360dev[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_SET_REPORT_OUTPUT << 8) | 0x01, 0, 8, usb_buf, usb_cmd_cb, (void *)pad);
    if (ret == USB_RC_OK) {
        usb_buf[0] = 0x01;
        usb_buf[1] = 0x03;
        usb_buf[2] = led[0];
        ret = UsbControlTransfer(xbox360dev[pad].controlEndp, REQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_SET_REPORT_OUTPUT << 8) | 0x01, 0, 3, usb_buf, usb_cmd_cb, (void *)pad);
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

void xbox360usb_set_rumble(u8 lrum, u8 rrum, int port)
{
    WaitSema(xbox360dev[port].sema);
    
    xbox360dev[port].update_rum = 1;
    xbox360dev[port].lrum = lrum;
    xbox360dev[port].rrum = rrum;
        
    SignalSema(xbox360dev[port].sema);
}

int xbox360usb_get_data(u8 *dst, int size, int port)
{
    int ret = 0;

    WaitSema(xbox360dev[port].sema);

    PollSema(xbox360dev[port].sema);

    ret = UsbInterruptTransfer(xbox360dev[port].interruptEndp, usb_buf, MAX_BUFFER_SIZE, usb_data_cb, (void *)port);

    if (ret == USB_RC_OK) {
        TransferWait(xbox360dev[port].sema);
        if (!xbox360dev[port].usb_resultcode)
            readReport(usb_buf, port);

        xbox360dev[port].usb_resultcode = 1;
    } else {
        DPRINTF("XBOX360USB: XBOX360USB_get_data usb transfer error %d\n", ret);
    }

    mips_memcpy(dst, xbox360dev[port].data, size);
    ret = xbox360dev[port].analog_btn & 1;
        
    if (xbox360dev[port].update_rum) {
        ret = LEDRumble(xbox360dev[port].oldled, xbox360dev[port].lrum, xbox360dev[port].rrum, port);
        if (ret == USB_RC_OK)
            TransferWait(xbox360dev[port].cmd_sema);
        else
            DPRINTF("XBOX360USB: LEDRumble usb transfer error %d\n", ret);

        xbox360dev[port].update_rum = 0;
    }

    SignalSema(xbox360dev[port].sema);

    return ret;
}

void xbox360usb_set_mode(int mode, int lock, int port)
{
    if (lock == 3)
        xbox360dev[port].analog_btn = 3;
    else
        xbox360dev[port].analog_btn = mode;
}

void xbox360usb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

int _start(int argc, char *argv[])
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        xbox360dev[pad].usb_id = -1;
		xbox360dev[pad].dev.id = -1;
        xbox360dev[pad].dev.pad_get_data = xbox360usb_get_data;
        xbox360dev[pad].dev.pad_set_rumble = xbox360usb_set_rumble;
        xbox360dev[pad].dev.pad_set_mode = xbox360usb_set_mode;

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
            DPRINTF("XBOX360USB: Failed to allocate I/O semaphore.\n");
            return MODULE_NO_RESIDENT_END;
        }
    }

    if (UsbRegisterDriver(&usb_driver) != USB_RC_OK) {
        DPRINTF("XBOX360USB: Error registering USB devices\n");
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}
