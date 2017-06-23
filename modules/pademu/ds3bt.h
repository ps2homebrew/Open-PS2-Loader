#ifndef _DS3BT_H_
#define _DS3BT_H_

#include "types.h"
#include "usbd.h"

typedef u8 uint8_t;
typedef u16 uint16_t;

enum eHCI {
    // {{{
    /* Bluetooth HCI states for HCI_task() */
    HCI_INIT_STATE = 0,
    HCI_RESET_STATE,
    HCI_CONNECT_IN_STATE,
    HCI_CHANGE_CONNECTION,
    HCI_READ_REMOTE_SUPPORTED_FEATURES,
    HCI_CONNECTED_STATE,

    /* HCI OpCode Group Field (OGF) */
    HCI_OGF_LINK_CNTRL = (0x01 << 2),  // OGF: Link Control Commands
    HCI_OGF_LINK_POLICY = (0x02 << 2), // OGF: Link Policy Commands
    HCI_OGF_CTRL_BBAND = (0x03 << 2),  // OGF: Controller & Baseband Commands
    HCI_OGF_INFO_PARAM = (0x04 << 2),  // OGF: Informational Parameters

    /* HCI OpCode Command Field (OCF) */
    HCI_OCF_ACCEPT_CONNECTION = 0x09,      // OGF = 0x01
    HCI_OCF_REJECT_CONNECTION = 0x0A,      // OGF = 0x01
    HCI_OCF_CHANGE_CONNECTION_TYPE = 0x0F, // OGF = 0x01
    HCI_OCF_RESET = 0x03,                  // OGF = 0x03
    HCI_OCF_WRITE_ACCEPT_TIMEOUT = 0x16,   // OGF = 0x03
    HCI_OCF_WRITE_SCAN_ENABLE = 0x1A,      // OGF = 0x03

    /* HCI events managed */
    HCI_EVENT_CONNECT_COMPLETE = 0x03,
    HCI_EVENT_CONNECT_REQUEST = 0x04,
    HCI_EVENT_DISCONN_COMPLETE = 0x05,
    HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE = 0x0B,
    HCI_EVENT_QOS_SETUP_COMPLETE = 0x0d, // do nothing
    HCI_EVENT_COMMAND_COMPLETE = 0x0e,
    HCI_EVENT_COMMAND_STATUS = 0x0f,
    HCI_EVENT_ROLE_CHANGED = 0x12,
    HCI_EVENT_NUM_COMPLETED_PKT = 0x13, // do nothing
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

    /* HCI Scan Enable Parameters */
    SCAN_ENABLE_NOINQ_NOPAG = 0x00,
    SCAN_ENABLE_ENINQ_NOPAG = 0x01,
    SCAN_ENABLE_NOINQ_ENPAG = 0x02,
    SCAN_ENABLE_ENINQ_ENPAG = 0x03,

    // used in control endpoint header for HCI Commands
    bmREQ_HCI_OUT = (USB_DIR_OUT |
                     USB_TYPE_CLASS |
                     USB_RECIP_DEVICE),
    HCI_COMMAND_REQ = 0,
    // }}}
};

enum eL2CAP {
    // {{{
    /* Bluetooth L2CAP PSM */
    L2CAP_PSM_WRITE = 0x11, // HID_Control
    L2CAP_PSM_READ = 0x13,  // HID_Interrupt

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
 *	buf[0]			buf[1]			buf[2]			buf[3]
 *	0		4		8	11 12		16				 24				31 MSB
 *  .-+-+-+-+-+-+-+-|-+-+-+-|-+-|-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *  |		HCI Handle		|PB |BC |		Data Total Length		|	HCI ACL Data Packet
 *  .-+-+-+-+-+-+-+-|-+-+-+-|-+-|-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *
 *	buf[4]			buf[5]			buf[6]			buf[7]
 *	0					8					16						31 MSB
 *  .-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *  |				Length				|			Channel ID			 |	Basic L2CAP header
 *  .-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *
 *	buf[8]			buf[9]			buf[10]			buf[11]
 *	0					8					16						31 MSB
 *  .-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.
 *  |	 Code		|  Identifier	|				Length				|	Control frame (C-frame)
 *  .-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-.	(signaling packet format)
 */
    // }}}
};

enum eHID {
    // {{{
    /* HID event flag */
    HID_FLAG_STATUS_REPORTED = 0x01,
    HID_FLAG_BUTTONS_CHANGED = 0x02,
    HID_FLAG_EXTENSION = 0x04,
    HID_FLAG_COMMAND_SUCCESS = 0x08,

    /* Bluetooth HID Transaction Header (THdr) */
    HID_THDR_GET_REPORT_FEATURE = 0x43,
    HID_THDR_SET_REPORT_OUTPUT = 0x52,
    HID_THDR_SET_REPORT_FEATURE = 0x53,
    HID_THDR_DATA_INPUT = 0xa1,

    /* Defines of various parameters for PS3 Game controller reports */
    PS3_F4_REPORT_ID = 0xF4,
    PS3_F4_REPORT_LEN = 0x04,

    PS3_01_REPORT_ID = 0x01,
    PS3_01_REPORT_LEN = 0x30,
    // }}}
};

enum eDS3BTLEDRumble {
    psLEDN = 0x00,
    psLED1 = 0x02,
    psLED2 = 0x04,
    psLED3 = 0x08,
    psLED4 = 0x0F,
    psLEDA = 0x10,
};

enum eDS3BTStatus {
    DS3BT_STATE_USB_DISCONNECTED = 0x00,
    DS3BT_STATE_USB_AUTHORIZED = 0x01,
    DS3BT_STATE_USB_CONFIGURED = 0x02,
    DS3BT_STATE_CONNECTED = 0x04,
    DS3BT_STATE_RUNNING = 0x08,
};

//Structure which describes the type 01 input report
enum TYPE_01_REPORT {
    ButtonStateL = 0, // Main buttons Low
    ButtonStateH,     // Main buttons High
    PSButtonState,    // PS button
    Reserved1,        // Unknown
    LeftStickX,       // left Joystick X axis 0 - 255, 128 is mid
    LeftStickY,       // left Joystick Y axis 0 - 255, 128 is mid
    RightStickX,      // right Joystick X axis 0 - 255, 128 is mid
    RightStickY,      // right Joystick Y axis 0 - 255, 128 is mid
    Reserved2,        // Unknown
    Reserved3,        // Unknown
    Reserved4,        // Unknown
    Reserved5,        // Unknown
    PressureUp,       // digital Pad Up button Pressure 0 - 255
    PressureRight,    // digital Pad Right button Pressure 0 - 255
    PressureDown,     // digital Pad Down button Pressure 0 - 255
    PressureLeft,     // digital Pad Left button Pressure 0 - 255
    PressureL2,       // digital Pad L2 button Pressure 0 - 255
    PressureR2,       // digital Pad R2 button Pressure 0 - 255
    PressureL1,       // digital Pad L1 button Pressure 0 - 255
    PressureR1,       // digital Pad R1 button Pressure 0 - 255
    PressureTriangle, // digital Pad Triangle button Pressure 0 - 255
    PressureCircle,   // digital Pad Circle button Pressure 0 - 255
    PressureCross,    // digital Pad Cross button Pressure 0 - 255
    PressureSquare,   // digital Pad Square button Pressure 0 - 255
    Reserved6,        // Unknown
    Reserved7,        // Unknown
    Reserved8,        // Unknown
    Charge,           // charging status ? 02 = charge, 03 = normal
    Power,            // Battery status ? 05=full - 02=dying, 01=just before shutdown, EE=charging
    Connection,       // Connection Type ? 14 when operating by bluetooth, 10 when operating by bluetooth with cable plugged in, 16 when bluetooh and rumble
    Reserved9,        // Unknown
    Reserved10,       // Unknown
    Reserved11,       // Unknown
    Reserved12,       // Unknown
    Reserved13,       // Unknown
    Reserved14,       // Unknown
    Reserved15,       // Unknown
    Reserved16,       // Unknown
    Reserved17,       // Unknown
    AccelXH,          // X axis accelerometer Big Endian 0 - 1023
    AccelXL,          // Low
    AccelYH,          // Y axis accelerometer Big Endian 0 - 1023
    AccelYL,          // Low
    AccelZH,          // Z Accelerometer Big Endian 0 - 1023
    AccelZL,          // Low
    GyroZH,           // Z axis Gyro Big Endian 0 - 1023
    GyroZL,           // Low
};

enum eBUF_SIZE {
    MAX_BUFFER_SIZE = 64, // Size of general purpose data buffer
};

/* HCI macro */
#define hci_timeout (hci_counter_-- == 0)

/* Macros for HCI event flag tests */
#define hci_command_complete (hci_event_flag_ & HCI_FLAG_COMMAND_COMPLETE)
#define hci_command_status (hci_event_flag_ & HCI_FLAG_COMMAND_STATUS)
#define hci_connect_complete (hci_event_flag_ & HCI_FLAG_CONNECT_COMPLETE)
#define hci_disconn_complete (hci_event_flag_ & HCI_FLAG_DISCONN_COMPLETE)
#define hci_incoming_connect_request (hci_event_flag_ & HCI_FLAG_INCOMING_REQUEST)
#define hci_read_bdaddr_complete (hci_event_flag_ & HCI_FLAG_READ_BDADDR)

/* Macros for L2CAP event flag tests */
#define l2cap_command_connected (l2cap_event_status_ & L2CAP_EV_COMMAND_CONNECTED)
#define l2cap_command_request (l2cap_event_status_ & L2CAP_EV_COMMAND_CONFIG_REQ)
#define l2cap_command_configured (l2cap_event_status_ & L2CAP_EV_COMMAND_CONFIGURED)
#define l2cap_command_disconnected (l2cap_event_status_ & L2CAP_EV_COMMAND_DISCONNECT_REQ)
#define l2cap_interrupt_connected (l2cap_event_status_ & L2CAP_EV_INTERRUPT_CONNECTED)
#define l2cap_interrupt_request (l2cap_event_status_ & L2CAP_EV_INTERRUPT_CONFIG_REQ)
#define l2cap_interrupt_configured (l2cap_event_status_ & L2CAP_EV_INTERRUPT_CONFIGURED)
#define l2cap_interrupt_disconnected (l2cap_event_status_ & L2CAP_EV_INTERRUPT_DISCONNECT_REQ)

/* Macros for L2CAP event task tests */
#define acl_handle_ok(handle) ((l2cap_buf[0] | (l2cap_buf[1] << 8)) == ((handle & 0x0FFF) | 0x2000))
#define l2cap_control ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == 0x0001) // Channel ID for ACL-U
#define l2cap_interrupt ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == interrupt_scid_)
#define l2cap_command ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == command_scid_)
#define l2cap_command_reject (l2cap_buf[8] == L2CAP_CMD_COMMAND_REJECT)
#define l2cap_connection_request (l2cap_buf[8] == L2CAP_CMD_CONNECTION_REQUEST)
#define l2cap_configuration_request (l2cap_buf[8] == L2CAP_CMD_CONFIG_REQUEST)
#define l2cap_configuration_response (l2cap_buf[8] == L2CAP_CMD_CONFIG_RESPONSE)
#define l2cap_configuration_success ((l2cap_buf[16] | (l2cap_buf[17] << 8)) == 0x0000)
#define l2cap_disconnect_request (l2cap_buf[8] == L2CAP_CMD_DISCONNECT_REQUEST)

/* Macros for HID event flag tests */
#define hid_buttons_changed (hid_flags_ & HID_FLAG_BUTTONS_CHANGED)
#define hid_extension (hid_flags_ & HID_FLAG_EXTENSION)
#define hid_command_success (hid_flags_ & HID_FLAG_COMMAND_SUCCESS)
#define hid_handshake_success (l2cap_buf[8] == 0)
#define hid_input_report ((data[8] == HID_THDR_DATA_INPUT) & (data[9] == PS3_01_REPORT_ID))

int ds3bt_init(uint8_t pads);
int ds3bt_get_status(int port);
void ds3bt_reset();
int ds3bt_get_data(char *dst, int size, int port);
void ds3bt_set_rumble(uint8_t lrum, uint8_t rrum, int port);
void ds3bt_set_mode(int mode, int lock, int port);

#endif
