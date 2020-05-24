#ifndef _DS3USB_H_
#define _DS3USB_H_

#include "irx.h"
#include "../pademu.h"

#define SONY_VID     0x054C // Sony Corporation
#define DS4_PID      0x05C4 // PS4 Controller
#define DS4_PID_SLIM 0x09CC // PS4 Slim Controller

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
    int usb_resultcode;
    u8 oldled[4];
    u8 lrum;
    u8 rrum;
    u8 update_rum;
    u8 data[18];
    u8 analog_btn;
    u8 btn_delay;
} ds4usb_device;

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

typedef struct {
    u8 ReportID;
    u8 LeftStickX;  // left Joystick X axis 0 - 255, 128 is mid
    u8 LeftStickY;  // left Joystick Y axis 0 - 255, 128 is mid
    u8 RightStickX; // right Joystick X axis 0 - 255, 128 is mid
    u8 RightStickY; // right Joystick Y axis 0 - 255, 128 is mid
    u8 Dpad : 4;    // hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW
    u8 Square : 1;
    u8 Cross : 1;
    u8 Circle : 1;
    u8 Triangle : 1;
    u8 L1 : 1;
    u8 R1 : 1;
    u8 L2 : 1;
    u8 R2 : 1;
    u8 Share : 1;
    u8 Option : 1;
    u8 L3 : 1;
    u8 R3 : 1;
    u8 PSButton : 1;
    u8 TPad : 1;
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
    u8 Reserved1[5]; // Unknown
    u8 Power : 4;    // from 0x0 to 0xA - charging, 0xB - charged
    u8 Usb_plugged : 1;
    u8 Headphones : 1;
    u8 Microphone : 1;
    u8 Padding : 1;
    u8 Reserved2[2];      // Unknown
    u8 TPpack;            // number of trackpad packets (0x00 to 0x04)
    u8 PackCounter;       // packet counter
    u8 Finger1ID : 7;     // counter
    u8 Finger1Active : 1; // 0 - active, 1 - unactive
    u16 Finger1X : 12;    // finger 1 coordinates resolution 1920x943
    u16 Finger1Y : 12;
    u8 Finger2ID : 7;
    u8 Finger2Active : 1;
    u16 Finger2X : 12; // finger 2 coordinates resolution 1920x943
    u16 Finger2Y : 12;
} __attribute__((packed)) ds4report_t;

#endif
