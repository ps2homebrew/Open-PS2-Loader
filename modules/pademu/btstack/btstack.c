
/* based on https://github.com/IonAgorria/Arduino-PSRemote */
/* and https://github.com/felis/USB_Host_Shield_2.0 */

#include "types.h"
#include "loadcore.h"
#include "xloadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "usbd.h"
#include "usbd_macro.h"
#include "thbase.h"
#include "thsemap.h"
#include "../pademu.h"
#include "../sys_utils.h"
#include "btstack.h"

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

static int bt_probe(int devId);
static int bt_connect(int devId);
static int bt_disconnect(int devId);
static void bt_config_set(int result, int count, void *arg);

static UsbDriver bt_driver = {NULL, NULL, "btstack", bt_probe, bt_connect, bt_disconnect};
static bt_adapter_t bt_adp = {-1, -1, -1, -1, -1, -1};

static void dev_clear(int pad);
static void dev_init();
static void disconnect_all();

static int bt_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;
    UsbConfigDescriptor *config = NULL;
    UsbInterfaceDescriptor *intf = NULL;

    DPRINTF("BTSTACK: probe: devId=%i\n", devId);

    if (bt_adp.devId != -1) {
        DPRINTF("BTSTACK: Error - only one device allowed !\n");
        return 0;
    }

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        DPRINTF("BTSTACK: Error - Couldn't get device descriptor\n");
        return 0;
    }

    if (device->bNumConfigurations < 1)
        return 0;

    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (config == NULL) {
        DPRINTF("BTSTACK: Error - Couldn't get configuration descriptor\n");
        return 0;
    }

    if ((config->bNumInterfaces < 1) || (config->wTotalLength < (sizeof(UsbConfigDescriptor) + sizeof(UsbInterfaceDescriptor)))) {
        DPRINTF("BTSTACK: Error - No interfaces available\n");
        return 0;
    }

    intf = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    DPRINTF("BTSTACK: bInterfaceClass %X bInterfaceSubClass %X bInterfaceProtocol %X\n", intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol);

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

    DPRINTF("BTSTACK: connect: devId=%i\n", devId);

    if (bt_adp.devId != -1) {
        DPRINTF("BTSTACK: Error - only one device allowed !\n");
        return 1;
    }

    bt_adp.interruptEndp = -1;
    bt_adp.inEndp = -1;
    bt_adp.outEndp = -1;

    bt_adp.controlEndp = UsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    config = (UsbConfigDescriptor *)UsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength);

    epCount = interface->bNumEndpoints - 1;

    DPRINTF("BTSTACK: Endpoint Count %d \n", epCount + 1);

    endpoint = (UsbEndpointDescriptor *)UsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);

    do {

        if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && bt_adp.outEndp < 0) {
                bt_adp.outEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("BTSTACK: register Output endpoint id =%i addr=%02X packetSize=%i\n", bt_adp.outEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            } else if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && bt_adp.inEndp < 0) {
                bt_adp.inEndp = UsbOpenEndpointAligned(devId, endpoint);
                DPRINTF("BTSTACK: register Input endpoint id =%i addr=%02X packetSize=%i\n", bt_adp.inEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        } else if (endpoint->bmAttributes == USB_ENDPOINT_XFER_INT) {
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && bt_adp.interruptEndp < 0) {
                bt_adp.interruptEndp = UsbOpenEndpoint(devId, endpoint);
                DPRINTF("BTSTACK: register Interrupt endpoint id =%i addr=%02X packetSize=%i\n", bt_adp.interruptEndp, endpoint->bEndpointAddress, (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB);
            }
        }

        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);

    } while (epCount--);

    if (bt_adp.interruptEndp < 0 || bt_adp.inEndp < 0 || bt_adp.outEndp < 0) {
        DPRINTF("BTSTACK: Error - connect failed: not enough endpoints! \n");
        return -1;
    }

    bt_adp.devId = devId;

    UsbSetDeviceConfiguration(bt_adp.controlEndp, config->bConfigurationValue, bt_config_set, NULL);

    return 0;
}

static int bt_disconnect(int devId)
{
    DPRINTF("BTSTACK: disconnect: devId=%i\n", devId);

    if (bt_adp.devId != -1) {

		disconnect_all();

        if (bt_adp.interruptEndp >= 0)
            UsbCloseEndpoint(bt_adp.interruptEndp);

        if (bt_adp.inEndp >= 0)
            UsbCloseEndpoint(bt_adp.inEndp);

        if (bt_adp.outEndp >= 0)
            UsbCloseEndpoint(bt_adp.outEndp);

        bt_adp.devId = -1;
        bt_adp.interruptEndp = -1;
        bt_adp.inEndp = -1;
        bt_adp.outEndp = -1;
        bt_adp.controlEndp = -1;

		dev_init();
        SignalSema(bt_adp.hid_sema);
    }

    return 0;
}

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
        //fake with AirohaTechnologyCorp's Chip
        {0x0C, 0xFC, 0x83}};

static u8 link_key[] = //for ds4 authorisation
    {
        0x56, 0xE8, 0x81, 0x38, 0x08, 0x06, 0x51, 0x41,
        0xC0, 0x7F, 0x12, 0xAA, 0xD9, 0x66, 0x3C, 0xCE};

#define REQ_HCI_OUT (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_DEVICE)
#define HCI_COMMAND_REQ 0

#define MAX_PADS 4
#define MAX_DELAY 10

static u8 hci_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};
static u8 l2cap_buf[MAX_BUFFER_SIZE + 32] __attribute((aligned(4))) = {0};
static u8 hci_cmd_buf[MAX_BUFFER_SIZE] __attribute((aligned(4))) = {0};
static u8 l2cap_cmd_buf[MAX_BUFFER_SIZE + 32] __attribute((aligned(4))) = {0};

static u8 identifier = 0;

static bt_dev_t dev[MAX_PADS];
static bt_paddrv_t *btpad[MAX_PADS];

static void hci_event_cb(int resultCode, int bytes, void *arg);
static void l2cap_event_cb(int resultCode, int bytes, void *arg);

static int l2cap_connection_request(u16 handle, u8 rxid, u16 scid, u16 psm);
static int hci_reset();

static void bt_config_set(int result, int count, void *arg)
{
    PollSema(bt_adp.hid_sema);

    UsbInterruptTransfer(bt_adp.interruptEndp, hci_buf, MAX_BUFFER_SIZE, hci_event_cb, NULL);
    UsbBulkTransfer(bt_adp.inEndp, l2cap_buf, MAX_BUFFER_SIZE, l2cap_event_cb, NULL);

    dev_init();
    hci_reset();

    SignalSema(bt_adp.hid_sema);
}

/************************************************************/
/* HCI Commands                                             */
/************************************************************/

static int HCI_Command(int nbytes, u8 *dataptr)
{
    return UsbControlTransfer(bt_adp.controlEndp, REQ_HCI_OUT, HCI_COMMAND_REQ, 0, 0, nbytes, dataptr, NULL, NULL);
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
    hci_cmd_buf[9] = 0x01; //switch role to (slave = 1 / master = 0)

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
    hci_cmd_buf[9] = 0x09; //reason max connection

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
                    if (hci_buf[5] != 0) {
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
                        if (strncmp(dev[i].bdaddr, hci_buf + 5, 6) == 0) {
                            // store the handle for the ACL connection
                            dev[i].hci_handle = hci_buf[3] | ((hci_buf[4] & 0x0F) << 8);
                            break;
                        }
                    }
                    if (i == MAX_PADS) {
                        break;
                    }
                    btdev_status_set(BTDEV_STATE_CONNECTED, i);
                    hci_remote_name(dev[i].bdaddr);
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
                for (i = 0; i < MAX_PADS; i++) { //detect pad
                    if (dev[i].hci_handle == (hci_buf[3] | ((hci_buf[4] & 0x0F) << 8))) {
                        break;
                    }
                }
                if (i == MAX_PADS) {
                    break;
                }
                pademu_disconnect(&dev[i].dev);
                dev_clear(i);
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
                        if (strncmp(dev[i].bdaddr, hci_buf + 3, 6) == 0) {
                            break;
                        }
                    }
                    if (i == MAX_PADS) {
                        break;
                    }
                    for (pad = 0; pad < MAX_PADS; pad++) {
                        if (btpad[pad] != NULL && btpad[pad]->pad_connect(&dev[i].data, hci_buf + 9, 32 - 9)) {
                            break;
                        }
                    }
                    if (pad == MAX_PADS) {
                        break;
                    }
                    dev[i].pad = btpad[pad];
                    hci_change_connection_type(dev[i].hci_handle);
                    if (dev[i].pad->control_dcid == 0x0070) {
                        identifier++;
                        l2cap_connection_request(dev[i].hci_handle, identifier, dev[i].pad->control_dcid, L2CAP_PSM_CTRL);
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
                for (i = 0; i < MAX_PADS; i++) { //find free slot
                    if (dev[i].pad == NULL && !btdev_status_check(BTDEV_STATE_RUNNING, i)) {
                        if (btdev_status_check(BTDEV_STATE_CONNECTED, i)) {
                            if (btdev_status_check(BTDEV_STATE_DISCONNECTING, i)) //if we're waiting for hci disconnect event
                                continue;
                            else
                                hci_disconnect(dev[i].hci_handle); //try to disconnect
                        }
                        break;
                    }
                }
                if (i == MAX_PADS) { //no free slot
                    DPRINTF("\t No free slot! \n");
                    hci_reject_connection(hci_buf + 2);
                    break;
                }
                pad = i;
                mips_memcpy(dev[pad].bdaddr, hci_buf + 2, 6);
                dev[pad].data.fix = 1;
                for (i = 0; i < sizeof(GenuineMacAddress) / 3; i++) { //check if ds3 is genuine
                    if (dev[pad].bdaddr[5] == GenuineMacAddress[i][0] &&
                        dev[pad].bdaddr[4] == GenuineMacAddress[i][1] &&
                        dev[pad].bdaddr[3] == GenuineMacAddress[i][2]) {
                        dev[pad].data.fix = 0;
                        break;
                    }
                }
                btdev_status_clear(BTDEV_STATE_CONNECTED, pad);
                btdev_status_clear(BTDEV_STATE_RUNNING, pad);
                btdev_status_clear(BTDEV_STATE_DISCONNECTING, pad);
                hci_accept_connection(dev[pad].bdaddr);
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

static void dev_init()
{
    int i;

    for (i = 0; i < MAX_PADS; i++) {
        dev_clear(i);
    }
    identifier = 0;
}

static void disconnect_all()
{
    int i;
    for (i = 0; i < MAX_PADS; i++) {
        pademu_disconnect(&dev[i].dev);
    }
}

static void hci_event_cb(int resultCode, int bytes, void *arg)
{
    PollSema(bt_adp.hid_sema);
    HCI_event_task(resultCode);
    UsbInterruptTransfer(bt_adp.interruptEndp, hci_buf, MAX_BUFFER_SIZE, hci_event_cb, NULL);
    SignalSema(bt_adp.hid_sema);
}

/************************************************************/
/* L2CAP Commands                                           */
/************************************************************/

static int L2CAP_Command(u16 handle, u16 scid, u8 *data, u8 length)
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
    return UsbBulkTransfer(bt_adp.outEndp, l2cap_cmd_buf, (8 + length), NULL, NULL);
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

    return L2CAP_Command(handle, 0x0001, cmd_buf, 8);
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

    return L2CAP_Command(handle, 0x0001, cmd_buf, 12);
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
    //cmd_buf[10] = 0x96; // Config Opt: data
    //cmd_buf[11] = 0x00;

    //this setting disable hid cmd reports from ds3
    cmd_buf[10] = 0xFF; // Config Opt: data
    cmd_buf[11] = 0xFF;

    return L2CAP_Command(handle, 0x0001, cmd_buf, 12);
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

    return L2CAP_Command(handle, 0x0001, cmd_buf, 14);
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

    return L2CAP_Command(handle, 0x0001, cmd_buf, 8);
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

    return L2CAP_Command(handle, 0x0001, cmd_buf, 8);
}

#define CMD_DELAY 2

static int L2CAP_event_task(int result, int bytes)
{
    int pad = -1;

    if (!result) {
        for (pad = 0; pad < MAX_PADS; pad++) {
            if (l2cap_handle_ok(dev[pad].hci_handle))
                break;
        }

        if (pad == MAX_PADS) {
            DPRINTF("L2CAP Wrong Handle = 0x%04X\n", ((l2cap_buf[0] | (l2cap_buf[1] << 8))));
            return pad;
        }

        if (l2cap_handle_ok(dev[pad].hci_handle)) {
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
                            dev[pad].control_scid = l2cap_buf[14] | (l2cap_buf[15] << 8);
                            l2cap_connection_response(dev[pad].hci_handle, l2cap_buf[9], dev[pad].pad->control_dcid, dev[pad].control_scid, PENDING);
                            DelayThread(CMD_DELAY);
                            l2cap_connection_response(dev[pad].hci_handle, l2cap_buf[9], dev[pad].pad->control_dcid, dev[pad].control_scid, SUCCESSFUL);
                            DelayThread(CMD_DELAY);
                            identifier++;
                            l2cap_config_request(dev[pad].hci_handle, identifier, dev[pad].control_scid);
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == L2CAP_PSM_INTR) {
                            dev[pad].interrupt_scid = l2cap_buf[14] | (l2cap_buf[15] << 8);
                            l2cap_connection_response(dev[pad].hci_handle, l2cap_buf[9], dev[pad].pad->interrupt_dcid, dev[pad].interrupt_scid, PENDING);
                            DelayThread(CMD_DELAY);
                            l2cap_connection_response(dev[pad].hci_handle, l2cap_buf[9], dev[pad].pad->interrupt_dcid, dev[pad].interrupt_scid, SUCCESSFUL);
                            DelayThread(CMD_DELAY);
                            identifier++;
                            l2cap_config_request(dev[pad].hci_handle, identifier, dev[pad].interrupt_scid);
                        }
                        break;

                    case L2CAP_CMD_CONNECTION_RESPONSE:
                        DPRINTF("Connection Response ID = 0x%02X \n", l2cap_buf[9]);
                        DPRINTF("\t PSM = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                        DPRINTF("\t SCID = 0x%04X \n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));
                        DPRINTF("\t RESULT = 0x%04X \n", (l2cap_buf[16] | (l2cap_buf[17] << 8)));

                        if (((l2cap_buf[16] | (l2cap_buf[17] << 8)) == 0x0000) && ((l2cap_buf[18] | (l2cap_buf[19] << 8)) == 0x0000)) {
                            if ((l2cap_buf[14] | (l2cap_buf[15] << 8)) == dev[pad].pad->control_dcid) {
                                dev[pad].control_scid = l2cap_buf[12] | (l2cap_buf[13] << 8);
                                identifier++;
                                l2cap_config_request(dev[pad].hci_handle, identifier, dev[pad].control_scid);
                            } else if ((l2cap_buf[14] | (l2cap_buf[15] << 8)) == dev[pad].pad->interrupt_dcid) {
                                dev[pad].interrupt_scid = l2cap_buf[12] | (l2cap_buf[13] << 8);
                                identifier++;
                                l2cap_config_request(dev[pad].hci_handle, identifier, dev[pad].interrupt_scid);
                            }
                        }
                        break;

                    case L2CAP_CMD_CONFIG_REQUEST:
                        DPRINTF("Configuration Request ID = 0x%02X \n", l2cap_buf[9]);
                        DPRINTF("\t LEN = 0x%04X \n", (l2cap_buf[10] | (l2cap_buf[11] << 8)));
                        DPRINTF("\t SCID = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                        DPRINTF("\t FLAG = 0x%04X \n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));

                        if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == dev[pad].pad->control_dcid) {
                            l2cap_config_response(dev[pad].hci_handle, l2cap_buf[9], dev[pad].control_scid);
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == dev[pad].pad->interrupt_dcid) {
                            l2cap_config_response(dev[pad].hci_handle, l2cap_buf[9], dev[pad].interrupt_scid);
                        }
                        break;

                    case L2CAP_CMD_CONFIG_RESPONSE:
                        DPRINTF("Configuration Response ID = 0x%02X \n", l2cap_buf[9]);
                        DPRINTF("\t LEN = 0x%04X \n", (l2cap_buf[10] | (l2cap_buf[11] << 8)));
                        DPRINTF("\t SCID = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));
                        DPRINTF("\t FLAG = 0x%04X \n", (l2cap_buf[14] | (l2cap_buf[15] << 8)));
                        DPRINTF("\t RESULT = 0x%04X \n", (l2cap_buf[16] | (l2cap_buf[17] << 8)));

                        if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == dev[pad].pad->control_dcid) {
                            if (dev[pad].pad->interrupt_dcid == 0x0071) {
                                identifier++;
                                l2cap_connection_request(dev[pad].hci_handle, identifier, dev[pad].pad->interrupt_dcid, L2CAP_PSM_INTR);
                            }
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == dev[pad].pad->interrupt_dcid) {
                            dev[pad].pad->pad_init(&dev[pad].data, dev[pad].dev.id);
                            pademu_connect(&dev[pad].dev);
                            btdev_status_set(BTDEV_STATE_RUNNING, pad);
                        }
                        break;

                    case L2CAP_CMD_DISCONNECT_REQUEST:
                        DPRINTF("Disconnect Request SCID = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));

                        if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == dev[pad].pad->control_dcid) {
                            btdev_status_set(BTDEV_STATE_DISCONNECTING, pad);
                            l2cap_disconnection_response(dev[pad].hci_handle, l2cap_buf[9], dev[pad].pad->control_dcid, dev[pad].control_scid);
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == dev[pad].pad->interrupt_dcid) {
                            btdev_status_set(BTDEV_STATE_DISCONNECTING, pad);
                            l2cap_disconnection_response(dev[pad].hci_handle, l2cap_buf[9], dev[pad].pad->interrupt_dcid, dev[pad].interrupt_scid);
                        }
                        break;

                    case L2CAP_CMD_DISCONNECT_RESPONSE:
                        DPRINTF("Disconnect Response SCID = 0x%04X \n", (l2cap_buf[12] | (l2cap_buf[13] << 8)));

                        if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == dev[pad].control_scid) {
                            btdev_status_set(BTDEV_STATE_DISCONNECTING, pad);
                            hci_disconnect(dev[pad].hci_handle);
                        } else if ((l2cap_buf[12] | (l2cap_buf[13] << 8)) == dev[pad].interrupt_scid) {
                            btdev_status_set(BTDEV_STATE_DISCONNECTING, pad);
                            identifier++;
                            l2cap_disconnection_request(dev[pad].hci_handle, identifier, dev[pad].control_scid, dev[pad].pad->control_dcid);
                        }
                        break;

                    default:
                        break;
                }
            } else if (l2cap_interrupt_channel) {
                dev[pad].pad->pad_read_report(&dev[pad].data, l2cap_buf, bytes);
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

    PollSema(bt_adp.hid_sema);

    ret = L2CAP_event_task(resultCode, bytes);

    if (ret < MAX_PADS) {
        if (btdev_status_check(BTDEV_STATE_RUNNING, ret)) {
            if (btdev_status_check(BTDEV_STATE_DISCONNECT_REQUEST, ret)) {
                if (!dev[ret].data.fix) {
                    identifier++;
                    l2cap_disconnection_request(dev[ret].hci_handle, identifier, dev[ret].interrupt_scid, dev[ret].pad->interrupt_dcid);
                } else {
                    hci_disconnect(dev[ret].hci_handle);
                }
                btdev_status_clear(BTDEV_STATE_DISCONNECT_REQUEST, ret);
            } else if (dev[ret].data.update_rum) {
                dev[ret].pad->pad_rumble(&dev[ret].data);
                dev[ret].data.update_rum = 0;
            }
        } else {
            if (!dev[ret].data.fix) {
                DelayThread(42000); //fix for some bt adapters
            }
        }
    }

    UsbBulkTransfer(bt_adp.inEndp, l2cap_buf, MAX_BUFFER_SIZE + 23, l2cap_event_cb, arg);
    SignalSema(bt_adp.hid_sema);
}

void btstack_register(bt_paddrv_t *pad)
{
    int i;
    for (i = 0; i < MAX_PADS; i++) {
        if (btpad[i] == NULL) {
            btpad[i] = pad;
            break;
        }
    }
}

void btstack_unregister(bt_paddrv_t *pad)
{
    int i;
    for (i = 0; i < MAX_PADS; i++) {
        if (btpad[i] == pad) {
            btpad[i] = NULL;
            break;
        }
    }
}

int btstack_hid_command(bt_paddrv_t *pad, u8 *data, u8 length, int id)
{
    int i;
    for (i = 0; i < MAX_PADS; i++) {
        if (dev[i].pad == pad && dev[i].data.id == id) {
            break;
        }
    }

    if (i == MAX_PADS) {
        return 0;
    }

    return L2CAP_Command(dev[i].hci_handle, dev[i].control_scid, data, length);
}

void btstack_exit(int mode)
{
    int pad;

    if (bt_adp.devId != -1) {
        for (pad = 0; pad < MAX_PADS; pad++) {
            WaitSema(bt_adp.hid_sema);
            btdev_status_set(BTDEV_STATE_DISCONNECT_REQUEST, pad);
            SignalSema(bt_adp.hid_sema);
            while (1) {
                DelayThread(500);
                WaitSema(bt_adp.hid_sema);
                if (!btdev_status_check(BTDEV_STATE_RUNNING, pad)) {
                    SignalSema(bt_adp.hid_sema);
                    break;
                }
                SignalSema(bt_adp.hid_sema);
            }
        }
        DelayThread(1000000);
        bt_disconnect(bt_adp.devId);
    }
}

int btpad_get_data(u8 *dst, int size, int port)
{
    int ret;

    WaitSema(bt_adp.hid_sema);

    //DPRINTF("BTSTACK: Get data\n");
    mips_memcpy(dst, dev[port].data.data, size);
    ret = dev[port].data.analog_btn & 1;

    SignalSema(bt_adp.hid_sema);

    return ret;
}

void btpad_set_rumble(u8 lrum, u8 rrum, int port)
{
    WaitSema(bt_adp.hid_sema);

    //DPRINTF("BTSTACK: Rumble\n");
    dev[port].data.update_rum = 1;
    dev[port].data.lrum = lrum;
    dev[port].data.rrum = rrum;

    SignalSema(bt_adp.hid_sema);
}

void btpad_set_mode(int mode, int lock, int port)
{
    WaitSema(bt_adp.hid_sema);

    //DPRINTF("BTSTACK: Set mode\n");
    if (lock == 3)
        dev[port].data.analog_btn = 3;
    else
        dev[port].data.analog_btn = mode;

    SignalSema(bt_adp.hid_sema);
}

static void dev_clear(int pad)
{
    dev[pad].pad = NULL;
    dev[pad].hci_handle = 0x0FFF;
    dev[pad].control_scid = 0;
    dev[pad].interrupt_scid = 0;
    dev[pad].data.lrum = 0;
    dev[pad].data.rrum = 0;
    dev[pad].data.update_rum = 1;
    dev[pad].data.data[0] = 0xFF;
    dev[pad].data.data[1] = 0xFF;
    dev[pad].data.analog_btn = 0;
    dev[pad].status = BTDEV_STATE_USB_CONFIGURED;
    dev[pad].dev.id = pad;
    dev[pad].dev.pad_get_data = btpad_get_data;
    dev[pad].dev.pad_set_rumble = btpad_set_rumble;
    dev[pad].dev.pad_set_mode = btpad_set_mode;

    mips_memset(&dev[pad].data.data[2], 0x7F, 4);
    mips_memset(&dev[pad].data.data[6], 0x00, 12);
    mips_memset(dev[pad].bdaddr, 0, 6);
}

extern struct irx_export_table _exp_btstack;

int _start(int argc, char *argv[])
{
    int ret;

    dev_init();

    if (RegisterLibraryEntries(&_exp_btstack) != 0) {
        return MODULE_NO_RESIDENT_END;
    }

    SetRebootTimeLibraryHandlingMode(&_exp_btstack, 2);

    bt_adp.hid_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

    if (bt_adp.hid_sema < 0) {
        DPRINTF("BTSTACK: Failed to allocate semaphore.\n");
        return MODULE_NO_RESIDENT_END;
    }

    ret = UsbRegisterDriver(&bt_driver);

    if (ret != USB_RC_OK) {
        DPRINTF("BTSTACK: Error registering USB devices: %02X\n", ret);
        return MODULE_NO_RESIDENT_END;
    }

    return MODULE_RESIDENT_END;
}
