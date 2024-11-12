#ifndef _DS34_H_
#define _DS34_H_

#define DS34_VID        0x054C // Sony Corporation
#define DS3_PID         0x0268 // PS3 Controller
#define DS4_PID         0x05C4 // PS4 Controller
#define DS4_PID_SLIM    0x09CC // PS4 Slim Controller
#define DS3             0
#define DS4             1
#define MAX_BUFFER_SIZE 64 // Size of general purpose data buffer

#define MAX_PORTS 4

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

typedef struct
{
    int devId;
    int hid_sema;
    int controlEndp;
    int interruptEndp;
    int inEndp;
    int outEndp;
    u8 status;
} bt_device;

typedef struct _usb_ds34
{
    int devId;
    int sema;
    int cmd_sema;
    int controlEndp;
    int interruptEndp;
    int outEndp;
    u8 enabled;
    u8 status;
    u8 type;      // 0 - ds3, 1 - ds4, 2 - guitar hero guitar, 3 - rock band guitar
    u8 oldled[4]; // rgb for ds4 and blink
    u8 lrum;
    u8 rrum;
    u8 update_rum;
    union
    {
        struct ds2report ds2;
        u8 data[18];
    };
    u8 analog_btn;
    u8 btn_delay;
} ds34usb_device;


#endif