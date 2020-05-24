#ifndef _BTSTACK_H_
#define _BTSTACK_H_

#define USB_CLASS_WIRELESS_CONTROLLER      0xE0
#define USB_SUBCLASS_RF_CONTROLLER         0x01
#define USB_PROTOCOL_BLUETOOTH_PROG        0x01

#define MAX_BUFFER_SIZE 64 // Size of general purpose data buffer

#define PENDING 1
#define SUCCESSFUL 0

typedef struct
{
    int devId;
    int hid_sema;
    int controlEndp;
    int interruptEndp;
    int inEndp;
    int outEndp;
} bt_adapter_t;

typedef struct
{
    int id;
    u8 fix;
    u8 lrum;
    u8 rrum;
    u8 update_rum;
    u8 analog_btn;
    u8 data[18];
    u8 led[4];
    u8 btn_delay;
} bt_paddata_t;

typedef struct
{
    u16 control_dcid;
    u16 interrupt_dcid;
    int (*pad_connect)(bt_paddata_t *data, u8 *str, int size);
    void (*pad_init)(bt_paddata_t *data, int id);
    void (*pad_read_report)(bt_paddata_t *data, u8 *in, int bytes);
    void (*pad_rumble)(bt_paddata_t *data);
} bt_paddrv_t;

typedef struct
{
    u16 hci_handle;     // hci connection handle
    u16 control_scid;   // Channel endpoint on command destination
    u16 interrupt_scid; // Channel endpoint on interrupt destination
    u8 bdaddr[6];
    bt_paddata_t data;
    bt_paddrv_t *pad;
    pad_device_t dev;
    u8 status;
} bt_dev_t;

enum eBTDEVStatus {
    BTDEV_STATE_USB_DISCONNECTED = 0x00,
    BTDEV_STATE_USB_AUTHORIZED = 0x01,
    BTDEV_STATE_USB_CONFIGURED = 0x02,
    BTDEV_STATE_CONNECTED = 0x04,
    BTDEV_STATE_RUNNING = 0x08,
    BTDEV_STATE_DISCONNECTING = 0x10,
    BTDEV_STATE_DISCONNECT_REQUEST = 0x20,
};

#define btdev_status_clear(flag, pad) dev[pad].status &= ~flag
#define btdev_status_set(flag, pad) dev[pad].status |= flag
#define btdev_status_check(flag, pad) (dev[pad].status & flag)

enum eHCI {
    // {{{
    /* Bluetooth HCI states for HCI_task() */
    HCI_INIT_STATE = 0,
    HCI_RESET_STATE,
    HCI_READBDADDR_STATE,
    HCI_CONNECT_IN_STATE,
    HCI_REMOTE_NAME_STATE,
    HCI_CHANGE_CONNECTION,
    HCI_READ_REMOTE_SUPPORTED_FEATURES,
    HCI_CONNECTED_STATE,

    /* HCI OpCode Group Field (OGF) */
    HCI_OGF_LINK_CNTRL = (0x01 << 2),  // OGF: Link Control Commands
    HCI_OGF_LINK_POLICY = (0x02 << 2), // OGF: Link Policy Commands
    HCI_OGF_CTRL_BBAND = (0x03 << 2),  // OGF: Controller & Baseband Commands
    HCI_OGF_INFO_PARAM = (0x04 << 2),  // OGF: Informational Parameters

    /* HCI OpCode Command Field (OCF) */
    HCI_OCF_DISCONNECT = 0x06,                // OGF = 0x01
    HCI_OCF_ACCEPT_CONNECTION = 0x09,      // OGF = 0x01
    HCI_OCF_REJECT_CONNECTION = 0x0A,      // OGF = 0x01
    HCI_OCF_CHANGE_CONNECTION_TYPE = 0x0F, // OGF = 0x01
    HCI_OCF_REMOTE_NAME = 0x19,               // OGF = 0x01
    HCI_OCF_LINK_KEY_REQUEST_REPLY = 0x0B, // OGF = 0x01
    
    HCI_OCF_RESET = 0x03,                  // OGF = 0x03
    HCI_OCF_WRITE_ACCEPT_TIMEOUT = 0x16,   // OGF = 0x03
    HCI_OCF_WRITE_SCAN_ENABLE = 0x1A,      // OGF = 0x03
    
    HCI_OCF_READ_BDADDR = 0x09,      // OGF = 0x04

    /* HCI events managed */
    HCI_EVENT_CONNECT_COMPLETE = 0x03,
    HCI_EVENT_CONNECT_REQUEST = 0x04,
    HCI_EVENT_DISCONN_COMPLETE = 0x05,
    HCI_EVENT_AUTHENTICATION_COMPLETE= 0x06,
    HCI_EVENT_REMOTE_NAME_COMPLETE = 0x07,
    HCI_EVENT_ENCRYPTION_CHANGE = 0x08,
    HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE = 0x0B,
    HCI_EVENT_QOS_SETUP_COMPLETE = 0x0d, // do nothing
    HCI_EVENT_COMMAND_COMPLETE = 0x0e,
    HCI_EVENT_COMMAND_STATUS = 0x0f,
    HCI_EVENT_ROLE_CHANGED = 0x12,
    HCI_EVENT_NUM_COMPLETED_PKT = 0x13, // do nothing
    HCI_EVENT_PIN_CODE_REQUEST = 0x16,
    HCI_EVENT_LINK_KEY_REQUEST = 0x17,
    HCI_EVENT_CHANGED_CONNECTION_TYPE = 0x1D,
    HCI_EVENT_PAGE_SR_CHANGED = 0x20,
    HCI_EVENT_MAX_SLOT_CHANGE = 0x1B, //Max Slots Change event

    /* HCI event flags for hci_event_flag */
    HCI_FLAG_COMMAND_COMPLETE = 0x01,
    HCI_FLAG_COMMAND_STATUS = 0x02,
    HCI_FLAG_CONNECT_COMPLETE = 0x04,
    HCI_FLAG_DISCONN_COMPLETE = 0x08,
    HCI_FLAG_INCOMING_REQUEST = 0x10,
    HCI_FLAG_READ_BDADDR = 0x20,
    HCI_FLAG_REMOTE_NAME_COMPLETE = 0x40,

    /* HCI Scan Enable Parameters */
    SCAN_ENABLE_NOINQ_NOPAG = 0x00,
    SCAN_ENABLE_ENINQ_NOPAG = 0x01,
    SCAN_ENABLE_NOINQ_ENPAG = 0x02,
    SCAN_ENABLE_ENINQ_ENPAG = 0x03,
    // }}}
};

enum eL2CAP {
    // {{{
    /* Bluetooth L2CAP PSM */
    L2CAP_PSM_SDP = 0x01,
    L2CAP_PSM_CTRL = 0x11, // HID_Control
    L2CAP_PSM_INTR = 0x13,  // HID_Interrupt

    /* Bluetooth L2CAP states for L2CAP_task() */
    L2CAP_DOWN_STATE = 0,
    L2CAP_INIT_STATE,
    L2CAP_CONTROL_CONNECTING_STATE,
    L2CAP_CONTROL_REQUEST_STATE,
    L2CAP_CONTROL_CONFIGURING_STATE,
    L2CAP_INTERRUPT_CONNECTING_STATE,
    L2CAP_INTERRUPT_REQUEST_STATE,
    L2CAP_INTERRUPT_CONFIGURING_STATE,
    L2CAP_CONNECTED_STATE,
    L2CAP_LED_STATE,
    L2CAP_LED_STATE_END,
    L2CAP_READY_STATE,
    L2CAP_DISCONNECT_STATE,

    /* L2CAP event flags */
    L2CAP_EV_COMMAND_CONNECTED = 0x01,
    L2CAP_EV_COMMAND_CONFIG_REQ = 0x02,
    L2CAP_EV_COMMAND_CONFIGURED = 0x04,
    L2CAP_EV_COMMAND_DISCONNECT_REQ = 0x08,
    L2CAP_EV_INTERRUPT_CONNECTED = 0x10,
    L2CAP_EV_INTERRUPT_CONFIG_REQ = 0x20,
    L2CAP_EV_INTERRUPT_CONFIGURED = 0x40,
    L2CAP_EV_INTERRUPT_DISCONNECT_REQ = 0x80,

    /* L2CAP signaling command */
    L2CAP_CMD_COMMAND_REJECT = 0x01,
    L2CAP_CMD_CONNECTION_REQUEST = 0x02,
    L2CAP_CMD_CONNECTION_RESPONSE = 0x03,
    L2CAP_CMD_CONFIG_REQUEST = 0x04,
    L2CAP_CMD_CONFIG_RESPONSE = 0x05,
    L2CAP_CMD_DISCONNECT_REQUEST = 0x06,
    L2CAP_CMD_DISCONNECT_RESPONSE = 0x07,

/*  HCI ACL Data Packet
 *
 *  buf[0]          buf[1]          buf[2]          buf[3]
 *  0      4        8    11 12      16              24             31 MSB
 *  .-+-+-+-+-+-+-+-|-+-+-+-|-+-|-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *  |        HCI Handle     |PB |BC |      Data Total Length        |    HCI ACL Data Packet
 *  .-+-+-+-+-+-+-+-|-+-+-+-|-+-|-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *
 *   buf[4]         buf[5]          buf[6]          buf[7]
 *  0               8               16                             31 MSB
 *  .-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *  |         Length                |        Channel ID             |    Basic L2CAP header
 *  .-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *
 *  buf[8]          buf[9]          buf[10]         buf[11]
 *  0               8               16                             31 MSB
 *  .-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *  |     Code      |  Identifier   |         Length                |    Control frame (C-frame)
 *  .-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.    (signaling packet format)
 */
    // }}}
};

#define l2cap_handle_ok(handle) (((u16)(l2cap_buf[0] | (l2cap_buf[1] << 8)) & 0x0FFF) == (u16)(handle & 0x0FFF))
#define l2cap_control_channel ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == 0x0001) // Channel ID for ACL-U
#define l2cap_interrupt_channel ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == dev[pad].pad->interrupt_dcid)
#define l2cap_command_channel ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == dev[pad].pad->control_dcid)

#define btstack_IMPORTS_start DECLARE_IMPORT_TABLE(btstack, 1, 1)

void btstack_register(bt_paddrv_t *pad);
#define I_btstack_register DECLARE_IMPORT(4, btstack_register)

void btstack_unregister(bt_paddrv_t *pad);
#define I_btstack_unregister DECLARE_IMPORT(5, btstack_unregister)

int btstack_hid_command(bt_paddrv_t *pad, u8 *data, u8 length, int id);
#define I_btstack_hid_command DECLARE_IMPORT(6, btstack_hid_command)

#define btstack_IMPORTS_end END_IMPORT_TABLE

#endif
