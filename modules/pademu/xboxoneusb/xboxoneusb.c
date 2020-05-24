#include "types.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "thsemap.h"
#include "xboxoneusb.h"

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

#define REQ_USB_OUT (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE)
#define REQ_USB_IN  (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE)

#define MAX_PADS 4

static u8 usb_buf[MAX_BUFFER_SIZE + 32] __attribute((aligned(4))) = {0};

int usb_probe(int devId);
int usb_connect(int devId);
int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

UsbDriver usb_driver = {NULL, NULL, "xboxoneusb", usb_probe, usb_connect, usb_disconnect};

static void readReport(u8 *data, int pad);
static int Rumble(u8 lrum, u8 rrum, int pad);

xboxoneusb_device xboxonedev[MAX_PADS];
static u8 cmdcnt = 0;

int usb_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("XBOXONEUSB: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("XBOXONEUSB: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if ((device->idVendor == XBOX_VID || device->idVendor == XBOX_VID2 || device->idVendor == XBOX_VID3 || device->idVendor == XBOX_VID4 || device->idVendor == XBOX_VID5 || device->idVendor == XBOX_VID6) &&
        (device->idProduct == XBOX_ONE_PID1 || device->idProduct == XBOX_ONE_PID2 || device->idProduct == XBOX_ONE_PID3 || device->idProduct == XBOX_ONE_PID4 ||
         device->idProduct == XBOX_ONE_PID5 || device->idProduct == XBOX_ONE_PID6 || device->idProduct == XBOX_ONE_PID7 || device->idProduct == XBOX_ONE_PID8 ||
         device->idProduct == XBOX_ONE_PID9 || device->idProduct == XBOX_ONE_PID10 || device->idProduct == XBOX_ONE_PID11 || device->idProduct == XBOX_ONE_PID12 || device->idProduct == XBOX_ONE_PID13))
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

    DPRINTF("XBOXONEUSB: connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (xboxonedev[pad].usb_id == -1)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("XBOXONEUSB: Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(xboxonedev[pad].sema);

    xboxonedev[pad].dev.id = pad;
    xboxonedev[pad].usb_id = devId;
    xboxonedev[pad].controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);
    epCount = interface->bNumEndpoints - 1;
	endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && xboxonedev[pad].interruptEndp < 0) {
                xboxonedev[pad].interruptEndp = UsbOpenEndpointAligned(devId, endpoint);
                xboxonedev[pad].endin = endpoint;
                DPRINTF("XBOXONEUSB: register Event endpoint id =%i addr=%02X packetSize=%i\n", xboxonedev[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && xboxonedev[pad].outEndp < 0) {
                xboxonedev[pad].outEndp = UsbOpenEndpointAligned(devId, endpoint);
                xboxonedev[pad].endout = endpoint;
                DPRINTF("DS34USB: register Output endpoint id =%i addr=%02X packetSize=%i\n", xboxonepad[pad].outEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (xboxonedev[pad].interruptEndp < 0 || xboxonedev[pad].outEndp < 0) {
        usb_release(pad);
        return 1;
    }

    UsbSetDeviceConfiguration(xboxonedev[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);
    SignalSema(xboxonedev[pad].sema);

    return 0;
}

int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("XBOXONEUSB: disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (xboxonedev[pad].usb_id == devId) {
            pademu_disconnect(&xboxonedev[pad].dev);
            break;
        }
    }

    if (pad < MAX_PADS)
        usb_release(pad);

    return 0;
}

static void usb_release(int pad)
{
    PollSema(xboxonedev[pad].sema);
    
    if (xboxonedev[pad].interruptEndp >= 0)
        UsbCloseEndpoint(xboxonedev[pad].interruptEndp);

	if (xboxonedev[pad].outEndp >= 0)
        UsbCloseEndpoint(xboxonedev[pad].outEndp);

    xboxonedev[pad].controlEndp = -1;
    xboxonedev[pad].interruptEndp = -1;
    xboxonedev[pad].outEndp = -1;
    xboxonedev[pad].usb_id = -1;
    
    SignalSema(xboxonedev[pad].sema);
}

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("XBOXONEUSB: usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    xboxonedev[pad].usb_resultcode = resultCode;

    SignalSema(xboxonedev[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("XBOXONEUSB: usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(xboxonedev[pad].cmd_sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;

    PollSema(xboxonedev[pad].sema);

    cmdcnt = 0;
    usb_buf[0] = 0x05;
    usb_buf[1] = 0x20;
    usb_buf[2] = cmdcnt++;
    usb_buf[3] = 0x01;
    usb_buf[4] = 0x00;
    UsbInterruptTransfer(xboxonedev[pad].outEndp, usb_buf, 5, NULL, NULL);
    DelayThread(10000);

    SignalSema(xboxonedev[pad].sema);

	pademu_connect(&xboxonedev[pad].dev);
}

#define MAX_DELAY 10

static void readReport(u8 *data, int pad)
{
    xboxonereport_t *report = (xboxonereport_t *)data;
    if (report->ReportID == 0x20) {
        xboxonedev[pad].data[0] = ~(report->Back | report->LS << 1 | report->RS << 2 | report->Start << 3 | report->Up << 4 | report->Right << 5 | report->Down << 6 | report->Left << 7);
        xboxonedev[pad].data[1] = ~((report->LeftTriggerH != 0) | (report->RightTriggerH != 0) << 1 | report->LB << 2 | report->RB << 3 | report->Y << 4 | report->B << 5 | report->A << 6 | report->X << 7);

        xboxonedev[pad].data[2] = report->RightStickXH + 128;    //rx
        xboxonedev[pad].data[3] = ~(report->RightStickYH + 128); //ry
        xboxonedev[pad].data[4] = report->LeftStickXH + 128;     //lx
        xboxonedev[pad].data[5] = ~(report->LeftStickYH + 128);  //ly

        xboxonedev[pad].data[6] = report->Right * 255; //right
        xboxonedev[pad].data[7] = report->Left * 255;  //left
        xboxonedev[pad].data[8] = report->Up * 255;    //up
        xboxonedev[pad].data[9] = report->Down * 255;  //down

        xboxonedev[pad].data[10] = report->Y * 255; //triangle
        xboxonedev[pad].data[11] = report->B * 255; //circle
        xboxonedev[pad].data[12] = report->A * 255; //cross
        xboxonedev[pad].data[13] = report->X * 255; //square

        xboxonedev[pad].data[14] = report->LB * 255;      //L1
        xboxonedev[pad].data[15] = report->RB * 255;      //R1
        xboxonedev[pad].data[16] = report->LeftTriggerH;  //L2
        xboxonedev[pad].data[17] = report->RightTriggerH; //R2
    }
}

static int Rumble(u8 lrum, u8 rrum, int pad)
{
    PollSema(xboxonedev[pad].cmd_sema);
    
    usb_buf[0] = 0x09;
    usb_buf[1] = 0x00;
    usb_buf[2] = cmdcnt++;
    usb_buf[3] = 0x09;  // Substructure (what substructure rest of this packet has)
    usb_buf[4] = 0x00;  // Mode
    usb_buf[5] = 0x0F;  // Rumble mask (what motors are activated) (0000 lT rT L R)
    usb_buf[6] = 0x00;  // lT force
    usb_buf[7] = 0x00;  // rT force
    usb_buf[8] = lrum;  // L force
    usb_buf[9] = rrum;  // R force
    usb_buf[10] = 0x80; // Length of pulse
    usb_buf[11] = 0x00; // Off period
    usb_buf[12] = 0x00; // Repeat count

    return UsbInterruptTransfer(xboxonedev[pad].outEndp, usb_buf, 13, usb_cmd_cb, (void *)pad);
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

void xboxoneusb_set_rumble(u8 lrum, u8 rrum, int port)
{
    WaitSema(xboxonedev[port].sema);
    
    xboxonedev[port].update_rum = 1;
    xboxonedev[port].lrum = lrum;
    xboxonedev[port].rrum = rrum;
        
    SignalSema(xboxonedev[port].sema);
}

int xboxoneusb_get_data(u8 *dst, int size, int port)
{
    int ret = 0;

    WaitSema(xboxonedev[port].sema);

    PollSema(xboxonedev[port].sema);

    ret = UsbInterruptTransfer(xboxonedev[port].interruptEndp, usb_buf, MAX_BUFFER_SIZE, usb_data_cb, (void *)port);

    if (ret == USB_RC_OK) {
        TransferWait(xboxonedev[port].sema);
        if (!xboxonedev[port].usb_resultcode)
            readReport(usb_buf, port);

        xboxonedev[port].usb_resultcode = 1;
    } else {
        UsbCloseEndpoint(xboxonedev[port].interruptEndp);
        xboxonedev[port].interruptEndp = UsbOpenEndpointAligned(xboxonedev[port].usb_id, xboxonedev[port].endin);
        DPRINTF("XBOXONEUSB: XBOXONEUSB_get_data usb transfer error %d\n", ret);
    }

    mips_memcpy(dst, xboxonedev[port].data, size);
    ret = xboxonedev[port].analog_btn & 1;
        
    if (xboxonedev[port].update_rum) {
        ret = Rumble(xboxonedev[port].lrum, xboxonedev[port].rrum, port);
        if (ret == USB_RC_OK) {
            TransferWait(xboxonedev[port].cmd_sema);
        } else {
            UsbCloseEndpoint(xboxonedev[port].outEndp);
            xboxonedev[port].outEndp = UsbOpenEndpointAligned(xboxonedev[port].usb_id, xboxonedev[port].endout);
            DPRINTF("XBOXONEUSB: LEDRumble usb transfer error %d\n", ret);
        }

        xboxonedev[port].update_rum = 0;
    }

    SignalSema(xboxonedev[port].sema);

    return ret;
}

void xboxoneusb_set_mode(int mode, int lock, int port)
{
    if (lock == 3)
        xboxonedev[port].analog_btn = 3;
    else
        xboxonedev[port].analog_btn = mode;
}

void xboxoneusb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

int _start(int argc, char *argv[])
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        xboxonedev[pad].usb_id = -1;
		xboxonedev[pad].dev.id = -1;
        xboxonedev[pad].dev.pad_get_data = xboxoneusb_get_data;
        xboxonedev[pad].dev.pad_set_rumble = xboxoneusb_set_rumble;
        xboxonedev[pad].dev.pad_set_mode = xboxoneusb_set_mode;

        xboxonedev[pad].lrum = 0;
        xboxonedev[pad].rrum = 0;
        xboxonedev[pad].update_rum = 1;
        xboxonedev[pad].sema = -1;
        xboxonedev[pad].cmd_sema = -1;
        xboxonedev[pad].controlEndp = -1;
        xboxonedev[pad].interruptEndp = -1;
        xboxonedev[pad].outEndp = -1;

        xboxonedev[pad].data[0] = 0xFF;
        xboxonedev[pad].data[1] = 0xFF;
        xboxonedev[pad].analog_btn = 0;

        mips_memset(&xboxonedev[pad].data[2], 0x7F, 4);
        mips_memset(&xboxonedev[pad].data[6], 0x00, 12);

        xboxonedev[pad].sema = CreateMutex(IOP_MUTEX_UNLOCKED);
        xboxonedev[pad].cmd_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

        if (xboxonedev[pad].sema < 0 || xboxonedev[pad].cmd_sema < 0) {
            DPRINTF("XBOXONEUSB: Failed to allocate I/O semaphore.\n");
            return MODULE_NO_RESIDENT_END;
        }
    }

    if (UsbRegisterDriver(&usb_driver) != USB_RC_OK) {
        DPRINTF("XBOXONEUSB: Error registering USB devices\n");
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}
