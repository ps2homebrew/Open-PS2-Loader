
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
#include "ds34bt.h"
#include "sys_utils.h"

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

static int bt_probe(int devId);
static int bt_connect(int devId);
static int bt_disconnect(int devId);
static void bt_config_set(int result, int count, void *arg);

static UsbDriver bt_driver = {NULL, NULL, "ds34bt", bt_probe, bt_connect, bt_disconnect};
static bt_device bt_dev = {-1, -1, -1, -1, -1, -1, DS34BT_STATE_USB_DISCONNECTED};

static int chrg_probe(int devId);
static int chrg_connect(int devId);
static int chrg_disconnect(int devId);
static int chrg_dev = -1;

static UsbDriver chrg_driver = {NULL, NULL, "ds34chrg", chrg_probe, chrg_connect, chrg_disconnect};

static void ds34pad_clear(int pad);
static void ds34pad_init();

static int bt_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;
    UsbConfigDescriptor *config = NULL;
    UsbInterfaceDescriptor *intf = NULL;

    DPRINTF("DS34BT: probe: devId=%i\n", devId);

    if ((bt_dev.devId > 0) && (bt_dev.status & DS34BT_STATE_USB_AUTHORIZED)) {
        DPRINTF("DS34BT: Error - only one device allowed !\n");
        return 0;
    }

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("DS34BT: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->bNumConfigurations < 1)
        return 0;

    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (config == NULL) {
        DPRINTF("DS34BT: Error - Couldn't get configuration descriptor\n");
        return 0;
    }

    if ((config->bNumInterfaces < 1) || (config->wTotalLength < (sizeof(UsbConfigDescriptor) + sizeof(UsbInterfaceDescriptor)))) {
        DPRINTF("DS34BT: Error - No interfaces available\n");
        return 0;
    }

    intf = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    DPRINTF("DS34BT: bInterfaceClass %X bInterfaceSubClass %X bInterfaceProtocol %X\n", intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol);

    if ((intf->bInterfaceClass != USB_CLASS_WIRELESS_CONTROLLER) ||
        (intf->bInterfaceSubClass != USB_SUBCLASS_RF_CONTROLLER) ||
        (intf->bInterfaceProtocol != USB_PROTOCOL_BLUETOOTH_PROG) ||
        (intf->bNumEndpoints < 3)) {
        return 0;
    }

    return 1;
}

static int bt_connect(int devId)
{
    int epCount;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;
    UsbEndpointDescriptor *endpoint;

    DPRINTF("DS34BT: connect: devId=%i\n", devId);

    if (bt_dev.devId != -1) {
        DPRINTF("DS34BT: Error - only one device allowed !\n");
        return 1;
    }

    bt_dev.status = DS34BT_STATE_USB_DISCONNECTED;

    bt_dev.interruptEndp = -1;
    bt_dev.inEndp = -1;
    bt_dev.outEndp = -1;

    bt_dev.controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    epCount = interface->bNumEndpoints - 1;

    DPRINTF("DS34BT: Endpoint Count %d \n", epCount + 1);

    endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {

        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && bt_dev.outEndp < 0) {
                bt_dev.outEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("DS34BT: register Output endpoint id =%i addr=%02X packetSize=%i\n", bt_dev.outEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            } else if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && bt_dev.inEndp < 0) {
                bt_dev.inEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("DS34BT: register Input endpoint id =%i addr=%02X packetSize=%i\n", bt_dev.inEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        } else if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && bt_dev.interruptEndp < 0) {
                bt_dev.interruptEndp = UsbOpenEndpoint(devId, endpoint);
                DPRINTF("DS34BT: register Interrupt endpoint id =%i addr=%02X packetSize=%i\n", bt_dev.interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (bt_dev.interruptEndp < 0 || bt_dev.inEndp < 0 || bt_dev.outEndp < 0) {
        DPRINTF("DS34BT: Error - connect failed: not enough endpoints! \n");
        return -1;
    }

    bt_dev.devId = devId;
    bt_dev.status = DS34BT_STATE_USB_AUTHORIZED;

    UsbSetDeviceConfiguration(bt_dev.controlEndp, config->bConfigurationValue, bt_config_set, NULL);

    return 0;
}

static int bt_disconnect(int devId)
{
    DPRINTF("DS34BT: disconnect: devId=%i\n", devId);

    if (bt_dev.status & DS34BT_STATE_USB_AUTHORIZED) {

        if (bt_dev.interruptEndp >= 0)
            UsbCloseEndpoint(bt_dev.interruptEndp);

        if (bt_dev.inEndp >= 0)
            UsbCloseEndpoint(bt_dev.inEndp);

        if (bt_dev.outEndp >= 0)
            UsbCloseEndpoint(bt_dev.outEndp);

        bt_dev.devId = -1;
        bt_dev.interruptEndp = -1;
        bt_dev.inEndp = -1;
        bt_dev.outEndp = -1;
        bt_dev.controlEndp = -1;
        bt_dev.status = DS34BT_STATE_USB_DISCONNECTED;

        ds34pad_init();
        SignalSema(bt_dev.hid_sema);
    }

    return 0;
}

int chrg_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;

    DPRINTF("DS34CHRG: probe: devId=%i\n", devId);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("DS34CHRG: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->idVendor == DS34_VID && (device->idProduct == DS3_PID || device->idProduct == DS4_PID || device->idProduct == DS4_PID_SLIM))
        return 1;

    return 0;
}

int chrg_connect(int devId)
{
    int chrg_end;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;

    DPRINTF("DS34CHRG: connect: devId=%i\n", devId);

    if (chrg_dev != -1) {
        DPRINTF("DS34CHRG: Error - only one device allowed !\n");
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
    DPRINTF("DS34CHRG: disconnect: devId=%i\n", devId);

    chrg_dev = -1;

    return 0;
}

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
        {{0x00, 0x10, 0x00}, {0x00, 0x7F, 0x00}}, // light green/green/
        {{0x10, 0x10, 0x00}, {0x7F, 0x7F, 0x00}}, // light yellow/yellow
        {{0x00, 0x10, 0x10}, {0x00, 0x7F, 0x7F}}, // light cyan/cyan
};

static u8 link_key[] = // for ds4 authorisation
    {
        0x56, 0xE8, 0x81, 0x38, 0x08, 0x06, 0x51, 0x41,
        0xC0, 0x7F, 0x12, 0xAA, 0xD9, 0x66, 0x3C, 0xCE};

// Taken from nefarius' SCPToolkit
// https://github.com/nefarius/ScpToolkit/blob/master/ScpControl/ScpControl.ini
// Valid MAC addresses used by Sony
static u8 GenuineMacAddress[][3] =
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
        // fake with AirohaTechnologyCorp's Chip
        {0x0C, 0xFC, 0x83}};

#define REQ_HCI_OUT     (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_DEVICE)
#define HCI_COMMAND_REQ 0

#define MAX_PADS  4
#define MAX_DELAY 10

static u8 hci_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};
static u8 l2cap_buf[MAX_BUFFER_SIZE + 32] __attribute((aligned(4))) = {0};
static u8 hci_cmd_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};
static u8 l2cap_cmd_buf[MAX_BUFFER_SIZE + 32] __attribute((aligned(4))) = {0};

static u8 identifier = 0;
static u8 press_emu = 0;
static u8 enable_fake = 0;

static ds34bt_pad_t ds34pad[MAX_PADS];

static void hci_event_cb(int resultCode, int bytes, void *arg);
static void l2cap_event_cb(int resultCode, int bytes, void *arg);

static int hid_initDS34(int pad);
static int hid_LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad);
static void hid_readReport(u8 *data, int bytes, int pad);

static int l2cap_connection_request(u16 handle, u8 rxid, u16 scid, u16 psm);
static int hci_reset();

static void bt_config_set(int result, int count, void *arg)
{
    PollSema(bt_dev.hid_sema);

    UsbInterruptTransfer(bt_dev.interruptEndp, hci_buf, MAX_BUFFER_SIZE, hci_event_cb, NULL);
    UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, NULL);

    ds34pad_init();
    hci_reset();

    SignalSema(bt_dev.hid_sema);
}

/************************************************************/
/* HCI Commands                                             */
/************************************************************/

static int HCI_Command(int nbytes, u8 *dataptr)
{
    return UsbControlTransfer(bt_dev.controlEndp, REQ_HCI_OUT, HCI_COMMAND_REQ, 0, 0, nbytes, dataptr, NULL, NULL);
}

static int hci_reset()
{
    hci_cmd_buf[0] = HCI_OCF_RESET;
    hci_cmd_buf[1] = HCI_OGF_CTRL_BBAND;
    hci_cmd_buf[2] = 0x00; // Parameter Total Length = 0

    return HCI_Command(3, hci_cmd_buf);
}

static int hci_write_scan_enable(u8 conf)
{
    hci_cmd_buf[0] = HCI_OCF_WRITE_SCAN_ENABLE;
    hci_cmd_buf[1] = HCI_OGF_CTRL_BBAND;
    hci_cmd_buf[2] = 0x01;
    hci_cmd_buf[3] = conf;

    return HCI_Command(4, hci_cmd_buf);
}

static int hci_accept_connection(u8 *bdaddr)
{
    hci_cmd_buf[0] = HCI_OCF_ACCEPT_CONNECTION; // HCI OCF = 9
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;        // HCI OGF = 1
    hci_cmd_buf[2] = 0x07;                      // parameter length 7
    hci_cmd_buf[3] = *bdaddr;                   // 6 octet bluetooth address
    hci_cmd_buf[4] = *(bdaddr + 1);
    hci_cmd_buf[5] = *(bdaddr + 2);
    hci_cmd_buf[6] = *(bdaddr + 3);
    hci_cmd_buf[7] = *(bdaddr + 4);
    hci_cmd_buf[8] = *(bdaddr + 5);
    hci_cmd_buf[9] = 0x01; // switch role to (slave = 1 / master = 0)

    return HCI_Command(10, hci_cmd_buf);
}

static int hci_remote_name(u8 *bdaddr)
{
    hci_cmd_buf[0] = HCI_OCF_REMOTE_NAME; // HCI OCF = 19
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;  // HCI OGF = 1
    hci_cmd_buf[2] = 0x0A;                // parameter length = 10
    hci_cmd_buf[3] = *bdaddr;             // 6 octet bluetooth address
    hci_cmd_buf[4] = *(bdaddr + 1);
    hci_cmd_buf[5] = *(bdaddr + 2);
    hci_cmd_buf[6] = *(bdaddr + 3);
    hci_cmd_buf[7] = *(bdaddr + 4);
    hci_cmd_buf[8] = *(bdaddr + 5);
    hci_cmd_buf[9] = 0x01;  // Page Scan Repetition Mode
    hci_cmd_buf[10] = 0x00; // Reserved
    hci_cmd_buf[11] = 0x00; // Clock offset - low byte
    hci_cmd_buf[12] = 0x00; // Clock offset - high byte

    return HCI_Command(13, hci_cmd_buf);
}

static int hci_reject_connection(u8 *bdaddr)
{
    hci_cmd_buf[0] = HCI_OCF_REJECT_CONNECTION; // HCI OCF = A
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;        // HCI OGF = 1
    hci_cmd_buf[2] = 0x07;                      // parameter length 7
    hci_cmd_buf[3] = *bdaddr;                   // 6 octet bluetooth address
    hci_cmd_buf[4] = *(bdaddr + 1);
    hci_cmd_buf[5] = *(bdaddr + 2);
    hci_cmd_buf[6] = *(bdaddr + 3);
    hci_cmd_buf[7] = *(bdaddr + 4);
    hci_cmd_buf[8] = *(bdaddr + 5);
    hci_cmd_buf[9] = 0x09; // reason max connection

    return HCI_Command(10, hci_cmd_buf);
}

static int hci_disconnect(u16 handle)
{
    hci_cmd_buf[0] = HCI_OCF_DISCONNECT;         // HCI OCF = 6
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;         // HCI OGF = 1
    hci_cmd_buf[2] = 0x03;                       // parameter length = 3
    hci_cmd_buf[3] = (u8)(handle & 0xFF);        // connection handle - low byte
    hci_cmd_buf[4] = (u8)((handle >> 8) & 0x0F); // connection handle - high byte
    hci_cmd_buf[5] = 0x13;                       // reason

    return HCI_Command(6, hci_cmd_buf);
}

static int hci_change_connection_type(u16 handle)
{
    hci_cmd_buf[0] = HCI_OCF_CHANGE_CONNECTION_TYPE;
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;
    hci_cmd_buf[2] = 0x04;                       // parameter length 4
    hci_cmd_buf[3] = (u8)(handle & 0xFF);        // connection handle - low byte
    hci_cmd_buf[4] = (u8)((handle >> 8) & 0x0F); // connection handle - high byte
    hci_cmd_buf[5] = 0x18;                       // Packet Type: 0xcc18
    hci_cmd_buf[6] = 0xcc;

    return HCI_Command(7, hci_cmd_buf);
}

static int hci_link_key_request_reply(u8 *bdaddr)
{
    hci_cmd_buf[0] = HCI_OCF_LINK_KEY_REQUEST_REPLY; // HCI OCF = 0E
    hci_cmd_buf[1] = HCI_OGF_LINK_CNTRL;             // HCI OGF = 1
    hci_cmd_buf[2] = 0x06 + sizeof(link_key);        // parameter length 6
    hci_cmd_buf[3] = *bdaddr;                        // 6 octet bluetooth address
    hci_cmd_buf[4] = *(bdaddr + 1);
    hci_cmd_buf[5] = *(bdaddr + 2);
    hci_cmd_buf[6] = *(bdaddr + 3);
    hci_cmd_buf[7] = *(bdaddr + 4);
    hci_cmd_buf[8] = *(bdaddr + 5);

    mips_memcpy(&hci_cmd_buf[9], link_key, sizeof(link_key));

    return HCI_Command(9 + sizeof(link_key), hci_cmd_buf);
}

static void HCI_event_task(int result)
{
    int i, pad;

    if (!result) {
        /*  buf[0] = Event Code                            */
        /*  buf[1] = Parameter Total Length                */
        /*  buf[n] = Event Parameters based on each event  */
        DPRINTF("HCI event = 0x%02X \n", hci_buf[0]);
        switch (hci_buf[0]) { // switch on event type
            case HCI_EVENT_COMMAND_COMPLETE:
                DPRINTF("HCI Command Complete = 0x%02X \n", hci_buf[3]);
                DPRINTF("\tReturned = 0x%02X \n", hci_buf[5]);
                if ((hci_buf[3] == HCI_OCF_RESET) && (hci_buf[4] == HCI_OGF_CTRL_BBAND)) {
                    if (hci_buf[5] == 0) {
                        hci_write_scan_enable(SCAN_ENABLE_NOINQ_ENPAG);
                    } else {
                        DelayThread(500);
                        hci_reset();
                    }
                } else if ((hci_buf[3] == HCI_OCF_WRITE_SCAN_ENABLE) && (hci_buf[4] == HCI_OGF_CTRL_BBAND)) {
                    if (hci_buf[5] == 0) {
                        bt_dev.status |= DS34BT_STATE_USB_CONFIGURED;
                    } else {
                        DelayThread(500);
                        hci_reset();
                    }
                }
                break;

            case HCI_EVENT_COMMAND_STATUS:
                if (hci_buf[2]) { // show status on serial if not OK
                    DPRINTF("HCI Command Failed: \n");
                    DPRINTF("\t Status = 0x%02X \n", hci_buf[2]);
                    DPRINTF("\t Command_OpCode(OGF) = 0x%02X \n", ((hci_buf[5] & 0xFC) >> 2));
                    DPRINTF("\t Command_OpCode(OCF) = 0x%02X 0x%02X \n", (hci_buf[5] & 0x03), hci_buf[4]);
                }
                break;

            case HCI_EVENT_CONNECT_COMPLETE:
                DPRINTF("HCI event Connect Complete: \n");
                if (!hci_buf[2]) { // check if connected OK
                    DPRINTF("\t Connection_Handle 0x%02X \n", hci_buf[3] | ((hci_buf[4] & 0x0F) << 8));
                    DPRINTF("\t Requested by BD_ADDR: \n\t");
                    for (i = 0; i < 6; i++) {
                        DPRINTF("0x%02X", hci_buf[5 + i]);
                        if (i < 5)
                            DPRINTF(":");
                    }
                    DPRINTF("\n");
                    for (i = 0; i < MAX_PADS; i++) {
                        if (memcmp(ds34pad[i].bdaddr, hci_buf + 5, 6) == 0) {
                            // store the handle for the ACL connection
                            ds34pad[i].hci_handle = hci_buf[3] | ((hci_buf[4] & 0x0F) << 8);
                            break;
                        }
                    }
                    if (i >= MAX_PADS) {
                        break;
                    }
                    pad_status_set(DS34BT_STATE_CONNECTED, i);
                    hci_remote_name(ds34pad[i].bdaddr);
                } else {
                    DPRINTF("\t Error 0x%02X \n", hci_buf[2]);
                }
                break;

            case HCI_EVENT_NUM_COMPLETED_PKT:
                DPRINTF("HCI Number Of Completed Packets Event: \n");
                DPRINTF("\t Number_of_Handles = 0x%02X \n", hci_buf[2]);
                for (i = 0; i < hci_buf[2]; i++) {
                    DPRINTF("\t Connection_Handle = 0x%04X \n", (hci_buf[3 + i] | ((hci_buf[4 + i] & 0x0F) << 8)));
                }
                break;

            case HCI_EVENT_QOS_SETUP_COMPLETE:
                break;

            case HCI_EVENT_DISCONN_COMPLETE:
                DPRINTF("HCI Disconnection Complete Event: \n");
                DPRINTF("\t Status = 0x%02X \n", hci_buf[2]);
                DPRINTF("\t Connection_Handle = 0x%04X \n", (hci_buf[3] | ((hci_buf[4] & 0x0F) << 8)));
                DPRINTF("\t Reason = 0x%02X \n", hci_buf[5]);
                for (i = 0; i < MAX_PADS; i++) { // detect pad
                    if (ds34pad[i].hci_handle == (hci_buf[3] | ((hci_buf[4] & 0x0F) << 8))) {
                        break;
                    }
                }
                ds34pad_clear(i);
                break;

            case HCI_EVENT_AUTHENTICATION_COMPLETE:
                DPRINTF("HCI Authentication Complete Event: \n");
                DPRINTF("\t Status = 0x%02X \n", hci_buf[2]);
                DPRINTF("\t Connection_Handle = 0x%04X \n", (hci_buf[3] | ((hci_buf[4] & 0x0F) << 8)));
                if (!hci_buf[2]) {
                    DPRINTF("\t Success \n");
                } else {
                    DPRINTF("\t Failed \n");
                }
                break;

            case HCI_EVENT_REMOTE_NAME_COMPLETE:
                DPRINTF("HCI Remote Name Requested Complete Event: \n");
                DPRINTF("\t Status = 0x%02X \n", hci_buf[2]);
                if (!hci_buf[2]) {
                    for (i = 0; i < MAX_PADS; i++) {
                        if (memcmp(ds34pad[i].bdaddr, hci_buf + 3, 6) == 0) {
                            break;
                        }
                    }
                    if (i >= MAX_PADS) {
                        break;
                    }
                    if (memcmp(hci_buf + 9, "Wireless Controller", 19) == 0) {
                        ds34pad[i].type = DS4;
                        ds34pad[i].isfake = 0;
                        DPRINTF("\t Type: Dualshock 4 \n");
                    } else {
                        ds34pad[i].type = DS3;
                        DPRINTF("\t Type: Dualshock 3 \n");
                    }
                    hci_change_connection_type(ds34pad[i].hci_handle);
                    if (ds34pad[i].type == DS4) {
                        identifier++;
                        l2cap_connection_request(ds34pad[i].hci_handle, identifier, 0x0070, L2CAP_PSM_CTRL);
                    }
                }
                break;

            case HCI_EVENT_ENCRYPTION_CHANGE:
                DPRINTF("HCI Encryption Change Event: \n");
                DPRINTF("\t Status = 0x%02X \n", hci_buf[2]);
                DPRINTF("\t Connection_Handle = 0x%04X \n", (hci_buf[3] | ((hci_buf[4] & 0x0F) << 8)));
                DPRINTF("\t Encryption_Enabled = 0x%02X \n", hci_buf[5]);
                break;

            case HCI_EVENT_CONNECT_REQUEST:
                DPRINTF("HCI Connection Requested by BD_ADDR: \n\t");
                for (i = 0; i < 6; i++) {
                    DPRINTF("0x%02X", hci_buf[2 + i]);
                    if (i < 5)
                        DPRINTF(":");
                }
                DPRINTF("\n\t Link = 0x%02X \n", hci_buf[11]);
                DPRINTF("\t Class = 0x%02X 0x%02X 0x%02X \n", hci_buf[8], hci_buf[9], hci_buf[10]);
                for (i = 0; i < MAX_PADS; i++) { // find free slot
                    if (!pad_status_check(DS34BT_STATE_RUNNING, i) && ds34pad[i].enabled) {
                        if (pad_status_check(DS34BT_STATE_CONNECTED, i)) {
                            if (pad_status_check(DS34BT_STATE_DISCONNECTING, i)) // if we're waiting for hci disconnect event
                                continue;
                            else
                                hci_disconnect(ds34pad[i].hci_handle); // try to disconnect
                        }
                        break;
                    }
                }
                if (i >= MAX_PADS) { // no free slot
                    hci_reject_connection(hci_buf + 2);
                    break;
                }
                pad = i;
                mips_memcpy(ds34pad[pad].bdaddr, hci_buf + 2, 6);
                ds34pad[pad].isfake = 0;
                if (enable_fake) {
                    ds34pad[pad].isfake = 1;                              // fake ds3
                    for (i = 0; i < sizeof(GenuineMacAddress) / 3; i++) { // check if ds3 is genuine
                        if (ds34pad[pad].bdaddr[5] == GenuineMacAddress[i][0] &&
                            ds34pad[pad].bdaddr[4] == GenuineMacAddress[i][1] &&
                            ds34pad[pad].bdaddr[3] == GenuineMacAddress[i][2]) {
                            ds34pad[pad].isfake = 0;
                            break;
                        }
                    }
                }
                pad_status_clear(DS34BT_STATE_CONNECTED, pad);
                pad_status_clear(DS34BT_STATE_RUNNING, pad);
                pad_status_clear(DS34BT_STATE_DISCONNECTING, pad);
                hci_accept_connection(ds34pad[pad].bdaddr);
                break;

            case HCI_EVENT_ROLE_CHANGED:
                DPRINTF("HCI Role Change Event: \n");
                DPRINTF("\t Status = 0x%02X \n", hci_buf[2]);
                DPRINTF("\t BD_ADDR: ");
                for (i = 0; i < 6; i++) {
                    DPRINTF("0x%02X", hci_buf[3 + i]);
                    if (i < 5)
                        DPRINTF(":");
                }
                DPRINTF("\n\t Role 0x%02X \n", hci_buf[9]);
                break;

            case HCI_EVENT_MAX_SLOT_CHANGE:
                DPRINTF("HCI Max Slot Change Event: \n");
                DPRINTF("\t Connection_Handle = 0x%x \n", (hci_buf[2] | ((hci_buf[3] & 0x0F) << 8)));
                DPRINTF("\t LMP Max Slots = 0x%x \n", hci_buf[5]);
                break;

            case HCI_EVENT_PIN_CODE_REQUEST:
                DPRINTF("HCI Pin Code Request Event \n");
                break;

            case HCI_EVENT_LINK_KEY_REQUEST:
                DPRINTF("HCI Link Key Request Event by BD_ADDR: \n\t");
                for (i = 0; i < 6; i++) {
                    DPRINTF("0x%02X", hci_buf[2 + i]);
                    if (i < 5)
                        DPRINTF(":");
                }
                DPRINTF("\n");
                hci_link_key_request_reply(hci_buf + 2);
                break;

            case HCI_EVENT_CHANGED_CONNECTION_TYPE:
                DPRINTF("Packet Type Changed STATUS: 0x%x \n", hci_buf[2]);
                DPRINTF("\t Type = %x \n", (hci_buf[5] | (hci_buf[6] << 8)));
                break;

            case HCI_EVENT_PAGE_SR_CHANGED:
                break;

            default:
                DPRINTF("Unmanaged Event: 0x%02X \n", hci_buf[0]);
                break;
        } // switch (buf[0])
    }
}

static void ds34pad_clear(int pad)
{
    if (pad >= MAX_PADS)
        return;

    ds34pad[pad].hci_handle = 0x0FFF;
    ds34pad[pad].control_scid = 0;
    ds34pad[pad].interrupt_scid = 0;
    mips_memset(ds34pad[pad].bdaddr, 0, 6);
    ds34pad[pad].status = bt_dev.status;
    ds34pad[pad].status |= DS34BT_STATE_USB_CONFIGURED;
    ds34pad[pad].isfake = 0;
    ds34pad[pad].type = 0;
    ds34pad[pad].btn_delay = 0;
    ds34pad[pad].data[0] = 0xFF;
    ds34pad[pad].data[1] = 0xFF;
    mips_memset(&ds34pad[pad].data[2], 0x7F, 4);
    mips_memset(&ds34pad[pad].data[6], 0x00, 12);
}

static void ds34pad_init()
{
    int i;

    for (i = 0; i < MAX_PADS; i++) {
        ds34pad_clear(i);
    }

    press_emu = 0;
    identifier = 0;
}

static void hci_event_cb(int resultCode, int bytes, void *arg)
{
    PollSema(bt_dev.hid_sema);
    HCI_event_task(resultCode);
    UsbInterruptTransfer(bt_dev.interruptEndp, hci_buf, MAX_BUFFER_SIZE, hci_event_cb, NULL);
    SignalSema(bt_dev.hid_sema);
}

/************************************************************/
/* L2CAP Commands                                           */
/************************************************************/

static int L2CAP_Command(u16 handle, u8 *data, u8 length)
{
    l2cap_cmd_buf[0] = (u8)(handle & 0xff); // HCI handle with PB,BC flag
    l2cap_cmd_buf[1] = (u8)(((handle >> 8) & 0x0f) | 0x20);
    l2cap_cmd_buf[2] = (u8)((4 + length) & 0xff); // HCI ACL total data length
    l2cap_cmd_buf[3] = (u8)((4 + length) >> 8);
    l2cap_cmd_buf[4] = (u8)(length & 0xff); // L2CAP header: Length
    l2cap_cmd_buf[5] = (u8)(length >> 8);
    l2cap_cmd_buf[6] = 0x01; // L2CAP header: Channel ID
    l2cap_cmd_buf[7] = 0x00; // L2CAP Signalling channel over ACL-U logical link

    mips_memcpy(&l2cap_cmd_buf[8], data, length);

    // output on endpoint 2
    return UsbBulkTransfer(bt_dev.outEndp, l2cap_cmd_buf, (8 + length), NULL, NULL);
}

static int l2cap_connection_request(u16 handle, u8 rxid, u16 scid, u16 psm)
{
    u8 cmd_buf[8];

    cmd_buf[0] = L2CAP_CMD_CONNECTION_REQUEST; // Code
    cmd_buf[1] = rxid;                         // Identifier
    cmd_buf[2] = 0x04;                         // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (u8)(psm & 0xff); // PSM
    cmd_buf[5] = (u8)(psm >> 8);
    cmd_buf[6] = (u8)(scid & 0xff); // Source CID (PS Remote)
    cmd_buf[7] = (u8)(scid >> 8);

    return L2CAP_Command(handle, cmd_buf, 8);
}

static int l2cap_connection_response(u16 handle, u8 rxid, u16 dcid, u16 scid, u8 result)
{
    u8 cmd_buf[12];

    cmd_buf[0] = L2CAP_CMD_CONNECTION_RESPONSE; // Code
    cmd_buf[1] = rxid;                          // Identifier
    cmd_buf[2] = 0x08;                          // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (u8)(dcid & 0xff); // Destination CID (Our)
    cmd_buf[5] = (u8)(dcid >> 8);
    cmd_buf[6] = (u8)(scid & 0xff); // Source CID (PS Remote)
    cmd_buf[7] = (u8)(scid >> 8);
    cmd_buf[8] = result; // Result
    cmd_buf[9] = 0x00;
    cmd_buf[10] = 0x00; // Status
    cmd_buf[11] = 0x00;

    if (result != 0)
        cmd_buf[10] = 0x01; // Authentication pending

    return L2CAP_Command(handle, cmd_buf, 12);
}

static int l2cap_config_request(u16 handle, u8 rxid, u16 dcid)
{
    u8 cmd_buf[12];

    cmd_buf[0] = L2CAP_CMD_CONFIG_REQUEST; // Code
    cmd_buf[1] = rxid;                     // Identifier
    cmd_buf[2] = 0x08;                     // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (u8)(dcid & 0xff); // Destination CID
    cmd_buf[5] = (u8)(dcid >> 8);
    cmd_buf[6] = 0x00; // Flags
    cmd_buf[7] = 0x00;
    cmd_buf[8] = 0x01; // Config Opt: type = MTU (Maximum Transmission Unit)
    cmd_buf[9] = 0x02; // Config Opt: length
    // cmd_buf[10] = 0x96; // Config Opt: data
    // cmd_buf[11] = 0x00;

    // this setting disable hid cmd reports from ds3
    cmd_buf[10] = 0xFF; // Config Opt: data
    cmd_buf[11] = 0xFF;

    return L2CAP_Command(handle, cmd_buf, 12);
}

static int l2cap_config_response(u16 handle, u8 rxid, u16 scid)
{
    u8 cmd_buf[14];

    cmd_buf[0] = L2CAP_CMD_CONFIG_RESPONSE; // Code
    cmd_buf[1] = rxid;                      // Identifier
    cmd_buf[2] = 0x0A;                      // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (u8)(scid & 0xff); // Source CID
    cmd_buf[5] = (u8)(scid >> 8);
    cmd_buf[6] = 0x00; // Result
    cmd_buf[7] = 0x00;
    cmd_buf[8] = 0x00; // Config
    cmd_buf[9] = 0x00;
    cmd_buf[10] = 0x01; // Config
    cmd_buf[11] = 0x02;
    cmd_buf[12] = 0xA0;
    cmd_buf[13] = 0x02;

    return L2CAP_Command(handle, cmd_buf, 14);
}

static int l2cap_disconnection_request(u16 handle, u8 rxid, u16 dcid, u16 scid)
{
    u8 cmd_buf[8];

    cmd_buf[0] = L2CAP_CMD_DISCONNECT_REQUEST; // Code
    cmd_buf[1] = rxid;                         // Identifier
    cmd_buf[2] = 0x04;                         // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (u8)(dcid & 0xff); // Destination CID
    cmd_buf[5] = (u8)(dcid >> 8);
    cmd_buf[6] = (u8)(scid & 0xff); // Source CID
    cmd_buf[7] = (u8)(scid >> 8);

    return L2CAP_Command(handle, cmd_buf, 8);
}

static int l2cap_disconnection_response(u16 handle, u8 rxid, u16 scid, u16 dcid)
{
    u8 cmd_buf[8];

    cmd_buf[0] = L2CAP_CMD_DISCONNECT_RESPONSE; // Code
    cmd_buf[1] = rxid;                          // Identifier
    cmd_buf[2] = 0x04;                          // Length
    cmd_buf[3] = 0x00;
    cmd_buf[4] = (u8)(dcid & 0xff); // Destination CID
    cmd_buf[5] = (u8)(dcid >> 8);
    cmd_buf[6] = (u8)(scid & 0xff); // Source CID
    cmd_buf[7] = (u8)(scid >> 8);

    return L2CAP_Command(handle, cmd_buf, 8);
}

#define CMD_DELAY 2

static int L2CAP_event_task(int result, int bytes)
{
    int pad = -1;
    u16 control_dcid = 0x0040;   // Channel endpoint on command source
    u16 interrupt_dcid = 0x0041; // Channel endpoint on interrupt source

    if (!result) {
        for (pad = 0; pad < MAX_PADS; pad++) {
            if (l2cap_handle_ok(ds34pad[pad].hci_handle))
                break;
        }

        if (pad >= MAX_PADS) {
            DPRINTF("L2CAP Wrong Handle = 0x%04X\n", ((l2cap_buf[0] | (l2cap_buf[1] << 8))));
            return pad;
        }

        if (l2cap_handle_ok(ds34pad[pad].hci_handle)) {
            if (ds34pad[pad].type == DS4) {
                control_dcid = 0x0070;
                interrupt_dcid = 0x0071;
            }

            if (l2cap_control_channel) {
                DPRINTF("L2CAP Signaling Command = 0x%02X, pad %d \n", l2cap_buf[8], pad);

                switch (l2cap_buf[8]) {
                    case L2CAP_CMD_COMMAND_REJECT:
                        DPRINTF("Command Reject ID = 0x%02X \n", l2cap_buf[9]);
                        DPRINTF("\t Reason = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                        DPRINTF("\t DATA = 0x%04X \n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));
                        break;

                    case L2CAP_CMD_CONNECTION_REQUEST:
                        DPRINTF("Connection Request ID = 0x%02X \n", l2cap_buf[9]);
                        DPRINTF("\t PSM = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                        DPRINTF("\t SCID = 0x%04X \n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));

                        if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == L2CAP_PSM_CTRL) {
                            ds34pad[pad].control_scid = l2cap_buf[14] | (l2cap_buf[15] << 8);
                            l2cap_connection_response(ds34pad[pad].hci_handle, l2cap_buf[9], control_dcid, ds34pad[pad].control_scid, PENDING);
                            DelayThread(CMD_DELAY);
                            l2cap_connection_response(ds34pad[pad].hci_handle, l2cap_buf[9], control_dcid, ds34pad[pad].control_scid, SUCCESSFUL);
                            DelayThread(CMD_DELAY);
                            identifier++;
                            l2cap_config_request(ds34pad[pad].hci_handle, identifier, ds34pad[pad].control_scid);
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == L2CAP_PSM_INTR) {
                            ds34pad[pad].interrupt_scid = l2cap_buf[14] | (l2cap_buf[15] << 8);
                            l2cap_connection_response(ds34pad[pad].hci_handle, l2cap_buf[9], interrupt_dcid, ds34pad[pad].interrupt_scid, PENDING);
                            DelayThread(CMD_DELAY);
                            l2cap_connection_response(ds34pad[pad].hci_handle, l2cap_buf[9], interrupt_dcid, ds34pad[pad].interrupt_scid, SUCCESSFUL);
                            DelayThread(CMD_DELAY);
                            identifier++;
                            l2cap_config_request(ds34pad[pad].hci_handle, identifier, ds34pad[pad].interrupt_scid);
                        }
                        break;

                    case L2CAP_CMD_CONNECTION_RESPONSE:
                        DPRINTF("Connection Response ID = 0x%02X \n", l2cap_buf[9]);
                        DPRINTF("\t PSM = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                        DPRINTF("\t SCID = 0x%04X \n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));
                        DPRINTF("\t RESULT = 0x%04X \n", (l2cap_buf[16] | (l2cap_buf[17] << 8)));

                        if (((l2cap_buf[16] | (l2cap_buf[17] << 8)) == 0x0000) && ((l2cap_buf[18] | (l2cap_buf[19] << 8)) == 0x0000)) {
                            if ((l2cap_buf[14] | (l2cap_buf[15] << 8)) == control_dcid) {
                                ds34pad[pad].control_scid = l2cap_buf[12] | (l2cap_buf[13] << 8);
                                identifier++;
                                l2cap_config_request(ds34pad[pad].hci_handle, identifier, ds34pad[pad].control_scid);
                            } else if ((l2cap_buf[14] | (l2cap_buf[15] << 8)) == interrupt_dcid) {
                                ds34pad[pad].interrupt_scid = l2cap_buf[12] | (l2cap_buf[13] << 8);
                                identifier++;
                                l2cap_config_request(ds34pad[pad].hci_handle, identifier, ds34pad[pad].interrupt_scid);
                            }
                        }
                        break;

                    case L2CAP_CMD_CONFIG_REQUEST:
                        DPRINTF("Configuration Request ID = 0x%02X \n", l2cap_buf[9]);
                        DPRINTF("\t LEN = 0x%04X \n", (l2cap_buf[10] | (l2cap_buf[11] << 8)));
                        DPRINTF("\t SCID = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                        DPRINTF("\t FLAG = 0x%04X \n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));

                        if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == control_dcid) {
                            l2cap_config_response(ds34pad[pad].hci_handle, l2cap_buf[9], ds34pad[pad].control_scid);
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == interrupt_dcid) {
                            l2cap_config_response(ds34pad[pad].hci_handle, l2cap_buf[9], ds34pad[pad].interrupt_scid);
                        }
                        break;

                    case L2CAP_CMD_CONFIG_RESPONSE:
                        DPRINTF("Configuration Response ID = 0x%02X \n", l2cap_buf[9]);
                        DPRINTF("\t LEN = 0x%04X \n", (l2cap_buf[10] | (l2cap_buf[11] << 8)));
                        DPRINTF("\t SCID = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                        DPRINTF("\t FLAG = 0x%04X \n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));
                        DPRINTF("\t RESULT = 0x%04X \n", (l2cap_buf[16] | (l2cap_buf[17] << 8)));

                        if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == control_dcid) {
                            if (ds34pad[pad].type == DS4) {
                                identifier++;
                                l2cap_connection_request(ds34pad[pad].hci_handle, identifier, interrupt_dcid, L2CAP_PSM_INTR);
                            }
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == interrupt_dcid) {
                            hid_initDS34(pad);
                            if (ds34pad[pad].type == DS3) {
                                ds34pad[pad].oldled[0] = led_patterns[pad][1];
                                ds34pad[pad].oldled[3] = 0;
                            } else if (ds34pad[pad].type == DS4) {
                                ds34pad[pad].oldled[0] = rgbled_patterns[pad][1][0];
                                ds34pad[pad].oldled[1] = rgbled_patterns[pad][1][1];
                                ds34pad[pad].oldled[2] = rgbled_patterns[pad][1][2];
                                ds34pad[pad].oldled[3] = 0;
                            }
                            DelayThread(CMD_DELAY);
                            hid_LEDRumble(ds34pad[pad].oldled, 0, 0, pad);
                            pad_status_set(DS34BT_STATE_RUNNING, pad);
                        }
                        ds34pad[pad].btn_delay = 0xFF;
                        break;

                    case L2CAP_CMD_DISCONNECT_REQUEST:
                        DPRINTF("Disconnect Request SCID = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));

                        if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == control_dcid) {
                            pad_status_set(DS34BT_STATE_DISCONNECTING, pad);
                            l2cap_disconnection_response(ds34pad[pad].hci_handle, l2cap_buf[9], control_dcid, ds34pad[pad].control_scid);
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == interrupt_dcid) {
                            pad_status_set(DS34BT_STATE_DISCONNECTING, pad);
                            l2cap_disconnection_response(ds34pad[pad].hci_handle, l2cap_buf[9], interrupt_dcid, ds34pad[pad].interrupt_scid);
                        }
                        break;

                    case L2CAP_CMD_DISCONNECT_RESPONSE:
                        DPRINTF("Disconnect Response SCID = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));

                        if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == ds34pad[pad].control_scid) {
                            pad_status_set(DS34BT_STATE_DISCONNECTING, pad);
                            hci_disconnect(ds34pad[pad].hci_handle);
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == ds34pad[pad].interrupt_scid) {
                            pad_status_set(DS34BT_STATE_DISCONNECTING, pad);
                            identifier++;
                            l2cap_disconnection_request(ds34pad[pad].hci_handle, identifier, ds34pad[pad].control_scid, control_dcid);
                        }
                        break;

                    default:
                        break;
                }
            } else if (l2cap_interrupt_channel) {
                hid_readReport(l2cap_buf, bytes, pad);
            } else if (l2cap_command_channel) {
                DPRINTF("HID command status 0x%02X \n", l2cap_buf[8]);
                pad = MAX_PADS;
            }
        } // acl_handle_ok
    }     // !rcode

    return pad;
}

static void l2cap_event_cb(int resultCode, int bytes, void *arg)
{
    int ret;
    u16 interrupt_dcid = 0x0041;

    PollSema(bt_dev.hid_sema);

    ret = L2CAP_event_task(resultCode, bytes);

    if (ret < MAX_PADS) {
        if (pad_status_check(DS34BT_STATE_RUNNING, ret)) {
            if (pad_status_check(DS34BT_STATE_DISCONNECT_REQUEST, ret)) {
                if (!ds34pad[ret].isfake) {
                    if (ds34pad[ret].type == DS4) {
                        interrupt_dcid = 0x0071;
                    }
                    identifier++;
                    l2cap_disconnection_request(ds34pad[ret].hci_handle, identifier, ds34pad[ret].interrupt_scid, interrupt_dcid);
                } else {
                    hci_disconnect(ds34pad[ret].hci_handle);
                }
                pad_status_clear(DS34BT_STATE_DISCONNECT_REQUEST, ret);
            } else if (ds34pad[ret].update_rum) {
                hid_LEDRumble(ds34pad[ret].oldled, ds34pad[ret].lrum, ds34pad[ret].rrum, ret);
                ds34pad[ret].update_rum = 0;
            }
        } else {
            if (!ds34pad[ret].isfake && ds34pad[ret].type == DS3)
                DelayThread(42000); // fix for some bt adapters
        }
    }

    UsbBulkTransfer(bt_dev.inEndp, l2cap_buf, MAX_BUFFER_SIZE + 23, l2cap_event_cb, arg);
    SignalSema(bt_dev.hid_sema);
}

/************************************************************/
/* HID Commands                                             */
/************************************************************/

static int HID_command(u16 handle, u16 scid, u8 *data, u8 length, int pad)
{
    l2cap_cmd_buf[0] = (u8)(handle & 0xff); // HCI handle with PB,BC flag
    l2cap_cmd_buf[1] = (u8)(((handle >> 8) & 0x0f) | 0x20);
    l2cap_cmd_buf[2] = (u8)((4 + length) & 0xff); // HCI ACL total data length
    l2cap_cmd_buf[3] = (u8)((4 + length) >> 8);
    l2cap_cmd_buf[4] = (u8)(length & 0xff); // L2CAP header: Length
    l2cap_cmd_buf[5] = (u8)(length >> 8);
    l2cap_cmd_buf[6] = (u8)(scid & 0xff); // L2CAP header: Channel ID
    l2cap_cmd_buf[7] = (u8)(scid >> 8);

    mips_memcpy(&l2cap_cmd_buf[8], data, length);

    // output on endpoint 2
    return UsbBulkTransfer(bt_dev.outEndp, l2cap_cmd_buf, (8 + length), NULL, NULL);
}

static int hid_initDS34(int pad)
{
    u8 init_buf[PS3_F4_REPORT_LEN + 2];
    u8 size = 2;

    if (ds34pad[pad].type == DS3) {
        init_buf[0] = HID_THDR_SET_REPORT_FEATURE; // THdr
        init_buf[1] = PS3_F4_REPORT_ID;            // Report ID
        init_buf[2] = 0x42;
        init_buf[3] = 0x03;
        init_buf[4] = 0x00;
        init_buf[5] = 0x00;
        size += PS3_F4_REPORT_LEN;
    } else if (ds34pad[pad].type == DS4) {
        init_buf[0] = HID_THDR_GET_REPORT_FEATURE; // THdr
        init_buf[1] = PS4_02_REPORT_ID;            // Report ID
    }

    return HID_command(ds34pad[pad].hci_handle, ds34pad[pad].control_scid, init_buf, size, pad);
}

static int hid_LEDRumble(u8 *led, u8 lrum, u8 rrum, int pad)
{
    u8 led_buf[PS4_11_REPORT_LEN + 2];
    u8 size = 2;

    if (ds34pad[pad].type == DS3) {
        if (ds34pad[pad].isfake)
            led_buf[0] = 0xA2; // THdr
        else
            led_buf[0] = HID_THDR_SET_REPORT_OUTPUT; // THdr

        led_buf[1] = PS3_01_REPORT_ID; // Report ID

        mips_memcpy(&led_buf[2], output_01_report, sizeof(output_01_report)); // PS3_01_REPORT_LEN);

        if (ds34pad[pad].isfake) {
            if (rrum < 5)
                rrum = 0;
        }

        led_buf[3] = 0xFE; // rt
        led_buf[4] = rrum; // rp
        led_buf[5] = 0xFE; // lt
        led_buf[6] = lrum; // lp

        led_buf[11] = led[0] & 0x7F; // LED Conf

        if (led[3]) { // means charging, so blink
            led_buf[15] = 0x32;
            led_buf[20] = 0x32;
            led_buf[25] = 0x32;
            led_buf[30] = 0x32;
        }

        size += sizeof(output_01_report);
    } else if (ds34pad[pad].type == DS4) {
        mips_memset(led_buf, 0, PS3_01_REPORT_LEN + 2);

        led_buf[0] = HID_THDR_SET_REPORT_OUTPUT; // THdr
        led_buf[1] = PS4_11_REPORT_ID;           // Report ID
        led_buf[2] = 0x80;                       // update rate 1000Hz
        led_buf[4] = 0xFF;

        led_buf[7] = rrum * 255;
        led_buf[8] = lrum;

        led_buf[9] = led[0];  // r
        led_buf[10] = led[1]; // g
        led_buf[11] = led[2]; // b

        if (led[3]) {           // means charging, so blink
            led_buf[12] = 0x80; // Time to flash bright (255 = 2.5 seconds)
            led_buf[13] = 0x80; // Time to flash dark (255 = 2.5 seconds)
        }

        size += PS4_11_REPORT_LEN;
    }

    ds34pad[pad].oldled[0] = led[0];
    ds34pad[pad].oldled[1] = led[1];
    ds34pad[pad].oldled[2] = led[2];
    ds34pad[pad].oldled[3] = led[3];

    return HID_command(ds34pad[pad].hci_handle, ds34pad[pad].control_scid, led_buf, size, pad);
}

static void hid_readReport(u8 *data, int bytes, int pad)
{
    if (data[8] == HID_THDR_DATA_INPUT) {
        if (data[9] == PS3_01_REPORT_ID) {
            struct ds3report *report;

            report = (struct ds3report *)&data[11];

            if (report->RightStickX == 0 && report->RightStickY == 0) // ledrumble cmd causes null report sometime
                return;

            ds34pad[pad].data[0] = ~report->ButtonStateL;
            ds34pad[pad].data[1] = ~report->ButtonStateH;

            ds34pad[pad].data[2] = report->RightStickX; // rx
            ds34pad[pad].data[3] = report->RightStickY; // ry
            ds34pad[pad].data[4] = report->LeftStickX;  // lx
            ds34pad[pad].data[5] = report->LeftStickY;  // ly

            if (bytes == 21 && !press_emu)
                press_emu = 1;

            if (press_emu) {                                // needs emulating pressure buttons
                ds34pad[pad].data[6] = report->Right * 255; // right
                ds34pad[pad].data[7] = report->Left * 255;  // left
                ds34pad[pad].data[8] = report->Up * 255;    // up
                ds34pad[pad].data[9] = report->Down * 255;  // down

                ds34pad[pad].data[10] = report->Triangle * 255; // triangle
                ds34pad[pad].data[11] = report->Circle * 255;   // circle
                ds34pad[pad].data[12] = report->Cross * 255;    // cross
                ds34pad[pad].data[13] = report->Square * 255;   // square

                ds34pad[pad].data[14] = report->L1 * 255; // L1
                ds34pad[pad].data[15] = report->R1 * 255; // R1
                ds34pad[pad].data[16] = report->L2 * 255; // L2
                ds34pad[pad].data[17] = report->R2 * 255; // R2

                report->Power = 0x05;
            } else {
                ds34pad[pad].data[6] = report->PressureRight; // right
                ds34pad[pad].data[7] = report->PressureLeft;  // left
                ds34pad[pad].data[8] = report->PressureUp;    // up
                ds34pad[pad].data[9] = report->PressureDown;  // down

                ds34pad[pad].data[10] = report->PressureTriangle; // triangle
                ds34pad[pad].data[11] = report->PressureCircle;   // circle
                ds34pad[pad].data[12] = report->PressureCross;    // cross
                ds34pad[pad].data[13] = report->PressureSquare;   // square

                ds34pad[pad].data[14] = report->PressureL1; // L1
                ds34pad[pad].data[15] = report->PressureR1; // R1
                ds34pad[pad].data[16] = report->PressureL2; // L2
                ds34pad[pad].data[17] = report->PressureR2; // R2
            }

            if (report->PSButtonState) {                                       // display battery level
                if (report->Select && (ds34pad[pad].btn_delay == MAX_DELAY)) { // PS + SELECT
                    if (ds34pad[pad].analog_btn < 2)                           // unlocked mode
                        ds34pad[pad].analog_btn = !ds34pad[pad].analog_btn;

                    ds34pad[pad].oldled[0] = led_patterns[pad][(ds34pad[pad].analog_btn & 1)];
                    ds34pad[pad].btn_delay = 1;
                } else {
                    if (report->Power != 0xEE)
                        ds34pad[pad].oldled[0] = power_level[report->Power];

                    if (ds34pad[pad].btn_delay < MAX_DELAY)
                        ds34pad[pad].btn_delay++;
                }
            } else {
                ds34pad[pad].oldled[0] = led_patterns[pad][(ds34pad[pad].analog_btn & 1)];

                if (ds34pad[pad].btn_delay > 0)
                    ds34pad[pad].btn_delay--;
            }

            if (report->Power == 0xEE) // charging
                ds34pad[pad].oldled[3] = 1;
            else
                ds34pad[pad].oldled[3] = 0;

        } else if (data[9] == PS4_11_REPORT_ID) {
            struct ds4report *report;
            u8 up = 0, down = 0, left = 0, right = 0;

            report = (struct ds4report *)&data[11];

            switch (report->Dpad) {
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

            if (bytes > 63 && report->TPad) {
                if (!report->Finger1Active) {
                    if (report->Finger1X < 960)
                        report->Share = 1;
                    else
                        report->Option = 1;
                }

                if (!report->Finger2Active) {
                    if (report->Finger2X < 960)
                        report->Share = 1;
                    else
                        report->Option = 1;
                }
            }

            ds34pad[pad].data[0] = ~(report->Share | report->L3 << 1 | report->R3 << 2 | report->Option << 3 | up << 4 | right << 5 | down << 6 | left << 7);
            ds34pad[pad].data[1] = ~(report->L2 | report->R2 << 1 | report->L1 << 2 | report->R1 << 3 | report->Triangle << 4 | report->Circle << 5 | report->Cross << 6 | report->Square << 7);

            ds34pad[pad].data[2] = report->RightStickX; // rx
            ds34pad[pad].data[3] = report->RightStickY; // ry
            ds34pad[pad].data[4] = report->LeftStickX;  // lx
            ds34pad[pad].data[5] = report->LeftStickY;  // ly

            ds34pad[pad].data[6] = right * 255; // right
            ds34pad[pad].data[7] = left * 255;  // left
            ds34pad[pad].data[8] = up * 255;    // up
            ds34pad[pad].data[9] = down * 255;  // down

            ds34pad[pad].data[10] = report->Triangle * 255; // triangle
            ds34pad[pad].data[11] = report->Circle * 255;   // circle
            ds34pad[pad].data[12] = report->Cross * 255;    // cross
            ds34pad[pad].data[13] = report->Square * 255;   // square

            ds34pad[pad].data[14] = report->L1 * 255;   // L1
            ds34pad[pad].data[15] = report->R1 * 255;   // R1
            ds34pad[pad].data[16] = report->PressureL2; // L2
            ds34pad[pad].data[17] = report->PressureR2; // R2

            if (report->PSButton) {                                           // display battery level
                if (report->Share && (ds34pad[pad].btn_delay == MAX_DELAY)) { // PS + Share
                    if (ds34pad[pad].analog_btn < 2)                          // unlocked mode
                        ds34pad[pad].analog_btn = !ds34pad[pad].analog_btn;

                    ds34pad[pad].oldled[0] = rgbled_patterns[pad][(ds34pad[pad].analog_btn & 1)][0];
                    ds34pad[pad].oldled[1] = rgbled_patterns[pad][(ds34pad[pad].analog_btn & 1)][1];
                    ds34pad[pad].oldled[2] = rgbled_patterns[pad][(ds34pad[pad].analog_btn & 1)][2];
                    ds34pad[pad].btn_delay = 1;
                } else {
                    ds34pad[pad].oldled[0] = report->Battery;
                    ds34pad[pad].oldled[1] = 0;
                    ds34pad[pad].oldled[2] = 0;

                    if (ds34pad[pad].btn_delay < MAX_DELAY)
                        ds34pad[pad].btn_delay++;
                }
            } else {
                ds34pad[pad].oldled[0] = rgbled_patterns[pad][(ds34pad[pad].analog_btn & 1)][0];
                ds34pad[pad].oldled[1] = rgbled_patterns[pad][(ds34pad[pad].analog_btn & 1)][1];
                ds34pad[pad].oldled[2] = rgbled_patterns[pad][(ds34pad[pad].analog_btn & 1)][2];

                if (ds34pad[pad].btn_delay > 0)
                    ds34pad[pad].btn_delay--;
            }

            if (report->Power != 0xB && report->Usb_plugged) // charging
                ds34pad[pad].oldled[3] = 1;
            else
                ds34pad[pad].oldled[3] = 0;
        }
        if (ds34pad[pad].btn_delay > 0) {
            ds34pad[pad].update_rum = 1;
        }
    } else {
        DPRINTF("Unmanaged Input Report: THDR 0x%02X ", data[8]);
        DPRINTF(" ID 0x%02X \n", data[9]);
    }
}

/************************************************************/
/* DS34BT Commands                                          */
/************************************************************/

void ds34bt_set_rumble(u8 lrum, u8 rrum, int port)
{
    WaitSema(bt_dev.hid_sema);

    ds34pad[port].update_rum = 1;
    ds34pad[port].lrum = lrum;
    ds34pad[port].rrum = rrum;

    SignalSema(bt_dev.hid_sema);
}

int ds34bt_get_data(u8 *dst, int size, int port)
{
    int ret;

    WaitSema(bt_dev.hid_sema);

    mips_memcpy(dst, ds34pad[port].data, size);
    ret = ds34pad[port].analog_btn & 1;

    SignalSema(bt_dev.hid_sema);

    return ret;
}

void ds34bt_set_mode(int mode, int lock, int port)
{
    WaitSema(bt_dev.hid_sema);

    if (lock == 3)
        ds34pad[port].analog_btn = 3;
    else
        ds34pad[port].analog_btn = mode;

    SignalSema(bt_dev.hid_sema);
}

int ds34bt_get_status(int port)
{
    int ret;

    WaitSema(bt_dev.hid_sema);

    ret = ds34pad[port].status;

    SignalSema(bt_dev.hid_sema);

    return ret;
}

int ds34bt_init(u8 pads, u8 options)
{
    int ret, i;

    for (i = 0; i < MAX_PADS; i++)
        ds34pad[i].enabled = (pads >> i) & 1;

    ds34pad_init();

    enable_fake = options & 1;

    bt_dev.hid_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

    if (bt_dev.hid_sema < 0) {
        DPRINTF("DS34BT: Failed to allocate semaphore.\n");
        return 0;
    }

    ret = UsbRegisterDriver(&bt_driver);

    if (ret != USB_RC_OK) {
        DPRINTF("DS34BT: Error registering USB devices: %02X\n", ret);
        return 0;
    }

    UsbRegisterDriver(&chrg_driver);

    return 1;
}

void ds34bt_reset()
{
    int pad;

    if (bt_dev.status & DS34BT_STATE_USB_AUTHORIZED) {
        for (pad = 0; pad < MAX_PADS; pad++) {
            WaitSema(bt_dev.hid_sema);
            pad_status_set(DS34BT_STATE_DISCONNECT_REQUEST, pad);
            SignalSema(bt_dev.hid_sema);
            while (1) {
                DelayThread(500);
                WaitSema(bt_dev.hid_sema);
                if (!pad_status_check(DS34BT_STATE_RUNNING, pad)) {
                    SignalSema(bt_dev.hid_sema);
                    break;
                }
                SignalSema(bt_dev.hid_sema);
            }
        }
        DelayThread(1000000);
        bt_disconnect(bt_dev.devId);
    }
}
