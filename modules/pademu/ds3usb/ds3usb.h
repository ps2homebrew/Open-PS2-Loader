#ifndef _DS3USB_H_
#define _DS3USB_H_

#include "irx.h"
#include "../pademu.h"

//#include <ds34common.h>

#define DS3       0
#define DS4       1
#define GUITAR_GH 2
#define GUITAR_RB 3

#define MODEL_GUITAR 1
#define MODEL_PS2    3
#define SONY_VID     0x054C // Sony Corporation
#define DS3_PID      0x0268 // PS3 Controller
#define SONY_VID 0x054C // Sony Corporation
#define DS3_PID 0x0268  // PS3 Controller

#define MAX_BUFFER_SIZE 64 // Size of general purpose data buffer

typedef struct
{
    pad_device_t dev;
    int usb_id;
    int sema;
    int cmd_sema;
    int controlEndp;
    int interruptEndp;
    int outEndp;
    u8 enabled;
    u8 status;
    u8 type;      // 0 - ds3, 1 - ds4, 2 - guitar hero guitar, 3 - rock band guitar
    u8 oldled[4]; // rgb for ds4 and blink
    int usb_resultcode;
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
} ds3usb_device;

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
    u8 ReportID;
    u8 Zero;
    union
    {
        u8 ButtonStateL; // Main buttons Low
        struct {
            u8 Select : 1;
            u8 L3 : 1;
            u8 R3 : 1;
            u8 Start : 1;
            u8 Up : 1;
            u8 Right : 1;
            u8 Down : 1;
            u8 Left : 1;
        };
    };
    union {
        u8 ButtonStateH;     // Main buttons High
        struct {
            u8 L2 : 1;
            u8 R2 : 1;
            u8 L1 : 1;
            u8 R1 : 1;
            u8 Triangle : 1;
            u8 Circle : 1;
            u8 Cross : 1;
            u8 Square : 1;
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

} __attribute__((packed)) ds3report_t;

#endif
