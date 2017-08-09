
/* bt code ported from: https://github.com/IonAgorria/Arduino-PSRemote see README */
/* usb code based on usbhdfsd module from ps2sdk */

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
#include "ds3bt.h"
#include "sys_utils.h"

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

#define USB_CLASS_WIRELESS_CONTROLLER 0xE0
#define USB_SUBCLASS_RF_CONTROLLER 0x01
#define USB_PROTOCOL_BLUETOOTH_PROG 0x01

int bt_probe(int devId);
int bt_connect(int devId);
int bt_disconnect(int devId);

UsbDriver bt_driver = {NULL, NULL, "ds3bt", bt_probe, bt_connect, bt_disconnect};

typedef struct _bt_dev
{
    int devId;
    int hci_sema;
    int l2cap_sema;
    int l2cap_cmd_sema;
    int controlEndp;
    int eventEndp;
    int inEndp;
    int outEndp;
    uint8_t status;
} bt_device;

bt_device bt_dev = {-1, -1, -1, -1, -1, -1, -1, -1, DS3BT_STATE_USB_DISCONNECTED};

static void usb_probeEndpoint(int devId, UsbEndpointDescriptor *endpoint);
static void bt_config_set(int result, int count, void *arg);
static void bt_release();

int bt_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;
    UsbConfigDescriptor *config = NULL;
    UsbInterfaceDescriptor *intf = NULL;

    DPRINTF("DS3BT: probe: devId=%i\n", devId);

    if ((bt_dev.devId > 0) && (bt_dev.status & DS3BT_STATE_USB_AUTHORIZED)) {
        DPRINTF("DS3BT: Error - only one device allowed !\n");
        return 0;
    }

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("DS3BT: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->bNumConfigurations < 1)
        return 0;

    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (config == NULL) {
        DPRINTF("BT: Error - Couldn't get configuration descriptor\n");
        return 0;
    }

    if ((config->bNumInterfaces < 1) || (config->wTotalLength < (sizeof(UsbConfigDescriptor) + sizeof(UsbInterfaceDescriptor)))) {
        DPRINTF("DS3BT: Error - No interfaces available\n");
        return 0;
    }

    intf = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    DPRINTF("DS3BT: bInterfaceClass %X bInterfaceSubClass %X bInterfaceProtocol %X\n",
            intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol);

    if ((intf->bInterfaceClass != USB_CLASS_WIRELESS_CONTROLLER) ||
        (intf->bInterfaceSubClass != USB_SUBCLASS_RF_CONTROLLER) ||
        (intf->bInterfaceProtocol != USB_PROTOCOL_BLUETOOTH_PROG) ||
        (intf->bNumEndpoints < 3)) {
        return 0;
    }

    return 1;
}

int bt_connect(int devId)
{
    int i;
    int epCount;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;
    UsbEndpointDescriptor *endpoint;
    iop_sema_t SemaData;

    DPRINTF("DS3BT: connect: devId=%i\n", devId);

    if (bt_dev.devId != -1) {
        DPRINTF("DS3BT: Error - only one device allowed !\n");
        return 1;
    }

    bt_dev.status = DS3BT_STATE_USB_DISCONNECTED;

    bt_dev.eventEndp = -1;
    bt_dev.inEndp = -1;
    bt_dev.outEndp = -1;

    bt_dev.controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);

    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);

    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    epCount = interface->bNumEndpoints;

    DPRINTF("DS3BT: Endpoint Count %d \n", epCount);

    endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);
    usb_probeEndpoint(devId, endpoint);

    for (i = 1; i < epCount; i++) {
        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);
        usb_probeEndpoint(devId, endpoint);
    }

    if (bt_dev.eventEndp < 0 || bt_dev.inEndp < 0 || bt_dev.outEndp < 0) {
        bt_release();
        DPRINTF("DS3BT: Error - connect failed: not enough endpoints! \n");
        return -1;
    }

    SemaData.initial = 1;
    SemaData.max = 1;
    SemaData.option = 0;
    SemaData.attr = 0;

    if ((bt_dev.hci_sema = CreateSema(&SemaData)) < 0) {
        DPRINTF("DS3BT: Failed to allocate I/O semaphore.\n");
        return -1;
    }

    if ((bt_dev.l2cap_sema = CreateSema(&SemaData)) < 0) {
        DPRINTF("DS3BT: Failed to allocate I/O semaphore.\n");
        return -1;
    }

    if ((bt_dev.l2cap_cmd_sema = CreateSema(&SemaData)) < 0) {
        DPRINTF("DS3BT: Failed to allocate I/O semaphore.\n");
        return -1;
    }

    bt_dev.devId = devId;

    bt_dev.status = DS3BT_STATE_USB_AUTHORIZED;

    UsbSetDeviceConfiguration(bt_dev.controlEndp, config->bConfigurationValue, bt_config_set, NULL);

    return 0;
}

int bt_disconnect(int devId)
{
    DPRINTF("DS3BT: disconnect: devId=%i\n", devId);

    if (bt_dev.status & DS3BT_STATE_USB_AUTHORIZED) {
        if (bt_dev.eventEndp >= 0)
            UsbCloseEndpoint(bt_dev.eventEndp);

        if (bt_dev.inEndp >= 0)
            UsbCloseEndpoint(bt_dev.inEndp);

        if (bt_dev.outEndp >= 0)
            UsbCloseEndpoint(bt_dev.outEndp);

        bt_release();
    }

    return 0;
}

static void bt_release()
{
    DeleteSema(bt_dev.hci_sema);
    DeleteSema(bt_dev.l2cap_sema);
    DeleteSema(bt_dev.l2cap_cmd_sema);

    bt_dev.devId = -1;
    bt_dev.eventEndp = -1;
    bt_dev.inEndp = -1;
    bt_dev.outEndp = -1;
    bt_dev.controlEndp = -1;
    bt_dev.status = DS3BT_STATE_USB_DISCONNECTED;
}

static void usb_probeEndpoint(int devId, UsbEndpointDescriptor *endpoint)
{
    if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
        if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && bt_dev.outEndp < 0) {
            bt_dev.outEndp = UsbOpenEndpointAligned(devId, endpoint);
            DPRINTF("DS3BT: register Output endpoint id =%i addr=%02X packetSize=%i\n", bt_dev.outEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
        } else if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && bt_dev.inEndp < 0) {
            bt_dev.inEndp = UsbOpenEndpointAligned(devId, endpoint);
            DPRINTF("DS3BT: register Input endpoint id =%i addr=%02X packetSize=%i\n", bt_dev.inEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
        }
    } else if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
        if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && bt_dev.eventEndp < 0) {
            bt_dev.eventEndp = UsbOpenEndpoint(devId, endpoint);
            DPRINTF("DS3BT: register Event endpoint id =%i addr=%02X packetSize=%i\n", bt_dev.eventEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
        }
    }
}

static int chrg_dev = -1;
static int chrg_end = -1;

int chrg_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("CHRG: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("CHRG: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->idVendor == 0x054C && device->idProduct == 0x0268)
        return 1;

    return 0;
}

int chrg_connect(int devId)
{
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;

    DPRINTF("CHRG: connect: devId=%i\n", devId);

    if (chrg_dev != -1) {
        DPRINTF("CHRG: Error - only one device allowed !\n");
        return 1;
    }

    chrg_dev = devId;

    chrg_end = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);

    UsbSetDeviceConfiguration(chrg_end, config->bConfigurationValue, NULL, NULL);

    return 0;
}

int chrg_disconnect(int devId)
{
    DPRINTF("CHRG: disconnect: devId=%i\n", devId);

    if (chrg_dev == devId) {
        if (chrg_end >= 0)
            UsbCloseEndpoint(chrg_end);

        chrg_dev = -1;
        chrg_end = -1;
    }

    return 0;
}

UsbDriver chrg_driver = {NULL, NULL, "chrg", chrg_probe, chrg_connect, chrg_disconnect};

/* PS Remote Reports */
static uint8_t feature_F4_report[] =
    {
        0x42, 0x03, 0x00, 0x00};

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

static uint8_t led_patterns[][2] =
    {
        {0x1C, 0x02},
        {0x1A, 0x04},
        {0x16, 0x08},
        {0x0E, 0x10},
};

static uint8_t power_level[] =
    {
        0x00, 0x00, 0x02, 0x06, 0x0E, 0x1E};

// Taken from nefarius' SCPToolkit
// https://github.com/nefarius/ScpToolkit/blob/master/ScpControl/ScpControl.ini
// Valid MAC addresses used by Sony
static uint8_t GenuineMacAddress[][3] =
    {
        // Bluetooth chips by ALPS ELECTRIC CO., LTD
        {0x00, 0x02, 0xC7},
        {0x00, 0x06, 0xF5},
        {0x00, 0x06, 0xF7},
        {0x00, 0x07, 0x04},
        {0x00, 0x16, 0xFE},
        {0x00, 0x19, 0xC1},
        {0x00, 0x1B, 0xFB},
        {0x00, 0x1E, 0x3D},
        {0x00, 0x21, 0x4F},
        {0x00, 0x23, 0x06},
        {0x00, 0x24, 0x33},
        {0x00, 0x26, 0x43},
        {0x00, 0xA0, 0x79},
        {0x04, 0x76, 0x6E},
        {0x04, 0x98, 0xF3},
        {0x28, 0xA1, 0x83},
        {0x34, 0xC7, 0x31},
        {0x38, 0xC0, 0x96},
        {0x60, 0x38, 0x0E},
        {0x64, 0xD4, 0xBD},
        {0xAC, 0x7A, 0x4D},
        {0xE0, 0x75, 0x0A},
        {0xE0, 0xAE, 0x5E},
        {0xFC, 0x62, 0xB9},
        // Bluetooth chips by AzureWave Technology Inc.
        {0xE0, 0xB9, 0xA5},
        {0xDC, 0x85, 0xDE},
        {0xD0, 0xE7, 0x82},
        {0xB0, 0xEE, 0x45},
        {0xAC, 0x89, 0x95},
        {0xA8, 0x1D, 0x16},
        {0x94, 0xDB, 0xC9},
        {0x80, 0xD2, 0x1D},
        {0x80, 0xA5, 0x89},
        {0x78, 0x18, 0x81},
        {0x74, 0xF0, 0x6D},
        {0x74, 0xC6, 0x3B},
        {0x74, 0x2F, 0x68},
        {0x6C, 0xAD, 0xF8},
        {0x6C, 0x71, 0xD9},
        {0x60, 0x5B, 0xB4},
        {0x5C, 0x96, 0x56},
        {0x54, 0x27, 0x1E},
        {0x4C, 0xAA, 0x16},
        {0x48, 0x5D, 0x60},
        {0x44, 0xD8, 0x32},
        {0x40, 0xE2, 0x30},
        {0x38, 0x4F, 0xF0},
        {0x28, 0xC2, 0xDD},
        {0x24, 0x0A, 0x64},
        {0x1C, 0x4B, 0xD6},
        {0x08, 0xA9, 0x5A},
        {0x00, 0x25, 0xD3},
        {0x00, 0x24, 0x23},
        {0x00, 0x22, 0x43},
        {0x00, 0x15, 0xAF},
        //fake with AirohaTechnologyCorp's Chip
        {0x0C, 0xFC, 0x83}};

/* variables used by high level HCI task */
static uint16_t hci_counter_; // counter used for bluetooth HCI loops

/* variables filled from HCI event management */
static volatile uint8_t hci_event_flag_; // flag of received bluetooth events

/* variables used by L2CAP */
static volatile uint8_t l2cap_event_status_;
static uint8_t l2cap_txid_; // packet id increments for each packet sent
/* L2CAP CID name space: 0x0040-0xffff dynamically allocated */
static uint16_t command_scid_;   // Channel endpoint on command source
static uint16_t interrupt_scid_; // Channel endpoint on interrupt source

/* variables used by Bluetooth HID */
static uint8_t hid_flags_;
static uint8_t ds3bt_bdaddr_[6];

static uint8_t hci_reset();
static uint8_t hci_write_scan_enable(uint8_t conf);
static uint8_t hci_accept_connection(uint8_t *bdaddr);
static uint8_t hci_reject_connection(uint8_t *bdaddr);
static uint8_t hci_disconnect(uint16_t handle);
static uint8_t hci_change_connection_type(uint8_t pad);
static uint8_t HCI_Command(uint16_t nbytes, uint8_t *dataptr);

static uint8_t l2cap_connect_response(uint8_t rxid, uint16_t dcid, uint16_t scid, uint8_t pad);
static uint8_t l2cap_configure(uint16_t dcid, uint8_t pad);
static uint8_t l2cap_config_response(uint8_t rxid, uint16_t dcid, uint8_t pad);
static uint8_t l2cap_disconnect_response(uint8_t rxid, uint16_t scid, uint16_t dcid, uint8_t pad);
static uint8_t L2CAP_Command(uint8_t *data, uint8_t length, uint8_t pad);

static uint8_t initPSController(int pad);
static void readReport(uint8_t *data, int bytes, int pad);
static uint8_t writeReport(uint8_t *data, uint8_t length, int pad);

static void HCI_task(uint8_t pad);
static void L2CAP_task(uint8_t pad);
static uint8_t HCI_event_task(int result);
static uint8_t L2CAP_event_task(int result, int bytes);

static uint8_t hci_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};
static uint8_t l2cap_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};
static uint8_t hci_cmd_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};
static uint8_t l2cap_cmd_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};

static void DS3BT_init();
static int Rumble(uint8_t lrum, uint8_t rrum, int pad);
static int LEDRumble(uint8_t led, uint8_t lrum, uint8_t rrum, int pad);

static uint8_t current_pad;
static uint8_t enable_pad;
static uint8_t press_emu;

typedef struct
{
    uint16_t hci_handle_;     //connection handle
    uint16_t command_dcid_;   // Channel endpoint on command destination
    uint16_t interrupt_dcid_; // Channel endpoint on interrupt destination
    uint8_t hci_state_;       // current state of bluetooth HCI connection
    uint8_t l2cap_state_;     // current state of L2CAP connection
    uint8_t status_;
    uint8_t oldled;
    uint8_t oldlrumble;
    uint8_t oldrrumble;
    uint8_t enabled;
    uint8_t type;
    uint8_t data[18];
    uint8_t analog_btn;
} ds3bt_pad_t;

#define MAX_PADS 2

ds3bt_pad_t ds3pad[MAX_PADS];

static void DS3BT_init()
{
    uint8_t i;

    l2cap_txid_ = 1;
    command_scid_ = 0x0040;   // L2CAP local CID for HID_Control
    interrupt_scid_ = 0x0041; // L2CAP local CID for HID_Interrupt

    hid_flags_ = 0;

    for (i = 0; i < MAX_PADS; i++) {
        ds3pad[i].hci_handle_ = 0x0FFF;
        ds3pad[i].status_ = bt_dev.status;
        ds3pad[i].status_ |= DS3BT_STATE_USB_CONFIGURED;
        ds3pad[i].hci_state_ = HCI_RESET_STATE;
        ds3pad[i].l2cap_state_ = L2CAP_DOWN_STATE;

        ds3pad[i].data[0] = 0xFF;
        ds3pad[i].data[1] = 0xFF;
        ds3pad[i].enabled = (enable_pad >> i) & 1;
        ds3pad[i].type = 0;

        mips_memset(&ds3pad[i].data[2], 0x7F, 4);
        mips_memset(&ds3pad[i].data[6], 0x00, 12);
        ds3pad[i].analog_btn = 0;
    }

    hci_counter_ = 10;
    current_pad = 0;
    press_emu = 0;

    hci_reset();
}

static void hci_event_cb(int resultCode, int bytes, void *arg)
{
    uint8_t pad;

    PollSema(bt_dev.hci_sema);

    //DPRINTF("hci_event_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);

    pad = HCI_event_task(resultCode);
    HCI_task(pad);

    SignalSema(bt_dev.hci_sema);

    UsbInterruptTransfer(bt_dev.eventEndp, hci_buf, MAX_BUFFER_SIZE, hci_event_cb, arg);
}

static void l2cap_event_cb(int resultCode, int bytes, void *arg)
{
    int pad;
    static uint8_t ret_ctr = 0;

    PollSema(bt_dev.l2cap_sema);

    //DPRINTF("l2cap_event_cb: res %d, bytes %d, arg %p\n", resultCode, bytes, arg);

    pad = L2CAP_event_task(resultCode, bytes);
    L2CAP_task(pad);

    if (pad >= MAX_PADS) {
        UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, (void *)-1);
        return;
    }

    if (ds3pad[pad].l2cap_state_ != L2CAP_READY_STATE) { //ds3 is connecting
        DelayThread(14000);                              //fix for some bt adapters
        UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, (void *)-1);
    } else {                                                            //ds3 is running
        if ((int)arg != -1 && pad != (int)arg && (int)arg < MAX_PADS) { //check if we get what was requested
            if (ret_ctr == 20) {
                ret_ctr = 0;
                SignalSema(bt_dev.l2cap_sema);
            } else {
                ret_ctr++;
                UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, arg); //try again
            }
        } else { //we got what we wanted
            SignalSema(bt_dev.l2cap_sema);
        }
    }
}

static void l2cap_cmd_cb(int resultCode, int bytes, void *arg)
{
    //DPRINTF("l2cap_cmd_cb: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);
    SignalSema(bt_dev.l2cap_cmd_sema);
}

static void bt_config_set(int result, int count, void *arg)
{
    UsbInterruptTransfer(bt_dev.eventEndp, hci_buf, MAX_BUFFER_SIZE, hci_event_cb, NULL);
    UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, (void *)-1);

    SignalSema(bt_dev.hci_sema);
    SignalSema(bt_dev.l2cap_sema);

    DS3BT_init();
}

static uint8_t HCI_event_task(int result)
{
    uint8_t i, pad;

    pad = current_pad;

    if (!result) {
        /*  buf[0] = Event Code                            */
        /*  buf[1] = Parameter Total Length                */
        /*  buf[n] = Event Parameters based on each event  */
        DPRINTF("HCI event = 0x%x\n", hci_buf[0]);
        switch (hci_buf[0]) { // switch on event type
            case HCI_EVENT_COMMAND_COMPLETE:
                hci_event_flag_ |= HCI_FLAG_COMMAND_COMPLETE;

                DPRINTF("Command OK = 0x%x", hci_buf[3]);
                DPRINTF(" Returned = 0x%x\n", hci_buf[5]);
                break;

            case HCI_EVENT_COMMAND_STATUS:
                hci_event_flag_ |= HCI_FLAG_COMMAND_STATUS;

                if (hci_buf[2]) // show status on serial if not OK
                {
                    DPRINTF("HCI Command Failed: \n");
                    DPRINTF("\tStatus = %x\n", hci_buf[2]);
                    DPRINTF("\tCommand_OpCode(OGF) = %x\n", ((hci_buf[5] & 0xFC) >> 2));
                    DPRINTF("\tCommand_OpCode(OCF) = %x%x\n", (hci_buf[5] & 0x03), hci_buf[4]);
                }

                break;

            case HCI_EVENT_CONNECT_COMPLETE:
                if (!hci_buf[2]) // check if connected OK
                {
                    // store the handle for the ACL connection
                    ds3pad[pad].hci_handle_ = hci_buf[3] | ((hci_buf[4] & 0x0F) << 8); //pad = current_pad

                    DPRINTF("HCI event Connect Complete = 0x%x\n", ds3pad[pad].hci_handle_);
                    hci_event_flag_ |= HCI_FLAG_CONNECT_COMPLETE;
                } else {
                    DPRINTF("Error on Connect Complete = 0x%x\n", hci_buf[2]);
                }
                break;

            case HCI_EVENT_NUM_COMPLETED_PKT:

                DPRINTF("HCI Number Of Completed Packets Event: \n");
                DPRINTF("\tNumber_of_Handles = 0x%x\n", hci_buf[2]);
                for (i = 0; i < hci_buf[2]; i++) {
                    DPRINTF("\tConnection_Handle = 0x%x\n", (hci_buf[3 + i] | ((hci_buf[4 + i] & 0x0F) << 8)));
                }

                if (hci_buf[2] == 1) {
                    for (i = 0; i < MAX_PADS; i++) //detect pad
                    {
                        if (ds3pad[i].hci_handle_ == (hci_buf[3] | ((hci_buf[4] & 0x0F) << 8))) {
                            pad = i;
                            break;
                        }
                    }
                }
                break;

            case HCI_EVENT_QOS_SETUP_COMPLETE:
                break;

            case HCI_EVENT_DISCONN_COMPLETE:
                hci_event_flag_ |= HCI_FLAG_DISCONN_COMPLETE;
                DPRINTF("HCI Disconnection Complete Event: \n");
                DPRINTF("\tStatus = 0x%x\n", hci_buf[2]);
                DPRINTF("\tConnection_Handle = 0x%x\n", (hci_buf[3] | ((hci_buf[4] & 0x0F) << 8)));
                DPRINTF("\tReason = 0x%x\n", hci_buf[5]);

                for (i = 0; i < MAX_PADS; i++) //detect pad
                {
                    if (ds3pad[i].hci_handle_ == (hci_buf[3] | ((hci_buf[4] & 0x0F) << 8))) {
                        pad = i;
                        break;
                    }
                }

                break;

            case HCI_EVENT_CONNECT_REQUEST:
                hci_event_flag_ |= HCI_FLAG_INCOMING_REQUEST;
                DPRINTF("Connection Requested by BD_ADDR: \n");
                for (i = 0; i < 6; i++) {
                    ds3bt_bdaddr_[i] = (unsigned char)hci_buf[2 + i];
                    DPRINTF("%x", ds3bt_bdaddr_[i]);
                    if (i < 5)
                        DPRINTF(":");
                }

                DPRINTF(" LINK: 0x%x\n", hci_buf[11]);

                for (i = 0; i < MAX_PADS; i++) //find free slot
                {
                    if (!(ds3pad[i].status_ & DS3BT_STATE_RUNNING) && ds3pad[i].enabled) {
                        if (ds3pad[i].status_ & DS3BT_STATE_CONNECTED) {
                            if (ds3pad[i].l2cap_state_ == L2CAP_DISCONNECT_STATE) //if we're waiting for hci disconnect event
                                continue;
                            else
                                hci_disconnect(ds3pad[i].hci_handle_); //try to disconnect
                        }

                        current_pad = i;
                        break;
                    }
                }

                if (i >= MAX_PADS) //no free slot
                {
                    hci_reject_connection(ds3bt_bdaddr_);
                    current_pad = pad;
                    return MAX_PADS;
                }

                pad = current_pad;

                ds3pad[pad].type = 0xA2; //fake ds3

                for (i = 0; i < sizeof(GenuineMacAddress) / 3; i++) //check if ds3 is genuine
                {
                    if (ds3bt_bdaddr_[5] == GenuineMacAddress[i][0] &&
                        ds3bt_bdaddr_[4] == GenuineMacAddress[i][1] &&
                        ds3bt_bdaddr_[3] == GenuineMacAddress[i][2]) {
                        ds3pad[pad].type = HID_THDR_SET_REPORT_OUTPUT;
                        break;
                    }
                }

                ds3pad[pad].hci_state_ = HCI_CONNECT_IN_STATE;
                ds3pad[pad].l2cap_state_ = L2CAP_INIT_STATE;
                ds3pad[pad].status_ &= ~DS3BT_STATE_CONNECTED;
                ds3pad[pad].status_ &= ~DS3BT_STATE_RUNNING;

                l2cap_event_status_ &= ~L2CAP_EV_COMMAND_CONNECTED;

                break;

            case HCI_EVENT_ROLE_CHANGED:

                DPRINTF("Role Change STATUS: 0x%x\n", hci_buf[2]);

                DPRINTF(" BD_ADDR: ");
                for (i = 0; i < 6; i++) {
                    DPRINTF("%x", (unsigned char)hci_buf[3 + i]);
                    if (i < 5)
                        DPRINTF(":");
                }

                DPRINTF(" ROLE: 0x%x\n", hci_buf[9]);
                break;

            case HCI_EVENT_MAX_SLOT_CHANGE:

                DPRINTF("Max Slot Change: \n");
                DPRINTF("\tConnection_Handle = 0x%x\n", (hci_buf[2] | ((hci_buf[3] & 0x0F) << 8)));
                DPRINTF("\tLMP Max Slots = 0x%x\n", hci_buf[5]);
                break;

            case HCI_EVENT_CHANGED_CONNECTION_TYPE:

                DPRINTF("Packet Type Changed STATUS: 0x%x \n", hci_buf[2]);
                DPRINTF(" TYPE: %x \n", (hci_buf[5] | (hci_buf[6] << 8)));

                break;

            case HCI_EVENT_PAGE_SR_CHANGED:
                break;

            default:
                DPRINTF("Unmanaged Event: 0x%x\n", hci_buf[0]);
                break;
        } // switch (buf[0])
    }

    return pad;
}

static void HCI_task(uint8_t pad)
{
    int i;

    if (pad >= MAX_PADS)
        return;

    switch (ds3pad[pad].hci_state_) {
        case HCI_INIT_STATE:
            // wait until we have looped 10 times to clear any old events
            if (hci_timeout) {
                for (i = 0; i < MAX_PADS; i++) {
                    if (ds3pad[i].status_ & DS3BT_STATE_RUNNING)
                        break;
                }

                if (i == MAX_PADS) {
                    ds3pad[pad].hci_state_ = HCI_RESET_STATE;
                    hci_reset();
                }

                hci_counter_ = 10;
            }
            break;

        case HCI_RESET_STATE:
            if (hci_command_complete) {
                DPRINTF("HCI Reset complete\n");
                hci_write_scan_enable(SCAN_ENABLE_NOINQ_ENPAG);
                ds3pad[pad].hci_state_ = HCI_CONNECT_IN_STATE;
                hci_event_flag_ &= ~HCI_FLAG_INCOMING_REQUEST;
            }
            if (hci_timeout) {
                DPRINTF("No response to HCI Reset\n");
                ds3pad[pad].hci_state_ = HCI_INIT_STATE;
                hci_counter_ = 10;
            }
            break;

        case HCI_CONNECT_IN_STATE:
            if (hci_incoming_connect_request) {
                hci_accept_connection(ds3bt_bdaddr_);
                DPRINTF("PS Remote Connected\n");
                ds3pad[pad].hci_state_ = HCI_CHANGE_CONNECTION;
                hci_event_flag_ &= ~HCI_FLAG_CONNECT_COMPLETE;
            }
            break;

        case HCI_CHANGE_CONNECTION:
            if (hci_connect_complete) {
                hci_change_connection_type(pad);

                ds3pad[pad].hci_state_ = HCI_CONNECTED_STATE;

                ds3pad[pad].status_ |= DS3BT_STATE_CONNECTED;
                hci_event_flag_ &= ~HCI_FLAG_DISCONN_COMPLETE;
            }
            break;

        case HCI_CONNECTED_STATE:
            if (hci_disconn_complete) {
                DPRINTF("PS Remote Disconnected pad %d\n", pad);

                hci_event_flag_ &= ~HCI_FLAG_DISCONN_COMPLETE;

                ds3pad[pad].status_ &= ~DS3BT_STATE_CONNECTED;
                ds3pad[pad].status_ &= ~DS3BT_STATE_RUNNING;

                hci_counter_ = 10;

                if (ds3pad[pad].type != HID_THDR_SET_REPORT_OUTPUT) {
                    ds3pad[pad].l2cap_state_ = L2CAP_DISCONNECT_STATE;
                    for (i = 0; i < MAX_PADS; i++) {
                        if (ds3pad[i].status_ & DS3BT_STATE_RUNNING)
                            break;
                    }

                    if (i == MAX_PADS)
                        UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, (void *)-1);
                }

                SignalSema(bt_dev.l2cap_sema);
            }
            break;

        default:
            break;
    } // switch (hci_state_)
}

/************************************************************/
/* HCI Commands                                             */
/************************************************************/

static uint8_t hci_reset()
{
    hci_event_flag_ = 0; // clear all the flags

    hci_cmd_buf[0] = HCI_OCF_RESET;
    hci_cmd_buf[1] = HCI_OGF_CTRL_BBAND;
    hci_cmd_buf[2] = 0x00; // Parameter Total Length = 0

    return HCI_Command(3, hci_cmd_buf);
}

static uint8_t hci_write_scan_enable(uint8_t conf)
{
    hci_event_flag_ &= ~HCI_FLAG_COMMAND_COMPLETE;

    hci_cmd_buf[0] = HCI_OCF_WRITE_SCAN_ENABLE;
    hci_cmd_buf[1] = HCI_OGF_CTRL_BBAND;
    hci_cmd_buf[2] = 0x01;
    hci_cmd_buf[3] = conf;
    return HCI_Command(4, hci_cmd_buf);
}

static uint8_t hci_accept_connection(uint8_t *bdaddr)
{
    hci_event_flag_ &= ~(HCI_FLAG_INCOMING_REQUEST);

    hci_cmd_buf[0] = HCI_OCF_ACCEPT_CONNECTION; // HCI OCF = 9
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;        // HCI OGF = 1
    hci_cmd_buf[2] = 0x07;                      // parameter length 7
    hci_cmd_buf[3] = *bdaddr;                   // 6 octet bluetooth address
    hci_cmd_buf[4] = *(bdaddr + 1);
    hci_cmd_buf[5] = *(bdaddr + 2);
    hci_cmd_buf[6] = *(bdaddr + 3);
    hci_cmd_buf[7] = *(bdaddr + 4);
    hci_cmd_buf[8] = *(bdaddr + 5);
    hci_cmd_buf[9] = 1; //switch role to slave

    return HCI_Command(10, hci_cmd_buf);
}

static uint8_t hci_reject_connection(uint8_t *bdaddr)
{
    hci_event_flag_ &= ~(HCI_FLAG_INCOMING_REQUEST);

    hci_cmd_buf[0] = HCI_OCF_REJECT_CONNECTION; // HCI OCF = A
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;        // HCI OGF = 1
    hci_cmd_buf[2] = 0x07;                      // parameter length 7
    hci_cmd_buf[3] = *bdaddr;                   // 6 octet bluetooth address
    hci_cmd_buf[4] = *(bdaddr + 1);
    hci_cmd_buf[5] = *(bdaddr + 2);
    hci_cmd_buf[6] = *(bdaddr + 3);
    hci_cmd_buf[7] = *(bdaddr + 4);
    hci_cmd_buf[8] = *(bdaddr + 5);
    hci_cmd_buf[9] = 0x09; //reason max connection

    return HCI_Command(10, hci_cmd_buf);
}

static uint8_t hci_disconnect(uint16_t handle)
{
    hci_cmd_buf[0] = 0x06;                            // HCI OCF = 6
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;              // HCI OGF = 1
    hci_cmd_buf[2] = 0x03;                            // parameter length = 3
    hci_cmd_buf[3] = (uint8_t)(handle & 0xFF);        //connection handle - low byte
    hci_cmd_buf[4] = (uint8_t)((handle >> 8) & 0x0F); //connection handle - high byte
    hci_cmd_buf[5] = 0x13;                            // reason

    return HCI_Command(6, hci_cmd_buf);
}

static uint8_t hci_change_connection_type(uint8_t pad)
{
    hci_event_flag_ &= ~HCI_FLAG_COMMAND_COMPLETE;

    hci_cmd_buf[0] = HCI_OCF_CHANGE_CONNECTION_TYPE;
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;
    hci_cmd_buf[2] = 0x04;                                      // parameter length 4
    hci_cmd_buf[3] = (uint8_t)(ds3pad[pad].hci_handle_ & 0xff); // HCI handle with PB,BC flag
    hci_cmd_buf[4] = (uint8_t)(ds3pad[pad].hci_handle_ >> 8);
    hci_cmd_buf[5] = 0x18; // Packet Type: 0xcc18
    hci_cmd_buf[6] = 0xcc;

    return HCI_Command(7, hci_cmd_buf);
}
/* perform HCI Command */
static uint8_t HCI_Command(uint16_t nbytes, uint8_t *dataptr)
{
    return UsbControlTransfer(bt_dev.controlEndp, bmREQ_HCI_OUT, HCI_COMMAND_REQ, 0, 0, nbytes, dataptr, NULL, NULL);
}

static uint8_t L2CAP_event_task(int result, int bytes)
{
    uint8_t pad = 0xFF;

    if (!result) {
        for (pad = 0; pad < MAX_PADS; pad++) {
            if (acl_handle_ok(ds3pad[pad].hci_handle_))
                break;
        }

        if (pad >= MAX_PADS) {
            DPRINTF("L2CAP Wrong Handle = 0x%x\n", ((l2cap_buf[0] | (l2cap_buf[1] << 8)) & 0x0FFF));
            return pad;
        }

        if (acl_handle_ok(ds3pad[pad].hci_handle_)) {
            if (l2cap_control) {
                DPRINTF("L2CAP Signaling Command = 0x%x pad %d\n", l2cap_buf[8], pad);
                if (l2cap_command_reject) {
                    DPRINTF("ID = 0x%x", l2cap_buf[9]);
                    DPRINTF(" Reason = 0x%x", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                    DPRINTF(" DATA = 0x%x\n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));
                } else if (l2cap_connection_request) {
                    DPRINTF("Connection Request ID = 0x%x", l2cap_buf[9]);
                    DPRINTF(" PSM = 0x%x", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                    DPRINTF(" SCID = 0x%x\n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));

                    if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == L2CAP_PSM_WRITE) {
                        ds3pad[pad].command_dcid_ = l2cap_buf[14] | (l2cap_buf[15] << 8);
                        l2cap_connect_response(l2cap_buf[9], command_scid_, ds3pad[pad].command_dcid_, pad);
                        l2cap_event_status_ |= L2CAP_EV_COMMAND_CONNECTED;
                    } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == L2CAP_PSM_READ) {
                        ds3pad[pad].interrupt_dcid_ = l2cap_buf[14] | (l2cap_buf[15] << 8);
                        l2cap_connect_response(l2cap_buf[9], interrupt_scid_, ds3pad[pad].interrupt_dcid_, pad);
                        l2cap_event_status_ |= L2CAP_EV_INTERRUPT_CONNECTED;
                    }
                } else if (l2cap_configuration_request) {

                    DPRINTF("Conf Request ID = 0x%x\n", l2cap_buf[9]);
                    DPRINTF(" LEN = 0x%x", (l2cap_buf[10] | (l2cap_buf[11] << 8)));
                    DPRINTF(" SCID = 0x%x", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                    DPRINTF(" FLAG = 0x%x\n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));

                    if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == command_scid_) {
                        l2cap_event_status_ |= L2CAP_EV_COMMAND_CONFIG_REQ;
                        l2cap_config_response(l2cap_buf[9], ds3pad[pad].command_dcid_, pad);
                    } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == interrupt_scid_) {
                        l2cap_event_status_ |= L2CAP_EV_INTERRUPT_CONFIG_REQ;
                        l2cap_config_response(l2cap_buf[9], ds3pad[pad].interrupt_dcid_, pad);
                    }
                } else if (l2cap_configuration_response) {

                    DPRINTF("Conf Response ID = 0x%x\n", l2cap_buf[9]);
                    DPRINTF(" LEN = 0x%x", (l2cap_buf[10] | (l2cap_buf[11] << 8)));
                    DPRINTF(" SCID = 0x%x", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                    DPRINTF(" FLAG = 0x%x", (l2cap_buf[14] | (l2cap_buf[15] << 8)));
                    DPRINTF(" RESULT = 0x%x\n", (l2cap_buf[16] | (l2cap_buf[17] << 8)));

                    if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == command_scid_) {
                        l2cap_event_status_ |= L2CAP_EV_COMMAND_CONFIGURED;
                    } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == interrupt_scid_) {
                        l2cap_event_status_ |= L2CAP_EV_INTERRUPT_CONFIGURED;
                    }
                } else if (l2cap_disconnect_request) {
                    DPRINTF("Disconnect Req  SCID = 0x%x\n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));

                    ds3pad[pad].l2cap_state_ = L2CAP_DISCONNECT_STATE;

                    if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == command_scid_) {
                        l2cap_event_status_ |= L2CAP_EV_COMMAND_DISCONNECT_REQ;
                        l2cap_disconnect_response(l2cap_buf[9], command_scid_, ds3pad[pad].command_dcid_, pad);
                    } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == interrupt_scid_) {
                        l2cap_event_status_ |= L2CAP_EV_INTERRUPT_DISCONNECT_REQ;
                        l2cap_disconnect_response(l2cap_buf[9], command_scid_, ds3pad[pad].command_dcid_, pad);
                    }
                }
            } else if (l2cap_interrupt) {
                readReport(l2cap_buf, bytes, pad);
            } else if (l2cap_command) {
                if (hid_handshake_success) {
                    hid_flags_ |= HID_FLAG_COMMAND_SUCCESS;
                }
            }
        } // acl_handle_ok
    }     // !rcode

    return pad;
}

static void L2CAP_task(uint8_t pad)
{
    if (pad >= MAX_PADS)
        return;

    switch (ds3pad[pad].l2cap_state_) {
        case L2CAP_INIT_STATE:
            DPRINTF("L2CAP_I\n");
            l2cap_event_status_ &= ~L2CAP_EV_COMMAND_DISCONNECT_REQ;
            l2cap_event_status_ &= ~L2CAP_EV_INTERRUPT_DISCONNECT_REQ;
            ds3pad[pad].l2cap_state_ = L2CAP_CONTROL_CONNECTING_STATE;

        case L2CAP_CONTROL_CONNECTING_STATE:
            DPRINTF("L2CAP_C1 \n");
            if (l2cap_command_connected) {
                l2cap_event_status_ &= ~L2CAP_EV_COMMAND_CONFIG_REQ;
                ds3pad[pad].l2cap_state_ = L2CAP_CONTROL_REQUEST_STATE;
            }
            break;

        case L2CAP_CONTROL_REQUEST_STATE:
            DPRINTF("L2CAP_C2\n");
            if (l2cap_command_request) {
                l2cap_event_status_ &= ~L2CAP_EV_COMMAND_CONFIGURED;
                l2cap_configure(ds3pad[pad].command_dcid_, pad);
                ds3pad[pad].l2cap_state_ = L2CAP_CONTROL_CONFIGURING_STATE;
            }
            break;

        case L2CAP_CONTROL_CONFIGURING_STATE:
            DPRINTF("L2CAP_C3\n");
            if (l2cap_command_configured) {
                l2cap_event_status_ &= ~L2CAP_EV_INTERRUPT_CONNECTED;
                ds3pad[pad].l2cap_state_ = L2CAP_INTERRUPT_CONNECTING_STATE;
            }
            break;

        case L2CAP_INTERRUPT_CONNECTING_STATE:
            DPRINTF("L2CAP_I1\n");
            if (l2cap_interrupt_connected) {
                l2cap_event_status_ &= ~L2CAP_EV_INTERRUPT_CONFIG_REQ;
                l2cap_event_status_ &= ~L2CAP_EV_INTERRUPT_CONFIGURED;
                ds3pad[pad].l2cap_state_ = L2CAP_INTERRUPT_REQUEST_STATE;
            }
            break;

        case L2CAP_INTERRUPT_REQUEST_STATE:
            DPRINTF("L2CAP_I2\n");
            if (l2cap_interrupt_request) {
                l2cap_configure(ds3pad[pad].interrupt_dcid_, pad);
                ds3pad[pad].l2cap_state_ = L2CAP_INTERRUPT_CONFIGURING_STATE;
            }
            break;

        case L2CAP_INTERRUPT_CONFIGURING_STATE:
            DPRINTF("L2CAP_I3\n");
            if (l2cap_interrupt_configured) {
                ds3pad[pad].l2cap_state_ = L2CAP_CONNECTED_STATE;
            } else
                break;

        /* Established L2CAP */
        case L2CAP_CONNECTED_STATE:
            DPRINTF("L2CAP_S\n");
            hid_flags_ = 0;

            initPSController(pad);
            ds3pad[pad].l2cap_state_ = L2CAP_LED_STATE;
            break;

        case L2CAP_LED_STATE:
            DPRINTF("L2CAP_LED\n");
            if (hid_command_success) {
                if (ds3pad[pad].type == HID_THDR_SET_REPORT_OUTPUT) {
                    ds3pad[pad].l2cap_state_ = L2CAP_LED_STATE_END;
                } else {
                    ds3pad[pad].l2cap_state_ = L2CAP_READY_STATE;
                    ds3pad[pad].status_ |= DS3BT_STATE_RUNNING;
                }
                LEDRumble((pad + 1) << 1, ds3pad[pad].oldlrumble, ds3pad[pad].oldrrumble, pad);
            }
            break;

        case L2CAP_LED_STATE_END:
            DPRINTF("L2CAP_LED_END\n");
            if (hid_command_success) {
                ds3pad[pad].l2cap_state_ = L2CAP_READY_STATE;
                ds3pad[pad].status_ |= DS3BT_STATE_RUNNING;
            }
            break;

        case L2CAP_READY_STATE:
            break;

        case L2CAP_DISCONNECT_STATE:
            DPRINTF("L2CAP_D\n");
            if (l2cap_interrupt_disconnected || l2cap_command_disconnected) {
                ds3pad[pad].status_ &= ~DS3BT_STATE_RUNNING;
            }
            break;

        default:
            break;
    }
}

/************************************************************/
/* L2CAP Commands                                            */
/************************************************************/
static uint8_t l2cap_connect_response(uint8_t rxid, uint16_t dcid, uint16_t scid, uint8_t pad)
{
    uint8_t cmd_buf[12];

    cmd_buf[0] = L2CAP_CMD_CONNECTION_RESPONSE; // Code
    cmd_buf[1] = rxid;                          // Identifier
    cmd_buf[2] = 0x08;                          // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (uint8_t)(dcid & 0xff); // Destination CID (Our)
    cmd_buf[5] = (uint8_t)(dcid >> 8);
    cmd_buf[6] = (uint8_t)(scid & 0xff); // Source CID (PS Remote)
    cmd_buf[7] = (uint8_t)(scid >> 8);
    cmd_buf[8] = 0x00; // Result
    cmd_buf[9] = 0x00;
    cmd_buf[10] = 0x00; // Status
    cmd_buf[11] = 0x00;

    return L2CAP_Command((uint8_t *)cmd_buf, 12, pad);
}

static uint8_t l2cap_configure(uint16_t dcid, uint8_t pad)
{
    uint8_t cmd_buf[12];

    cmd_buf[0] = L2CAP_CMD_CONFIG_REQUEST; // Code
    cmd_buf[1] = (uint8_t)(l2cap_txid_++); // Identifier
    cmd_buf[2] = 0x08;                     // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (uint8_t)(dcid & 0xff); // Destination CID
    cmd_buf[5] = (uint8_t)(dcid >> 8);
    cmd_buf[6] = 0x00; // Flags
    cmd_buf[7] = 0x00;
    cmd_buf[8] = 0x01;  // Config Opt: type = MTU (Maximum Transmission Unit)
    cmd_buf[9] = 0x02;  // Config Opt: length
    cmd_buf[10] = 0x40; // Config Opt: data = maximum SDU size is 672 octets
    cmd_buf[11] = 0x00;

    return L2CAP_Command((uint8_t *)cmd_buf, 12, pad);
}

static uint8_t l2cap_config_response(uint8_t rxid, uint16_t dcid, uint8_t pad)
{
    uint8_t cmd_buf[10];

    cmd_buf[0] = L2CAP_CMD_CONFIG_RESPONSE; // Code
    cmd_buf[1] = rxid;                      // Identifier
    cmd_buf[2] = 0x06;                      // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (uint8_t)(dcid & 0xff); // Source CID
    cmd_buf[5] = (uint8_t)(dcid >> 8);
    cmd_buf[6] = 0x00; // Result
    cmd_buf[7] = 0x00;
    cmd_buf[8] = 0x00; // Config
    cmd_buf[9] = 0x00;

    return L2CAP_Command((uint8_t *)cmd_buf, 10, pad);
}

static uint8_t l2cap_disconnect_response(uint8_t rxid, uint16_t scid, uint16_t dcid, uint8_t pad)
{
    uint8_t cmd_buf[8];

    cmd_buf[0] = L2CAP_CMD_DISCONNECT_RESPONSE; // Code
    cmd_buf[1] = rxid;                          // Identifier
    cmd_buf[2] = 0x04;                          // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (uint8_t)(dcid & 0xff); // Destination CID
    cmd_buf[5] = (uint8_t)(dcid >> 8);
    cmd_buf[6] = (uint8_t)(scid & 0xff); // Source CID
    cmd_buf[7] = (uint8_t)(scid >> 8);

    return L2CAP_Command((uint8_t *)cmd_buf, 8, pad);
}

static uint8_t L2CAP_Command(uint8_t *data, uint8_t length, uint8_t pad)
{
    PollSema(bt_dev.l2cap_cmd_sema);

    l2cap_cmd_buf[0] = (uint8_t)(ds3pad[pad].hci_handle_ & 0xff); // HCI handle with PB,BC flag
    l2cap_cmd_buf[1] = (uint8_t)(((ds3pad[pad].hci_handle_ >> 8) & 0x0f) | 0x20);
    l2cap_cmd_buf[2] = (uint8_t)((4 + length) & 0xff); // HCI ACL total data length
    l2cap_cmd_buf[3] = (uint8_t)((4 + length) >> 8);
    l2cap_cmd_buf[4] = (uint8_t)(length & 0xff); // L2CAP header: Length
    l2cap_cmd_buf[5] = (uint8_t)(length >> 8);
    l2cap_cmd_buf[6] = 0x01; // L2CAP header: Channel ID
    l2cap_cmd_buf[7] = 0x00; // L2CAP Signalling channel over ACL-U logical link

    mips_memcpy(&l2cap_cmd_buf[8], data, length);

    // output on endpoint 2
    return UsbBulkTransfer(bt_dev.outEndp, l2cap_cmd_buf, (8 + length), l2cap_cmd_cb, NULL);
}

/************************************************************/
/* HID Commands                                             */
/************************************************************/

static uint8_t initPSController(int pad)
{
    uint8_t init_buf[2 + PS3_F4_REPORT_LEN];
    uint8_t i;
    init_buf[0] = HID_THDR_SET_REPORT_FEATURE; // THdr
    init_buf[1] = PS3_F4_REPORT_ID;            // Report ID

    for (i = 0; i < PS3_F4_REPORT_LEN; i++) {
        init_buf[2 + i] = (uint8_t)feature_F4_report[i];
    }

    return writeReport((uint8_t *)init_buf, 2 + PS3_F4_REPORT_LEN, pad);
}

#define DATA_START 11
#define MAX_DELAY 10

static uint8_t btn_delay = 0;

static void readReport(uint8_t *data, int bytes, int pad)
{
    if (hid_input_report) {
        ds3pad[pad].data[0] = ~data[DATA_START + ButtonStateL];
        ds3pad[pad].data[1] = ~data[DATA_START + ButtonStateH];

        ds3pad[pad].data[2] = data[DATA_START + RightStickX]; //rx
        ds3pad[pad].data[3] = data[DATA_START + RightStickY]; //ry
        ds3pad[pad].data[4] = data[DATA_START + LeftStickX];  //lx
        ds3pad[pad].data[5] = data[DATA_START + LeftStickY];  //ly

        if (bytes == 21 && !press_emu)
            press_emu = 1;

        if (press_emu) //needs emulating pressure buttons
        {
            ds3pad[pad].data[6] = ((data[DATA_START + ButtonStateL] >> 5) & 1) * 255; //right
            ds3pad[pad].data[7] = ((data[DATA_START + ButtonStateL] >> 7) & 1) * 255; //left
            ds3pad[pad].data[8] = ((data[DATA_START + ButtonStateL] >> 4) & 1) * 255; //up
            ds3pad[pad].data[9] = ((data[DATA_START + ButtonStateL] >> 6) & 1) * 255; //down

            ds3pad[pad].data[10] = ((data[DATA_START + ButtonStateH] >> 4) & 1) * 255; //triangle
            ds3pad[pad].data[11] = ((data[DATA_START + ButtonStateH] >> 5) & 1) * 255; //circle
            ds3pad[pad].data[12] = ((data[DATA_START + ButtonStateH] >> 6) & 1) * 255; //cross
            ds3pad[pad].data[13] = ((data[DATA_START + ButtonStateH] >> 7) & 1) * 255; //square

            ds3pad[pad].data[14] = ((data[DATA_START + ButtonStateH] >> 2) & 1) * 255; //L1
            ds3pad[pad].data[15] = ((data[DATA_START + ButtonStateH] >> 3) & 1) * 255; //R1
            ds3pad[pad].data[16] = ((data[DATA_START + ButtonStateH] >> 0) & 1) * 255; //L2
            ds3pad[pad].data[17] = ((data[DATA_START + ButtonStateH] >> 1) & 1) * 255; //R2

            data[DATA_START + Power] = 0x05;
        } else {
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
        }

        if (data[DATA_START + PSButtonState]) {                                      //display battery level
            if ((data[DATA_START + ButtonStateL] & 1) && (btn_delay == MAX_DELAY)) { //PS + SELECT
                if (ds3pad[pad].analog_btn < 2)                                      //unlocked mode
                    ds3pad[pad].analog_btn = !ds3pad[pad].analog_btn;

                ds3pad[pad].oldled = led_patterns[pad][(ds3pad[pad].analog_btn & 1)];
                btn_delay = 0;
            } else if (data[DATA_START + Power] != 0xEE)
                ds3pad[pad].oldled = power_level[data[DATA_START + Power]];

            if (btn_delay < MAX_DELAY)
                btn_delay++;
        } else
            ds3pad[pad].oldled = led_patterns[pad][(ds3pad[pad].analog_btn & 1)];

        if (data[DATA_START + Power] == 0xEE) //charging
            ds3pad[pad].oldled |= 0x80;
        else
            ds3pad[pad].oldled &= 0x7F;

    } else {
        DPRINTF("Unmanaged Input Report: THDR 0x%x ", data[8]);
        DPRINTF(" ID 0x%x\n", data[9]);
    }
} // readReport

static uint8_t writeReport(uint8_t *data, uint8_t length, int pad)
{
    PollSema(bt_dev.l2cap_cmd_sema);

    l2cap_cmd_buf[0] = (uint8_t)(ds3pad[pad].hci_handle_ & 0xff); // HCI handle with PB,BC flag
    l2cap_cmd_buf[1] = (uint8_t)(((ds3pad[pad].hci_handle_ >> 8) & 0x0f) | 0x20);
    l2cap_cmd_buf[2] = (uint8_t)((4 + length) & 0xff); // HCI ACL total data length
    l2cap_cmd_buf[3] = (uint8_t)((4 + length) >> 8);
    l2cap_cmd_buf[4] = (uint8_t)(length & 0xff); // L2CAP header: Length
    l2cap_cmd_buf[5] = (uint8_t)(length >> 8);
    l2cap_cmd_buf[6] = (uint8_t)(ds3pad[pad].command_dcid_ & 0xff); // L2CAP header: Channel ID
    l2cap_cmd_buf[7] = (uint8_t)(ds3pad[pad].command_dcid_ >> 8);

    mips_memcpy(&l2cap_cmd_buf[8], data, length);

    hid_flags_ &= ~HID_FLAG_COMMAND_SUCCESS;

    // output on endpoint 2
    return UsbBulkTransfer(bt_dev.outEndp, l2cap_cmd_buf, (8 + length), l2cap_cmd_cb, (void *)pad);
} // writeReport

static int LEDRumble(uint8_t led, uint8_t lrum, uint8_t rrum, int pad)
{
    uint8_t led_buf[PS3_01_REPORT_LEN + 2];

    led_buf[0] = ds3pad[pad].type; // THdr
    led_buf[1] = PS3_01_REPORT_ID; // Report ID

    mips_memcpy(&led_buf[2], output_01_report, sizeof(output_01_report)); // PS3_01_REPORT_LEN);

    if (ds3pad[pad].type == 0xA2) {
        if (rrum < 5)
            rrum = 0;
    }

    led_buf[2 + 1] = 0xFE; //rt
    led_buf[2 + 2] = rrum; //rp
    led_buf[2 + 3] = 0xFE; //lt
    led_buf[2 + 4] = lrum; //lp

    led_buf[2 + 9] = led & 0x7F; //LED Conf

    if (led & 0x80) //msb means charging, so blink
    {
        led_buf[2 + 13] = 0x32;
        led_buf[2 + 18] = 0x32;
        led_buf[2 + 23] = 0x32;
        led_buf[2 + 28] = 0x32;
    }

    ds3pad[pad].oldled = led;
    ds3pad[pad].oldlrumble = lrum;
    ds3pad[pad].oldrrumble = rrum;

    return writeReport((uint8_t *)led_buf, sizeof(output_01_report) + 2, pad);
}
/************************************************************/
/* DS3BT Commands                                            */
/************************************************************/

static int Rumble(uint8_t lrum, uint8_t rrum, int pad)
{
    int ret;

    ret = LEDRumble(ds3pad[pad].oldled, lrum, rrum, pad);
    if (ret == USB_RC_OK) {
        WaitSema(bt_dev.l2cap_cmd_sema);
        if (ds3pad[pad].type == HID_THDR_SET_REPORT_OUTPUT) {
            WaitSema(bt_dev.l2cap_sema);
            do {
                ret = UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, (void *)pad);
                if (ret == USB_RC_OK)
                    WaitSema(bt_dev.l2cap_sema);
                else
                    break;
            } while (!hid_command_success);

            SignalSema(bt_dev.l2cap_sema);
        }
    } else
        DPRINTF("DS3BT: LEDRumble usb transfer error %d\n", ret);

    return ret;
}

#ifdef USE_THREAD
static uint8_t update_rum = 0, update_lrum = 0, update_rrum = 0;
static int update_port, update_thread_id, update_sema;

void ds3bt_set_rumble(uint8_t lrum, uint8_t rrum, int port)
{
    WaitSema(update_sema);

    update_rum = 1;
    update_lrum = lrum;
    update_rrum = rrum;

    SignalSema(update_sema);
}

int ds3bt_get_data(char *dst, int size, int port)
{
    int ret;

    WaitSema(update_sema);

    mips_memcpy(dst, ds3pad[port].data, size);
    update_port = port;
    ret = ds3pad[port].analog_btn & 1;

    WakeupThread(update_thread_id);

    return ret;
}

void ds3bt_set_mode(int mode, int lock, int port)
{
    WaitSema(update_sema);

    if (lock == 3)
        ds3pad[port].analog_btn = 3;
    else
        ds3pad[port].analog_btn = mode;

    SignalSema(update_sema);
}

static void update_thread(void *param)
{
    int ret;

    while (1) {
        SleepThread();

        if (update_rum) {
            Rumble(update_lrum, update_rrum, update_port);
            update_rum = 0;
        }

        WaitSema(bt_dev.l2cap_sema);

        ret = UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, (void *)update_port);

        if (ret == USB_RC_OK)
            WaitSema(bt_dev.l2cap_sema);
        else
            DPRINTF("DS3BT: ds3bt_get_data usb transfer error %d\n", ret);

        SignalSema(bt_dev.l2cap_sema);
        SignalSema(update_sema);
    }
}
#else
void ds3bt_set_rumble(uint8_t lrum, uint8_t rrum, int port)
{
    Rumble(lrum, rrum, port);
}

int ds3bt_get_data(char *dst, int size, int port)
{
    int ret;

    WaitSema(bt_dev.l2cap_sema);

    ret = UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, (void *)port);

    if (ret == USB_RC_OK)
        WaitSema(bt_dev.l2cap_sema);
    else
        DPRINTF("DS3BT: ds3bt_get_data usb transfer error %d\n", ret);

    mips_memcpy(dst, ds3pad[port].data, size);
    ret = ds3pad[port].analog_btn & 1;

    SignalSema(bt_dev.l2cap_sema);

    return ret;
}

void ds3bt_set_mode(int mode, int lock, int port)
{
    if (lock == 3)
        ds3pad[port].analog_btn = 3;
    else
        ds3pad[port].analog_btn = mode;
}
#endif
void ds3bt_reset()
{
    int pad;

    if (!(bt_dev.status & DS3BT_STATE_USB_AUTHORIZED))
        return;

    for (pad = 0; pad < MAX_PADS; pad++) {
        if ((ds3pad[pad].status_ & DS3BT_STATE_CONNECTED) || (ds3pad[pad].status_ & DS3BT_STATE_RUNNING)) {
            ds3pad[pad].status_ &= ~DS3BT_STATE_RUNNING;
            ds3pad[pad].l2cap_state_ = L2CAP_DOWN_STATE;

            UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, (void *)pad);
            hci_disconnect(ds3pad[pad].hci_handle_);
        }
    }

    DS3BT_init();

#ifdef USE_THREAD
    TerminateThread(update_thread_id);
    DeleteThread(update_thread_id);
    DeleteSema(update_sema);
#endif
    DelayThread(1000000);

    bt_release();
}

int ds3bt_get_status(int port)
{
    int status = bt_dev.status;

    if (status & DS3BT_STATE_USB_AUTHORIZED)
        status |= ds3pad[port].status_;

    return status;
}

int ds3bt_init(uint8_t pads)
{
    enable_pad = pads;

    if (UsbRegisterDriver(&bt_driver) != USB_RC_OK) {
        DPRINTF("BT: Error registering USB devices\n");
        return 0;
    }

    UsbRegisterDriver(&chrg_driver);
#ifdef USE_THREAD
    iop_thread_t param;
    iop_sema_t sema;

    sema.initial = 1;
    sema.max = 1;
    sema.option = 0;
    sema.attr = 0;

    update_sema = CreateSema(&sema);

    if (update_sema < 0) {
        return 0;
    }

    param.attr = TH_C;
    param.thread = update_thread;
    param.priority = 0x4f;
    param.stacksize = 0xB00;
    param.option = 0;
    update_thread_id = CreateThread(&param);

    if (update_thread_id > 0) {
        StartThread(update_thread_id, 0);
        return 1;
    }

    return 0;
#else
    return 1;
#endif
}
