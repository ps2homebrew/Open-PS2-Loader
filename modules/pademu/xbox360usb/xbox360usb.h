#ifndef _XBOX360USB_H_
#define _XBOX360USB_H_

#include "irx.h"
#include "../pademu.h"

#define XBOX_VID 0x045E     // Microsoft Corporation
#define MADCATZ_VID 0x1BAD  // For unofficial Mad Catz controllers
#define JOYTECH_VID 0x162E  // For unofficial Joytech controllers
#define GAMESTOP_VID 0x0E6F // Gamestop controller

#define XBOX_WIRED_PID 0x028E                         // Microsoft 360 Wired controller
#define XBOX_WIRELESS_PID 0x028F                      // Wireless controller only support charging
#define XBOX_WIRELESS_RECEIVER_PID 0x0719             // Microsoft Wireless Gaming Receiver
#define XBOX_WIRELESS_RECEIVER_THIRD_PARTY_PID 0x0291 // Third party Wireless Gaming Receiver
#define MADCATZ_WIRED_PID 0xF016                      // Mad Catz wired controller
#define JOYTECH_WIRED_PID 0xBEEF                      // For Joytech wired controller
#define GAMESTOP_WIRED_PID 0x0401                     // Gamestop wired controller
#define AFTERGLOW_WIRED_PID 0x0213                    // Afterglow wired controller - it uses the same VID as a Gamestop controller

#define MAX_BUFFER_SIZE 64 // Size of general purpose data buffer

typedef struct
{
    pad_device_t dev;
    int usb_id;
    int sema;
    int cmd_sema;
    int controlEndp;
    int interruptEndp;
    int usb_resultcode;
    u8 oldled[2];
    u8 lrum;
    u8 rrum;
    u8 update_rum;
    u8 data[18];
    u8 analog_btn;
    u8 btn_delay;
} xbox360usb_device;

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
    u8 Length; //0x14
    union
    {
        u8 ButtonStateL; // Main buttons Low
        struct
        {
            u8 Up : 1;
            u8 Down : 1;
            u8 Left : 1;
            u8 Right : 1;
            u8 Start : 1;
            u8 Back : 1;
            u8 LS : 1;
            u8 RS : 1;
        };
    };
    union
    {
        u8 ButtonStateH; // Main buttons High
        struct
        {
            u8 LB : 1;
            u8 RB : 1;
            u8 XBOX : 1;
            u8 Dummy1 : 1;
            u8 A : 1;
            u8 B : 1;
            u8 X : 1;
            u8 Y : 1;
        };
    };
    u8 LeftTrigger;
    u8 RightTrigger;
    union
    {
        u16 LeftStickX;
        struct
        {
            u8 LeftStickXL;
            u8 LeftStickXH;
        };
    };
    union
    {
        u16 LeftStickY;
        struct
        {
            u8 LeftStickYL;
            u8 LeftStickYH;
        };
    };
    union
    {
        u16 RightStickX;
        struct
        {
            u8 RightStickXL;
            u8 RightStickXH;
        };
    };
    union
    {
        u16 RightStickY;
        struct
        {
            u8 RightStickYL;
            u8 RightStickYH;
        };
    };
} __attribute__((packed)) xbox360report_t;

#endif
