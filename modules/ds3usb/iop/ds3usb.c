#include "types.h"
#include "ioman.h"
#include "intrman.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifcmd.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "sysmem.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "thevent.h"
#include "thsemap.h"
#include "sifman.h"
#include "vblank.h"
#include "ds3usb.h"

IRX_ID("ds3usb", 1, 1);

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

#define MAX_PADS 2

static uint8_t output_01_report[] =
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

static uint8_t power_level[] =
    {
        0x00, 0x00, 0x02, 0x06, 0x0E, 0x1E};

static uint8_t usb_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};

int usb_probe(int devId);
int usb_connect(int devId);
int usb_disconnect(int devId);

static void usb_release(int pad);
static void usb_config_set(int result, int count, void *arg);

UsbDriver usb_driver = {NULL, NULL, "ds3usb", usb_probe, usb_connect, usb_disconnect};

static void DS3USB_init(int pad);
static void readReport(uint8_t *data, int pad);
static uint8_t LEDRumble(uint8_t led, uint8_t lrum, uint8_t rrum, int pad);
static uint8_t LED(uint8_t led, int pad);
static uint8_t Rumble(uint8_t lrum, uint8_t rrum, int pad);

typedef struct _usb_ds3
{
    int devId;
    int sema;
    int controlEndp;
    int eventEndp;
    uint8_t status;
    uint8_t oldled;
    uint8_t oldlrumble;
    uint8_t oldrrumble;
    uint8_t data[18];
    uint8_t enabled;
} ds3usb_device;

ds3usb_device ds3pad[MAX_PADS];

int usb_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("DS3USB: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("DS3USB: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->idVendor == 0x054C && device->idProduct == 0x0268)
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
    iop_sema_t SemaData;

    DPRINTF("DS3USB: connect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds3pad[pad].devId == -1 && ds3pad[pad].enabled)
            break;
    }

    if (pad >= MAX_PADS) {
        DPRINTF("DS3USB: Error - only %d device allowed !\n", MAX_PADS);
        return 1;
    }

    ds3pad[pad].devId = devId;

    ds3pad[pad].status = DS3USB_STATE_AUTHORIZED;

    ds3pad[pad].controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    epCount = interface->bNumEndpoints - 1;

    endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {

        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && ds3pad[pad].eventEndp < 0) {
                ds3pad[pad].eventEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("BT: register Event endpoint id =%i addr=%02X packetSize=%i\n", ds3pad[pad].eventEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
                break;
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (ds3pad[pad].eventEndp < 0) {
        usb_release(pad);
        return 1;
    }

    SemaData.initial = 1;
    SemaData.max = 1;
    SemaData.option = 0;
    SemaData.attr = 0;

    if ((ds3pad[pad].sema = CreateSema(&SemaData)) < 0) {
        DPRINTF("DS3USB: Failed to allocate I/O semaphore.\n");
        return -1;
    }

    ds3pad[pad].status |= DS3USB_STATE_CONNECTED;

    UsbSetDeviceConfiguration(ds3pad[pad].controlEndp, config->bConfigurationValue, usb_config_set, (void *)pad);

    return 0;
}

int usb_disconnect(int devId)
{
    uint8_t pad;

    DPRINTF("DS3USB: disconnect: devId=%i\n", devId);

    for (pad = 0; pad < MAX_PADS; pad++) {
        if (ds3pad[pad].devId == devId)
            break;
    }

    if (pad < MAX_PADS && (ds3pad[pad].status & DS3USB_STATE_AUTHORIZED))
        usb_release(pad);

    return 0;
}

static void usb_release(int pad)
{
    if (ds3pad[pad].sema >= 0)
        DeleteSema(ds3pad[pad].sema);

    if (ds3pad[pad].eventEndp >= 0)
        UsbCloseEndpoint(ds3pad[pad].eventEndp);

    ds3pad[pad].controlEndp = -1;
    ds3pad[pad].eventEndp = -1;
    ds3pad[pad].devId = -1;
    ds3pad[pad].status = DS3USB_STATE_DISCONNECTED;
}

static void usb_data_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("usb_data_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    if (!resultCode)
        readReport(usb_buf, pad);

    SignalSema(ds3pad[pad].sema);
}

static void usb_cmd_cb(int resultCode, int bytes, void *arg)
{
    int pad = (int)arg;

    //DPRINTF("usb_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    SignalSema(ds3pad[pad].sema);
}

static void usb_config_set(int result, int count, void *arg)
{
    int pad = (int)arg;

    ds3pad[pad].status |= DS3USB_STATE_CONFIGURED;

    DS3USB_init(pad);

    DelayThread(10000);

    LED((pad + 1) << 1, pad);

    ds3pad[pad].status |= DS3USB_STATE_RUNNING;
}

static void DS3USB_init(int pad)
{
    usb_buf[0] = 0x42;
    usb_buf[1] = 0x0c;
    usb_buf[2] = 0x00;
    usb_buf[3] = 0x00;

    UsbControlTransfer(ds3pad[pad].controlEndp, bmREQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0xF4, 0, 4, usb_buf, NULL, NULL);
}

#define DATA_START 2

static void readReport(uint8_t *data, int pad)
{
    if (data[0]) {
        ds3pad[pad].data[0] = ~data[DATA_START + ButtonStateL];
        ds3pad[pad].data[1] = ~data[DATA_START + ButtonStateH];

        ds3pad[pad].data[2] = data[DATA_START + RightStickX]; //rx
        ds3pad[pad].data[3] = data[DATA_START + RightStickY]; //ry
        ds3pad[pad].data[4] = data[DATA_START + LeftStickX];  //lx
        ds3pad[pad].data[5] = data[DATA_START + LeftStickY];  //ly

        ds3pad[pad].data[6] = data[DATA_START + PressureRight]; //right
        ds3pad[pad].data[7] = data[DATA_START + PressureLeft];  //left
        ds3pad[pad].data[8] = data[DATA_START + PressureUp];    //up
        ds3pad[pad].data[9] = data[DATA_START + PressureDown];  //down

        ds3pad[pad].data[10] = data[DATA_START + PressureTriangle]; //triangle
        ds3pad[pad].data[11] = data[DATA_START + PressureCircle];   //circle
        ds3pad[pad].data[12] = data[DATA_START + PressureCross];    //cross
        ds3pad[pad].data[13] = data[DATA_START + PressureSquare];   //square

        ds3pad[pad].data[14] = data[DATA_START + PressureL1]; //L1
        ds3pad[pad].data[15] = data[DATA_START + PressureR1]; //R1
        ds3pad[pad].data[16] = data[DATA_START + PressureL2]; //L2
        ds3pad[pad].data[17] = data[DATA_START + PressureR2]; //R2

        if (data[DATA_START + PSButtonState] && (data[DATA_START + Power] != 0xEE)) //display battery level
            ds3pad[pad].oldled = power_level[data[DATA_START + Power]];
        else
            ds3pad[pad].oldled = (pad + 1) << 1;

        if (data[DATA_START + Power] == 0xEE) //charging
            ds3pad[pad].oldled |= 0x80;
        else
            ds3pad[pad].oldled &= 0x7F;
    }
}

static uint8_t LEDRumble(uint8_t led, uint8_t lrum, uint8_t rrum, int pad)
{
    mips_memcpy(usb_buf, output_01_report, sizeof(output_01_report));

    usb_buf[1] = 0xFE; //rt
    usb_buf[2] = rrum; //rp
    usb_buf[3] = 0xFE; //lt
    usb_buf[4] = lrum; //lp

    usb_buf[9] = led & 0x7F; //LED Conf

    if (led & 0x80) //msb means charging, so blink
    {
        usb_buf[13] = 0x32;
        usb_buf[18] = 0x32;
        usb_buf[23] = 0x32;
        usb_buf[28] = 0x32;
    }

    ds3pad[pad].oldled = led;
    ds3pad[pad].oldlrumble = lrum;
    ds3pad[pad].oldrrumble = rrum;

    return UsbControlTransfer(ds3pad[pad].controlEndp, bmREQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_SET_REPORT_OUTPUT << 8) | 0x01, 0, sizeof(output_01_report), usb_buf, usb_cmd_cb, (void *)pad);
}

static uint8_t LED(uint8_t led, int pad)
{
    return LEDRumble(led, ds3pad[pad].oldlrumble, ds3pad[pad].oldrrumble, pad);
}

static uint8_t Rumble(uint8_t lrum, uint8_t rrum, int pad)
{
    return LEDRumble(ds3pad[pad].oldled, lrum, rrum, pad);
}

void ds3usb_set_rumble(uint8_t lrum, uint8_t rrum, int port)
{
    if (port >= MAX_PADS)
        return;

    WaitSema(ds3pad[port].sema);

    Rumble(lrum, rrum, port);

    WaitSema(ds3pad[port].sema);

    SignalSema(ds3pad[port].sema);
}

void ds3usb_set_led(uint8_t led, int port)
{
    if (port >= MAX_PADS)
        return;

    WaitSema(ds3pad[port].sema);

    LED(led, port);

    WaitSema(ds3pad[port].sema);

    SignalSema(ds3pad[port].sema);
}

void ds3usb_get_data(char *dst, int size, int port)
{
    if (port >= MAX_PADS)
        return;

    WaitSema(ds3pad[port].sema);

    UsbInterruptTransfer(ds3pad[port].eventEndp, usb_buf, MAX_BUFFER_SIZE, usb_data_cb, (void *)port);

    WaitSema(ds3pad[port].sema);

    mips_memcpy(dst, ds3pad[port].data, size);

    SignalSema(ds3pad[port].sema);
}

void ds3usb_get_bdaddr(uint8_t *data, int port)
{
    int i;

    if (port >= MAX_PADS)
        return;

    WaitSema(ds3pad[port].sema);

    UsbControlTransfer(ds3pad[port].controlEndp, bmREQ_USB_IN, USB_REQ_GET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0xF5, 0, 8, usb_buf, usb_cmd_cb, (void *)port);

    WaitSema(ds3pad[port].sema);

    for (i = 0; i < 6; i++)
        data[5 - i] = usb_buf[2 + i];

    SignalSema(ds3pad[port].sema);
}

void ds3usb_set_bdaddr(uint8_t *data, int port)
{
    int i;

    WaitSema(ds3pad[port].sema);

    usb_buf[0] = 0x01;
    usb_buf[1] = 0x00;

    for (i = 0; i < 6; i++)
        usb_buf[i + 2] = data[5 - i];

    UsbControlTransfer(ds3pad[port].controlEndp, bmREQ_USB_OUT, USB_REQ_SET_REPORT, (HID_USB_GET_REPORT_FEATURE << 8) | 0xF5, 0, 8, usb_buf, usb_cmd_cb, (void *)port);

    WaitSema(ds3pad[port].sema);

    SignalSema(ds3pad[port].sema);
}

void ds3usb_reset()
{
    int pad;

    for (pad = 0; pad < MAX_PADS; pad++)
        usb_release(pad);
}

int ds3usb_get_status(int port)
{
    return ds3pad[port].status;
}

void ds3usb_init(uint8_t pads)
{
    uint8_t pad;

    for (pad = 0; pad < MAX_PADS; pad++) {
        usb_release(pad);

        ds3pad[pad].status = 0;
        ds3pad[pad].devId = -1;
        ds3pad[pad].oldled = 0;
        ds3pad[pad].oldlrumble = 0;
        ds3pad[pad].oldrrumble = 0;
        ds3pad[pad].sema = -1;
        ds3pad[pad].controlEndp = -1;
        ds3pad[pad].eventEndp = -1;
        ds3pad[pad].enabled = (pads >> pad) & 1;

        ds3pad[pad].data[0] = 0xFF;
        ds3pad[pad].data[1] = 0xFF;

        mips_memset(&ds3pad[pad].data[2], 0x7F, 4);
        mips_memset(&ds3pad[pad].data[6], 0x00, 12);
    }
}

static void rpc_thread(void *data);
static void *rpc_sf(int cmd, void *data, int size);

static SifRpcDataQueue_t rpc_que __attribute__((aligned(16)));
static SifRpcServerData_t rpc_svr __attribute__((aligned(16)));

static int rpc_buf[64] __attribute((aligned(16)));

#define DS3USB_INIT 1
#define DS3USB_GET_STATUS 2
#define DS3USB_GET_BDADDR 3
#define DS3USB_SET_BDADDR 4
#define DS3USB_SET_RUMBLE 5
#define DS3USB_SET_LED 6
#define DS3USB_GET_DATA 7
#define DS3USB_RESET 8

#define DS3USB_BIND_RPC_ID 0x18E3878E

void rpc_thread(void *data)
{
    SifInitRpc(0);
    SifSetRpcQueue(&rpc_que, GetThreadId());
    SifRegisterRpc(&rpc_svr, DS3USB_BIND_RPC_ID, rpc_sf, rpc_buf, NULL, NULL, &rpc_que);
    SifRpcLoop(&rpc_que);
}

void *rpc_sf(int cmd, void *data, int size)
{
    switch (cmd) {
        case DS3USB_INIT:
            ds3usb_init(*(uint8_t *)data);
            break;
        case DS3USB_GET_STATUS:
            *(uint8_t *)data = ds3usb_get_status(*(uint8_t *)data);
            break;
        case DS3USB_GET_BDADDR:
            ds3usb_get_bdaddr((uint8_t *)data, *(uint8_t *)data);
            break;
        case DS3USB_SET_BDADDR:
            ds3usb_set_bdaddr((uint8_t *)(data + 1), *(uint8_t *)data);
            break;
        case DS3USB_SET_RUMBLE:
            ds3usb_set_rumble(*(uint8_t *)(data + 1), *(uint8_t *)(data + 2), *(uint8_t *)data);
            break;
        case DS3USB_SET_LED:
            ds3usb_set_led(*(uint8_t *)(data + 1), *(uint8_t *)data);
            break;
        case DS3USB_GET_DATA:
            ds3usb_get_data((uint8_t *)data, 18, *(uint8_t *)data);
            break;
        case DS3USB_RESET:
            ds3usb_reset();
            break;
        default:
            break;
    }

    return data;
}

int _start(int argc, char *argv[])
{
    uint8_t enable = 0xFF;

    if (argc > 1) {
        enable = argv[1][0];
    }

    ds3usb_init(enable);

    int ret = UsbRegisterDriver(&usb_driver);

    if (ret != USB_RC_OK) {
        DPRINTF("DS3USB: Error registering USB devices\n");
        return MODULE_NO_RESIDENT_END;
    }

    iop_thread_t rpc_th;

    rpc_th.attr = TH_C;
    rpc_th.thread = rpc_thread;
    rpc_th.priority = 40;
    rpc_th.stacksize = 0x800;
    rpc_th.option = 0;

    int thid = CreateThread(&rpc_th);

    if (thid > 0) {
        StartThread(thid, NULL);
        return MODULE_RESIDENT_END;
    }

    return MODULE_NO_RESIDENT_END;
}
