#ifndef _DS34BT_H_
#define _DS34BT_H_

#include "irx.h"

#define USB_CLASS_WIRELESS_CONTROLLER 0xE0
#define USB_SUBCLASS_RF_CONTROLLER    0x01
#define USB_PROTOCOL_BLUETOOTH_PROG   0x01

#define DS34_VID     0x054C // Sony Corporation
#define DS3_PID      0x0268 // PS3 Controller
#define DS4_PID      0x05C4 // PS4 Controller
#define DS4_PID_SLIM 0x09CC // PS4 Slim Controller

#define DS3 0
#define DS4 1

#define MAX_BUFFER_SIZE 64 // Size of general purpose data buffer

#define PENDING    1
#define SUCCESSFUL 0

typedef struct
{
    int devId;
    int hid_sema;
    int controlEndp;
    int interruptEndp;
    int inEndp;
    int outEndp;
    u8 status;
    u16 vid;
    u16 pid;
    u16 rev;
} bt_device;

typedef struct
{
    u16 hci_handle;     // hci connection handle
    u16 control_scid;   // Channel endpoint on command destination
    u16 interrupt_scid; // Channel endpoint on interrupt destination
    u8 hci_state;       // current state of bluetooth HCI connection
    u8 bdaddr[6];
    u8 enabled;
    u8 status;
    u8 isfake;
    u8 type;      // 0 - ds3, 1 - ds4
    u8 oldled[4]; // rgb for ds4 and blink
    u8 lrum;
    u8 rrum;
    u8 update_rum;
    u8 data[18];
} ds34bt_pad_t;

typedef struct
{
    u8 hci_ver;     // Version of the Current HCI in the BR/EDR Controller
    u16 hci_rev;    // Revision of the Current HCI in the BR/EDR Controller
    u8 lmp_ver;     // Version of the Current LMP or PAL in the Controller
    u16 mf_name;    // Manufacturer Name of the BR/EDR Controller
    u16 lmp_subver; // Subversion of the Current LMP or PAL in the Controller
    u16 vid;
    u16 pid;
    u16 rev;
} __attribute__((packed)) hci_information_t;

enum eDS34BTStatus {
    DS34BT_STATE_USB_DISCONNECTED = 0x00,
    DS34BT_STATE_USB_AUTHORIZED = 0x01,
    DS34BT_STATE_USB_CONFIGURED = 0x02,
    DS34BT_STATE_CONNECTED = 0x04,
    DS34BT_STATE_RUNNING = 0x08,
    DS34BT_STATE_DISCONNECTING = 0x10,
    DS34BT_STATE_DISCONNECT_REQUEST = 0x20,
};

#define pad_status_clear(flag, pad) ds34pad[pad].status &= ~flag
#define pad_status_set(flag, pad)   ds34pad[pad].status |= flag
#define pad_status_check(flag, pad) (ds34pad[pad].status & flag)

#define hci_event_flag_clear(flag) hci_event_flag &= ~flag
#define hci_event_flag_set(flag)   hci_event_flag |= flag
#define hci_event_flag_check(flag) (hci_event_flag & flag)

enum eHCI {
    // {{{
    /* Bluetooth HCI states for HCI_task() */
    HCI_INIT_STATE = 0,
    HCI_RESET_STATE,
    HCI_READBDADDR_STATE,
    HCI_READVERSION_STATE,
    HCI_READFEATURES_STATE,
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
    HCI_OCF_DISCONNECT = 0x06,             // OGF = 0x01
    HCI_OCF_ACCEPT_CONNECTION = 0x09,      // OGF = 0x01
    HCI_OCF_REJECT_CONNECTION = 0x0A,      // OGF = 0x01
    HCI_OCF_CHANGE_CONNECTION_TYPE = 0x0F, // OGF = 0x01
    HCI_OCF_REMOTE_NAME = 0x19,            // OGF = 0x01
    HCI_OCF_LINK_KEY_REQUEST_REPLY = 0x0B, // OGF = 0x01

    HCI_OCF_RESET = 0x03,                // OGF = 0x03
    HCI_OCF_WRITE_ACCEPT_TIMEOUT = 0x16, // OGF = 0x03
    HCI_OCF_WRITE_SCAN_ENABLE = 0x1A,    // OGF = 0x03

    HCI_OCF_READ_BDADDR = 0x09,   // OGF = 0x04
    HCI_OCF_READ_VERSION = 0x01,  // OGF = 0x04
    HCI_OCF_READ_FEATURES = 0x03, // OGF = 0x04

    /* HCI events managed */
    HCI_EVENT_CONNECT_COMPLETE = 0x03,
    HCI_EVENT_CONNECT_REQUEST = 0x04,
    HCI_EVENT_DISCONN_COMPLETE = 0x05,
    HCI_EVENT_AUTHENTICATION_COMPLETE = 0x06,
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
    HCI_EVENT_MAX_SLOT_CHANGE = 0x1B, // Max Slots Change event

    /* HCI event flags for hci_event_flag */
    HCI_FLAG_COMMAND_COMPLETE = 0x0001,
    HCI_FLAG_COMMAND_STATUS = 0x0002,
    HCI_FLAG_CONNECT_COMPLETE = 0x0004,
    HCI_FLAG_DISCONN_COMPLETE = 0x0008,
    HCI_FLAG_INCOMING_REQUEST = 0x0010,
    HCI_FLAG_READ_BDADDR = 0x0020,
    HCI_FLAG_REMOTE_NAME_COMPLETE = 0x0040,
    HCI_FLAG_READ_VERSION = 0x0080,
    HCI_FLAG_READ_FEATURES = 0x0100,

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
    L2CAP_PSM_INTR = 0x13, // HID_Interrupt

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
#define l2cap_control_channel   ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == 0x0001) // Channel ID for ACL-U
#define l2cap_interrupt_channel ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == interrupt_dcid)
#define l2cap_command_channel   ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == control_dcid)

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

    PS4_02_REPORT_ID = 0x02,
    PS4_11_REPORT_ID = 0x11,
    PS4_11_REPORT_LEN = 0x4D,

    // }}}
};

struct ds3report
{
    union
    {
        u8 ButtonStateL; // Main buttons Low
        struct
        {
            u8 Select : 1;
            u8 L3     : 1;
            u8 R3     : 1;
            u8 Start  : 1;
            u8 Up     : 1;
            u8 Right  : 1;
            u8 Down   : 1;
            u8 Left   : 1;
        };
    };
    union
    {
        u8 ButtonStateH; // Main buttons High
        struct
        {
            u8 L2       : 1;
            u8 R2       : 1;
            u8 L1       : 1;
            u8 R1       : 1;
            u8 Triangle : 1;
            u8 Circle   : 1;
            u8 Cross    : 1;
            u8 Square   : 1;
        };
    };
    u8 PSButtonState;    // PS button
    u8 Reserved1;        // Unknown
    u8 LeftStickX;       // left Joystick X axis 0 - 255, 128 is mid
    u8 LeftStickY;       // left Joystick Y axis 0 - 255, 128 is mid
    u8 RightStickX;      // right Joystick X axis 0 - 255, 128 is mid
    u8 RightStickY;      // right Joystick Y axis 0 - 255, 128 is mid
    u8 Reserved2[4];     // Unknown
    u8 PressureUp;       // digital Pad Up button Pressure 0 - 255
    u8 PressureRight;    // digital Pad Right button Pressure 0 - 255
    u8 PressureDown;     // digital Pad Down button Pressure 0 - 255
    u8 PressureLeft;     // digital Pad Left button Pressure 0 - 255
    u8 PressureL2;       // digital Pad L2 button Pressure 0 - 255
    u8 PressureR2;       // digital Pad R2 button Pressure 0 - 255
    u8 PressureL1;       // digital Pad L1 button Pressure 0 - 255
    u8 PressureR1;       // digital Pad R1 button Pressure 0 - 255
    u8 PressureTriangle; // digital Pad Triangle button Pressure 0 - 255
    u8 PressureCircle;   // digital Pad Circle button Pressure 0 - 255
    u8 PressureCross;    // digital Pad Cross button Pressure 0 - 255
    u8 PressureSquare;   // digital Pad Square button Pressure 0 - 255
    u8 Reserved3[3];     // Unknown
    u8 Charge;           // charging status ? 02 = charge, 03 = normal
    u8 Power;            // Battery status ? 05=full - 02=dying, 01=just before shutdown, EE=charging
    u8 Connection;       // Connection Type ? 14 when operating by bluetooth, 10 when operating by bluetooth with cable plugged in, 16 when bluetooh and rumble
    u8 Reserved4[9];     // Unknown
    s16 AccelX;
    s16 AccelY;
    s16 AccelZ;
    s16 GyroZ;

} __attribute__((packed));

struct ds4report
{
    u8 ReportID;
    u8 LeftStickX;   // left Joystick X axis 0 - 255, 128 is mid
    u8 LeftStickY;   // left Joystick Y axis 0 - 255, 128 is mid
    u8 RightStickX;  // right Joystick X axis 0 - 255, 128 is mid
    u8 RightStickY;  // right Joystick Y axis 0 - 255, 128 is mid
    u8 Dpad     : 4; // hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW
    u8 Square   : 1;
    u8 Cross    : 1;
    u8 Circle   : 1;
    u8 Triangle : 1;
    u8 L1       : 1;
    u8 R1       : 1;
    u8 L2       : 1;
    u8 R2       : 1;
    u8 Share    : 1;
    u8 Option   : 1;
    u8 L3       : 1;
    u8 R3       : 1;
    u8 PSButton : 1;
    u8 TPad     : 1;
    u8 Counter1 : 6; // counts up by 1 per report
    u8 PressureL2;   // digital Pad L2 button Pressure 0 - 255
    u8 PressureR2;   // digital Pad R2 button Pressure 0 - 255
    u8 Counter2;
    u8 Counter3;
    u8 Battery; // battery level from 0x00 to 0xff
    s16 AccelX;
    s16 AccelY;
    s16 AccelZ;
    s16 GyroZ;
    s16 GyroY;
    s16 GyroX;
    u8 Reserved1[5];    // Unknown
    u8 Power       : 4; // from 0x0 to 0xA - charging, 0xB - charged
    u8 Usb_plugged : 1;
    u8 Headphones  : 1;
    u8 Microphone  : 1;
    u8 Padding     : 1;
    u8 Reserved2[2];       // Unknown
    u8 TPpack;             // number of trackpad packets (0x00 to 0x04)
    u8 PackCounter;        // packet counter
    u8 Finger1ID     : 7;  // counter
    u8 Finger1Active : 1;  // 0 - active, 1 - unactive
    u16 Finger1X     : 12; // finger 1 coordinates resolution 1920x943
    u16 Finger1Y     : 12;
    u8 Finger2ID     : 7;
    u8 Finger2Active : 1;
    u16 Finger2X     : 12; // finger 2 coordinates resolution 1920x943
    u16 Finger2Y     : 12;

} __attribute__((packed));

void mips_memcpy(void *, const void *, unsigned);
void mips_memset(void *, int, unsigned);

#endif
