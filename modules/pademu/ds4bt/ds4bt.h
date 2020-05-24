#ifndef _DS4BT_H_
#define _DS4BT_H_

#define MAX_BUFFER_SIZE 64 // Size of general purpose data buffer

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

typedef struct {
    u8 ReportID;
    u8 Unk;
    u8 ReportID2;
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
