#ifndef _PADEMU_COMMON_H_
#define _PADEMU_COMMON_H_
#include <stdint.h>

#define USB_CLASS_WIRELESS_CONTROLLER 0xE0
#define USB_SUBCLASS_RF_CONTROLLER    0x01
#define USB_PROTOCOL_BLUETOOTH_PROG   0x01

#define SONY_VID            0x12ba // Sony
#define DS_VID              0x054C // Sony Corporation
#define DS3_PID             0x0268 // PS3 Controller
#define DS4_PID             0x05C4 // PS4 Controller
#define DS4_PID_SLIM        0x09CC // PS4 Slim Controller
#define GUITAR_HERO_PS3_PID 0x0100 // PS3 Guitar Hero Guitar
#define ROCK_BAND_PS3_PID   0x0200 // PS3 Rock Band Guitar
#define DS3                 0
#define DS4                 1
#define MAX_BUFFER_SIZE     64 // Size of general purpose data buffer

#define MAX_PORTS 4


enum DS2Buttons {
    DS2ButtonSelect = (1 << 0),
    DS2ButtonL3 = (1 << 1),
    DS2ButtonR3 = (1 << 2),
    DS2ButtonStart = (1 << 3),
    DS2ButtonUp = (1 << 4),
    DS2ButtonRight = (1 << 5),
    DS2ButtonDown = (1 << 6),
    DS2ButtonLeft = (1 << 7),
    DS2ButtonL2 = (1 << 8),
    DS2ButtonR2 = (1 << 9),
    DS2ButtonL1 = (1 << 10),
    DS2ButtonR1 = (1 << 11),
    DS2ButtonTriangle = (1 << 12),
    DS2ButtonCircle = (1 << 13),
    DS2ButtonCross = (1 << 14),
    DS2ButtonSquare = (1 << 15),
};

struct ds2report
{
    union
    {
        uint16_t nButtonState;
        struct
        {
            uint8_t nButtonStateL; // Main buttons low byte (active-low)
            uint8_t nButtonStateH; // Main buttons high byte (active-low)
        };
        struct
        {
            uint16_t nSelect   : 1;
            uint16_t nL3       : 1;
            uint16_t nR3       : 1;
            uint16_t nStart    : 1;
            uint16_t nUp       : 1;
            uint16_t nRight    : 1;
            uint16_t nDown     : 1;
            uint16_t nLeft     : 1;
            uint16_t nL2       : 1;
            uint16_t nR2       : 1;
            uint16_t nL1       : 1;
            uint16_t nR1       : 1;
            uint16_t nTriangle : 1;
            uint16_t nCircle   : 1;
            uint16_t nCross    : 1;
            uint16_t nSquare   : 1;
        };
    };
    uint8_t RightStickX;
    uint8_t RightStickY;
    uint8_t LeftStickX;
    uint8_t LeftStickY;

    uint8_t PressureRight;
    uint8_t PressureLeft;
    uint8_t PressureUp;
    uint8_t PressureDown;

    uint8_t PressureTriangle;
    uint8_t PressureCircle;
    uint8_t PressureCross;
    uint8_t PressureSquare;

    uint8_t PressureL1;
    uint8_t PressureR1;
    uint8_t PressureL2;
    uint8_t PressureR2;

} __attribute__((packed));

struct ds3report
{
    union
    {
        uint16_t ButtonState;
        struct
        {
            uint8_t ButtonStateL; // Main buttons low byte
            uint8_t ButtonStateH; // Main buttons high byte
        };
        struct
        {
            uint16_t Select   : 1;
            uint16_t L3       : 1;
            uint16_t R3       : 1;
            uint16_t Start    : 1;
            uint16_t Up       : 1;
            uint16_t Right    : 1;
            uint16_t Down     : 1;
            uint16_t Left     : 1;
            uint16_t L2       : 1;
            uint16_t R2       : 1;
            uint16_t L1       : 1;
            uint16_t R1       : 1;
            uint16_t Triangle : 1;
            uint16_t Circle   : 1;
            uint16_t Cross    : 1;
            uint16_t Square   : 1;
        };
    };
    uint8_t PSButton;         // PS button
    uint8_t Reserved1;        // Unknown
    uint8_t LeftStickX;       // left Joystick X axis 0 - 255, 128 is mid
    uint8_t LeftStickY;       // left Joystick Y axis 0 - 255, 128 is mid
    uint8_t RightStickX;      // right Joystick X axis 0 - 255, 128 is mid
    uint8_t RightStickY;      // right Joystick Y axis 0 - 255, 128 is mid
    uint8_t Reserved2[4];     // Unknown
    uint8_t PressureUp;       // digital Pad Up button Pressure 0 - 255
    uint8_t PressureRight;    // digital Pad Right button Pressure 0 - 255
    uint8_t PressureDown;     // digital Pad Down button Pressure 0 - 255
    uint8_t PressureLeft;     // digital Pad Left button Pressure 0 - 255
    uint8_t PressureL2;       // digital Pad L2 button Pressure 0 - 255
    uint8_t PressureR2;       // digital Pad R2 button Pressure 0 - 255
    uint8_t PressureL1;       // digital Pad L1 button Pressure 0 - 255
    uint8_t PressureR1;       // digital Pad R1 button Pressure 0 - 255
    uint8_t PressureTriangle; // digital Pad Triangle button Pressure 0 - 255
    uint8_t PressureCircle;   // digital Pad Circle button Pressure 0 - 255
    uint8_t PressureCross;    // digital Pad Cross button Pressure 0 - 255
    uint8_t PressureSquare;   // digital Pad Square button Pressure 0 - 255
    uint8_t Reserved3[3];     // Unknown
    uint8_t Charge;           // charging status ? 02 = charge, 03 = normal
    uint8_t Power;            // Battery status ? 05=full - 02=dying, 01=just before shutdown, EE=charging
    uint8_t Connection;       // Connection Type ? 14 when operating by bluetooth, 10 when operating by bluetooth with cable plugged in, 16 when bluetooh and rumble
    uint8_t Reserved4[9];     // Unknown
    int16_t AccelX;
    int16_t AccelY;
    int16_t AccelZ;
    int16_t GyroZ;

} __attribute__((packed));

struct ds3guitarreport
{
    union
    {
        uint16_t ButtonState;
        struct
        {
            uint8_t ButtonStateL; // Main buttons low byte
            uint8_t ButtonStateH; // Main buttons high byte
        };
        struct
        {
            uint16_t Blue      : 1;
            uint16_t Green     : 1;
            uint16_t Red       : 1;
            uint16_t Yellow    : 1;
            uint16_t Orange    : 1;
            uint16_t StarPower : 1;
            uint16_t           : 1;
            uint16_t           : 1;
            uint16_t Select    : 1;
            uint16_t Start     : 1;
            uint16_t           : 1;
            uint16_t           : 1;
            uint16_t PSButton  : 1;
            uint16_t           : 1;
            uint16_t           : 1;
            uint16_t           : 1;
        };
    };
    uint8_t Dpad; // hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW
    uint8_t : 8;
    uint8_t : 8;
    uint8_t Whammy; // Whammy axis 0 - 255, 128 is mid
    uint8_t : 8;
    uint8_t PressureRightYellow; // digital Pad Right + Yellow button Pressure 0 - 255 (if both are pressed, then they cancel eachother out)
    uint8_t PressureLeft;        // digital Pad Left button Pressure 0 - 255
    uint8_t PressureUpGreen;     // digital Pad Up + Green button Pressure 0 - 255 (if both are pressed, then they cancel eachother out)
    uint8_t PressureDownOrange;  // digital Pad Down + Orange button Pressure 0 - 255 (if both are pressed, then they cancel eachother out)
    uint8_t PressureBlue;        // digital Pad Blue button Pressure 0 - 255
    uint8_t PressureRed;         // digital Pad Red button Pressure 0 - 255
    uint8_t Reserved3[6];        // Unknown
    int16_t AccelX;
    int16_t AccelZ;
    int16_t AccelY;
    int16_t GyroZ;

} __attribute__((packed));

enum DS4DpadDirections {
    DS4DpadDirectionN = 0,
    DS4DpadDirectionNE,
    DS4DpadDirectionE,
    DS4DpadDirectionSE,
    DS4DpadDirectionS,
    DS4DpadDirectionSW,
    DS4DpadDirectionW,
    DS4DpadDirectionNW,
    DS4DpadDirectionReleased,
};

struct ds4report
{
    uint8_t ReportID;
    uint8_t LeftStickX;   // left Joystick X axis 0 - 255, 128 is mid
    uint8_t LeftStickY;   // left Joystick Y axis 0 - 255, 128 is mid
    uint8_t RightStickX;  // right Joystick X axis 0 - 255, 128 is mid
    uint8_t RightStickY;  // right Joystick Y axis 0 - 255, 128 is mid
    uint8_t Dpad     : 4; // hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW
    uint8_t Square   : 1;
    uint8_t Cross    : 1;
    uint8_t Circle   : 1;
    uint8_t Triangle : 1;
    uint8_t L1       : 1;
    uint8_t R1       : 1;
    uint8_t L2       : 1;
    uint8_t R2       : 1;
    uint8_t Share    : 1;
    uint8_t Option   : 1;
    uint8_t L3       : 1;
    uint8_t R3       : 1;
    uint8_t PSButton : 1;
    uint8_t TPad     : 1;
    uint8_t Counter1 : 6; // counts up by 1 per report
    uint8_t PressureL2;   // digital Pad L2 button Pressure 0 - 255
    uint8_t PressureR2;   // digital Pad R2 button Pressure 0 - 255
    uint8_t Counter2;
    uint8_t Counter3;
    uint8_t Battery; // battery level from 0x00 to 0xff
    int16_t AccelX;
    int16_t AccelY;
    int16_t AccelZ;
    int16_t GyroZ;
    int16_t GyroY;
    int16_t GyroX;
    uint8_t Reserved1[5];    // Unknown
    uint8_t Power       : 4; // from 0x0 to 0xA - charging, 0xB - charged
    uint8_t Usb_plugged : 1;
    uint8_t Headphones  : 1;
    uint8_t Microphone  : 1;
    uint8_t Padding     : 1;
    uint8_t Reserved2[2];        // Unknown
    uint8_t TPpack;              // number of trackpad packets (0x00 to 0x04)
    uint8_t PackCounter;         // packet counter
    uint8_t Finger1ID      : 7;  // counter
    uint8_t nFinger1Active : 1;  // 0 - active, 1 - unactive
    uint16_t Finger1X      : 12; // finger 1 coordinates resolution 1920x943
    uint16_t Finger1Y      : 12;
    uint8_t Finger2ID      : 7;
    uint8_t nFinger2Active : 1;
    uint16_t Finger2X      : 12; // finger 2 coordinates resolution 1920x943
    uint16_t Finger2Y      : 12;

} __attribute__((packed));

typedef struct
{
    int devId;
    int hid_sema;
    int controlEndp;
    int interruptEndp;
    int inEndp;
    int outEndp;
    uint8_t status;
} bt_device;

typedef struct _usb_ds34
{
    int devId;
    int sema;
    int cmd_sema;
    int controlEndp;
    int interruptEndp;
    int outEndp;
    uint8_t enabled;
    uint8_t status;
    uint8_t type;      // 0 - ds3, 1 - ds4, 2 - guitar hero guitar, 3 - rock band guitar
    uint8_t oldled[4]; // rgb for ds4 and blink
    uint8_t lrum;
    uint8_t rrum;
    uint8_t update_rum;
    union
    {
        struct ds2report ds2;
        uint8_t data[18];
    };
    uint8_t analog_btn;
    uint8_t btn_delay;
} ds34usb_device;

enum eHID {
    // {{{
    /* HID event flag */
    HID_FLAG_STATUS_REPORTED = 0x01,
    HID_FLAG_BUTTONS_CHANGED = 0x02,
    HID_FLAG_EXTENSION = 0x04,
    HID_FLAG_COMMAND_SUCCESS = 0x08,

    /* USB HID Transaction Header (THdr) */
    HID_USB_GET_REPORT_FEATURE = 0x03,
    HID_USB_SET_REPORT_OUTPUT = 0x02,
    HID_USB_DATA_INPUT = 0x01,

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
    HCI_OCF_DISCONNECT = 0x06,             // OGF = 0x01
    HCI_OCF_ACCEPT_CONNECTION = 0x09,      // OGF = 0x01
    HCI_OCF_REJECT_CONNECTION = 0x0A,      // OGF = 0x01
    HCI_OCF_CHANGE_CONNECTION_TYPE = 0x0F, // OGF = 0x01
    HCI_OCF_REMOTE_NAME = 0x19,            // OGF = 0x01
    HCI_OCF_LINK_KEY_REQUEST_REPLY = 0x0B, // OGF = 0x01

    HCI_OCF_RESET = 0x03,                // OGF = 0x03
    HCI_OCF_WRITE_ACCEPT_TIMEOUT = 0x16, // OGF = 0x03
    HCI_OCF_WRITE_SCAN_ENABLE = 0x1A,    // OGF = 0x03

    HCI_OCF_READ_BDADDR = 0x09, // OGF = 0x04

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

/**
 * Translate DS3 pad data into DS2 pad data.
 * @param in DS3 report
 * @param out DS2 report
 * @param pressure_emu set to 1 to extrapolate digital buttons into button pressure
 * NOTE: if set to 0, ds3report must be large enough for that data to be read!
 */
void translate_pad_ds3(const struct ds3report *in, struct ds2report *out, uint8_t pressure_emu);

/**
 * Translate PS3 Guitar pad data into DS2 Guitar pad data.
 * @param in PS3 Guitar report
 * @param out PS2 Guitar report
 * @param guitar_hero_format set to 1 if this is a guitar hero guitar, set to 0 if this is a rock band guitar
 */
void translate_pad_guitar(const struct ds3guitarreport *in, struct ds2report *out, uint8_t guitar_hero_format);

/**
 * Translate DS3 pad data into DS2 pad data.
 * @param in DS4 report
 * @param out DS2 report
 * @param have_touchpad set to 1 if input report has touchpad data
 * NOTE: if set to 1, ds4report must be large enough for that data to be read!
 */
void translate_pad_ds4(const struct ds4report *in, struct ds2report *out, uint8_t have_touchpad);

#endif
