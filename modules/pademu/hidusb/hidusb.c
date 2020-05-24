#include "types.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "thsemap.h"
#include "hidusb.h"

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

#define MAX_PADS 4

static u8 usb_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};

int usb_probe(int devId);
int usb_connect(int devId);
int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

UsbDriver usb_driver = {NULL, NULL, "hidusb", usb_probe, usb_connect, usb_disconnect};

int read_report_descriptor(u8 *data, int size, hidreport_t *report);
static void read_report(u8 *data, int pad);

hidusb_device hiddev[MAX_PADS];

int usb_probe(int devId)
{
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;

    DPRINTF("HIDUSB: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    if (interface->bInterfaceClass != USB_CLASS_HID)
        return 0;

    return 1;
}

int usb_connect(int devId)
{
    int pad, epCount, hid_report_size;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;
    UsbEndpointDescriptor *endpoint;
    UsbHidDescriptor *hid;
    u8 buf[512];

    DPRINTF("HIDUSB: connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (hiddev[pad].usb_id == -1)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("HIDUSB: Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    PollSema(hiddev[pad].sema);

    hiddev[pad].dev.id = pad;
    hiddev[pad].usb_id = devId;
    hiddev[pad].controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);
    hid = (UsbHidDescriptor *)UsbGetDeviceStaticDescriptor(devId, interface, USB_DT_HID);
    epCount = interface->bNumEndpoints - 1;
    endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {
        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && hiddev[pad].interruptEndp < 0) {
                hiddev[pad].interruptEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("DS3USB: register Event endpoint id =%i addr=%02X packetSize=%i\n", hiddev[pad].interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (hiddev[pad].interruptEndp < 0) {
        usb_release(pad);
        return 1;
    }
    hid_report_size = (hid->Sub[0].wDescriptorLengthL | hid->Sub[0].wDescriptorLengthH << 8);
    UsbControlTransfer(hiddev[pad].controlEndp, (USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE), USB_REQ_GET_DESCRIPTOR, (hid->Sub[0].bDescriptorType << 8), interface->bInterfaceNumber, 512, buf, NULL, NULL);
    DelayThread(10000);
    read_report_descriptor(buf, hid_report_size, &hiddev[pad].rep);
    UsbSetDeviceConfiguration(hiddev[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);
    SignalSema(hiddev[pad].sema);

    return 0;
}

int usb_disconnect(int devId)
{
    u8 pad;

    DPRINTF("DS3USB: disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (hiddev[pad].usb_id == devId) {
            pademu_disconnect(&hiddev[pad].dev);
            break;
        }
    }

    if (pad < MAX_PADS)
        usb_release(pad);

    return 0;
}

static void usb_release(int pad)
{
    PollSema(hiddev[pad].sema);

    if (hiddev[pad].interruptEndp >= 0)
        UsbCloseEndpoint(hiddev[pad].interruptEndp);

    hiddev[pad].controlEndp = -1;
    hiddev[pad].interruptEndp = -1;
    hiddev[pad].usb_id = -1;
    hiddev[pad].lrum = 0;
    hiddev[pad].rrum = 0;
    hiddev[pad].update_rum = 1;
    hiddev[pad].data[0] = 0xFF;
    hiddev[pad].data[1] = 0xFF;
    hiddev[pad].analog_btn = 0;

    mips_memset(&hiddev[pad].data[2], 0x7F, 4);
    mips_memset(&hiddev[pad].data[6], 0x00, 12);

    SignalSema(hiddev[pad].sema);
}

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("DS3USB: usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    hiddev[pad].usb_resultcode = resultCode;

    SignalSema(hiddev[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("DS3USB: usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(hiddev[pad].cmd_sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;
    PollSema(hiddev[pad].sema);
    DelayThread(10000);
    pademu_connect(&hiddev[pad].dev);
    SignalSema(hiddev[pad].sema);
}

#define MAX_DELAY 10

static void read_report(u8 *data, int pad)
{
    int i, count, pos, bits;
    u8 up = 0, down = 0, left = 0, right = 0, mask;


    if (data[0] == 0x01) {
        hiddev[pad].data[0] = 0x00;
        hiddev[pad].data[1] = 0x00;
        count = 4;
        if (hiddev[pad].rep.axes.count < 4)
            count = hiddev[pad].rep.axes.count;

        for (i = 0; i < count; i++) {
            hiddev[pad].data[2 + i] = data[1 + (hiddev[pad].rep.axes.start_pos + hiddev[pad].rep.axes.size * i) / 8];
        }

		count = 16;
        bits = 8;
        if (hiddev[pad].rep.hats.count > 0) {
            pos = hiddev[pad].rep.hats.start_pos;
			switch ((data[1 + pos / 8] >> pos % 8) & 0x0F) {
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
			hiddev[pad].data[0] = ~(up << 4 | right << 5 | down << 6 | left << 7 | 0x0F);
            hiddev[pad].data[6] = right * 255; //right
			hiddev[pad].data[7] = left * 255; //left
			hiddev[pad].data[8] = up * 255; //up
			hiddev[pad].data[9] = down * 255; //down

            count = 12;
            bits = 4;
        }

        if (hiddev[pad].rep.buttons.count < count)
            count = hiddev[pad].rep.buttons.count;

        for (i = 0; i < count; i++) {
            pos = hiddev[pad].rep.buttons.start_pos + hiddev[pad].rep.buttons.size * i;
            if (i >= bits) {
                //mask = ~(1 << (i - bits));
                //hiddev[pad].data[1] &= mask | (((data[1 + pos / 8] >> pos % 8) & 1) << (i - bits));
                hiddev[pad].data[1] |= (((data[1 + pos / 8] >> pos % 8) & 1) << (i - bits));
            } else {
                //mask = ~(1 << i);
                //hiddev[pad].data[0] &= mask | (((data[1 + pos / 8] >> pos % 8) & 1) << i);
                hiddev[pad].data[0] |= (((data[1 + pos / 8] >> pos % 8) & 1) << i);
            }
        }

		//(report->Share | report->L3 << 1 | report->R3 << 2 | report->Option << 3 | up << 4 | right << 5 | down << 6 | left << 7);
        //(report->L2 | report->R2 << 1 | report->L1 << 2 | report->R1 << 3 | report->Triangle << 4 | report->Circle << 5 | report->Cross << 6 | report->Square << 7);

		/*
        hiddev[pad].data[10] = report->PressureTriangle; //triangle
        hiddev[pad].data[11] = report->PressureCircle;   //circle
        hiddev[pad].data[12] = report->PressureCross;    //cross
        hiddev[pad].data[13] = report->PressureSquare;   //square

        hiddev[pad].data[14] = report->PressureL1; //L1
        hiddev[pad].data[15] = report->PressureR1; //R1
        hiddev[pad].data[16] = report->PressureL2; //L2
        hiddev[pad].data[17] = report->PressureR2; //R2*/

        /*hiddev[pad].data[0] = ~hiddev[pad].data[0];
        hiddev[pad].data[1] = ~hiddev[pad].data[1];*/
    }
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

void hidusb_set_rumble(u8 lrum, u8 rrum, int port)
{
    WaitSema(hiddev[port].sema);

    hiddev[port].update_rum = 1;
    hiddev[port].lrum = lrum;
    hiddev[port].rrum = rrum;

    SignalSema(hiddev[port].sema);
}

int hidusb_get_data(u8 *dst, int size, int port)
{
    int ret = 0;

    WaitSema(hiddev[port].sema);

    PollSema(hiddev[port].sema);

    ret = UsbInterruptTransfer(hiddev[port].interruptEndp, usb_buf, MAX_BUFFER_SIZE, usb_data_cb, (void *)port);

    if (ret == USB_RC_OK) {
        TransferWait(hiddev[port].sema);
        if (!hiddev[port].usb_resultcode)
            read_report(usb_buf, port);

        hiddev[port].usb_resultcode = 1;
    } else {
        DPRINTF("HIDUSB: hidusb_get_data usb transfer error %d\n", ret);
    }

    mips_memcpy(dst, hiddev[port].data, size);
    ret = hiddev[port].analog_btn & 1;

    /*if (hiddev[port].update_rum) {
        ret = LEDRumble(hiddev[port].oldled, hiddev[port].lrum, hiddev[port].rrum, port);
        if (ret == USB_RC_OK)
            TransferWait(hiddev[port].cmd_sema);
        else
            DPRINTF("DS3USB: LEDRumble usb transfer error %d\n", ret);

        hiddev[port].update_rum = 0;
    }*/

    SignalSema(hiddev[port].sema);

    return ret;
}

void hidusb_set_mode(int mode, int lock, int port)
{
    if (lock == 3)
        hiddev[port].analog_btn = 3;
    else
        hiddev[port].analog_btn = mode;
}

void hidusb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

int _start(int argc, char *argv[])
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        hiddev[pad].usb_id = -1;
        hiddev[pad].dev.id = -1;
        hiddev[pad].dev.pad_get_data = hidusb_get_data;
        hiddev[pad].dev.pad_set_rumble = hidusb_set_rumble;
        hiddev[pad].dev.pad_set_mode = hidusb_set_mode;

        hiddev[pad].lrum = 0;
        hiddev[pad].rrum = 0;
        hiddev[pad].update_rum = 1;
        hiddev[pad].sema = -1;
        hiddev[pad].cmd_sema = -1;
        hiddev[pad].controlEndp = -1;
        hiddev[pad].interruptEndp = -1;

        hiddev[pad].data[0] = 0xFF;
        hiddev[pad].data[1] = 0xFF;
        hiddev[pad].analog_btn = 0;

        mips_memset(&hiddev[pad].data[2], 0x7F, 4);
        mips_memset(&hiddev[pad].data[6], 0x00, 12);

        hiddev[pad].sema = CreateMutex(IOP_MUTEX_UNLOCKED);
        hiddev[pad].cmd_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

        if (hiddev[pad].sema < 0 || hiddev[pad].cmd_sema < 0) {
            DPRINTF("HIDUSB: Failed to allocate I/O semaphore.\n");
            return MODULE_NO_RESIDENT_END;
        }
    }

    if (UsbRegisterDriver(&usb_driver) != USB_RC_OK) {
        DPRINTF("HIDUSB: Error registering USB devices\n");
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}

int read_report_descriptor(u8 *data, int size, hidreport_t *report)
{
    u8 bSize, bType, bTag, bDataSize, bLongItemTag;
    int dSize;
    unsigned int dVal;
    u8 *dPtr;
    int i;
    int rPos, rSize, rCount, rUsage;

    dPtr = data;
    dSize = size;

    rPos = 0;
    rSize = 0;
    rCount = 0;
    rUsage = 0;

    report->buttons.count = 0;
    report->axes.count = 0;
    report->hats.count = 0;

    while (dSize > 0) {
        if (*dPtr != 0xFE) { //short item tag

            bSize = *dPtr & 3;

            if (bSize == 3)
                bSize = 4;

            bType = (*dPtr >> 2) & 3;
            bTag = (*dPtr >> 4) & 0x0F;

            for (i = 0, dVal = 0; i < bSize; i++)
                dVal |= *(dPtr + 1 + i) << (8 * i);

            if (*dPtr == 0x85 && dVal == 2) //ReportID
                break;

            if (*dPtr == 0x95)
                rCount = dVal;

            if (*dPtr == 0x75)
                rSize = dVal;

            if (*dPtr == 0x09 || *dPtr == 0x05)
                rUsage = dVal;

            if (*dPtr == 0x81) { //Input

                switch (rUsage) {
                    case 0x09: //buttons
                        report->buttons.count = rCount;
                        report->buttons.size = rSize;
                        report->buttons.start_pos = rPos;
                        printf("Usage buttons: rCount %d rSize %d rPos %d\n", rCount, rSize, rPos);
                        break;
                    case 0x30: //x
                    case 0x31: //y
                    case 0x32: //z
                    case 0x33: //rx
                    case 0x34: //ry
                    case 0x35: //rz
                        report->axes.count = rCount;
                        report->axes.size = rSize;
                        report->axes.start_pos = rPos;
                        printf("Usage axes: rCount %d rSize %d rPos %d\n", rCount, rSize, rPos);
                        break;
                    case 0x39: //hats
                        report->hats.count = rCount;
                        report->hats.size = rSize;
                        report->hats.start_pos = rPos;
                        printf("Usage hats: rCount %d rSize %d rPos %d\n", rCount, rSize, rPos);
                        break;
                }

                rPos += rSize * rCount;
                rSize = 0;
                rCount = 0;
                rUsage = 0;
            }

            dSize -= bSize + 1;
            dPtr += bSize + 1;
        } else { //long item tag

            bDataSize = *(dPtr + 1);
            bLongItemTag = *(dPtr + 2);

            for (i = 0; i < bDataSize + 3; i++) {
                dPtr++;
                dSize--;
            }
        }
    }

    return rPos;
}
