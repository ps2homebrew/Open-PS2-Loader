#ifndef _DS3USB_H_
#define _DS3USB_H_

#include "irx.h"
#include "usbd.h"
#include "../pademu.h"

#define XBOX_VID 0x045E // Microsoft Corporation

#define XBOX_ONE_PID1 0x02D1  // Microsoft X-Box One pad
#define XBOX_ONE_PID2 0x02DD  // Microsoft X-Box One pad (Firmware 2015)
#define XBOX_ONE_PID3 0x02E3  // Microsoft X-Box One Elite pad
#define XBOX_ONE_PID4 0x02EA  // Microsoft X-Box One S pad
#define XBOX_ONE_PID13 0x0B0A // Microsoft X-Box One Adaptive Controller

// Unofficial controllers
#define XBOX_VID2 0x0738   // Mad Catz
#define XBOX_VID3 0x0E6F   // Afterglow
#define XBOX_VID4 0x0F0D   // HORIPAD ONE
#define XBOX_VID5 0x1532   // Razer
#define XBOX_VID6 0x24C6   // PowerA

#define XBOX_ONE_PID5 0x4A01  // Mad Catz FightStick TE 2 - might have different mapping for triggers?
#define XBOX_ONE_PID6 0x0139  // Afterglow Prismatic Wired Controller
#define XBOX_ONE_PID7 0x0146  // Rock Candy Wired Controller for Xbox One
#define XBOX_ONE_PID8 0x0067  // HORIPAD ONE
#define XBOX_ONE_PID9 0x0A03  // Razer Wildcat
#define XBOX_ONE_PID10 0x541A // PowerA Xbox One Mini Wired Controller
#define XBOX_ONE_PID11 0x542A // Xbox ONE spectra
#define XBOX_ONE_PID12 0x543A // PowerA Xbox One wired controller

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
    UsbEndpointDescriptor *endin;
    UsbEndpointDescriptor *endout;
    u8 lrum;
    u8 rrum;
    u8 update_rum;
    u8 data[18];
    u8 analog_btn;
    u8 btn_delay;
} xboxoneusb_device;

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
    u8 ReportID; // 0x20
    u8 Zero;
    u16 id;

    union
    {
        u8 ButtonStateL; // Main buttons Low
        struct
        {
            u8 Sync : 1;
            u8 Dummy1 : 1;
            u8 Start : 1;
            u8 Back : 1;
            u8 A : 1;
            u8 B : 1;
            u8 X : 1;
            u8 Y : 1;
        };
    };
    union
    {
        u8 ButtonStateH; // Main buttons High
        struct
        {
            u8 Up : 1;
            u8 Down : 1;
            u8 Left : 1;
            u8 Right : 1;
            u8 LB : 1;
            u8 RB : 1;
            u8 LS : 1;
            u8 RS : 1;
        };
    };
    union
    {
        u16 LeftTrigger;
        struct
        {
            u8 LeftTriggerL;
            u8 LeftTriggerH;
        };
    };
    union
    {
        u16 RightTrigger;
        struct
        {
            u8 RightTriggerL;
            u8 RightTriggerH;
        };
    };
    union
    {
        s16 LeftStickX;
        struct
        {
            u8 LeftStickXL;
            u8 LeftStickXH;
        };
    };
    union
    {
        s16 LeftStickY;
        struct
        {
            u8 LeftStickYL;
            u8 LeftStickYH;
        };
    };
    union
    {
        s16 RightStickX;
        struct
        {
            u8 RightStickXL;
            u8 RightStickXH;
        };
    };
    union
    {
        s16 RightStickY;
        struct
        {
            u8 RightStickYL;
            u8 RightStickYH;
        };
    };
} __attribute__((packed)) xboxonereport_t;

#endif
