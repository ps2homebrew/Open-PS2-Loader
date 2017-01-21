#ifndef _DS3USB_H_
#define _DS3USB_H_

#include "types.h"
#include "usbd.h"

typedef u8 uint8_t;
typedef u16 uint16_t;

void mips_memcpy(void *, const void *, unsigned);
void mips_memset(void *, int, unsigned);

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
    // }}}

    bmREQ_USB_OUT = (USB_DIR_OUT |
                     USB_TYPE_CLASS |
                     USB_RECIP_INTERFACE),

    bmREQ_USB_IN = (USB_DIR_IN |
                    USB_TYPE_CLASS |
                    USB_RECIP_INTERFACE),

};


enum eDS3USBLEDRumble {
    psLEDN = 0x00,
    psLED1 = 0x02,
    psLED2 = 0x04,
    psLED3 = 0x08,
    psLED4 = 0x0F,
    psLEDA = 0x10,
};

enum eDS3USBStatus {
    DS3USB_STATE_DISCONNECTED = 0x00,
    DS3USB_STATE_AUTHORIZED = 0x01,
    DS3USB_STATE_CONFIGURED = 0x02,
    DS3USB_STATE_CONNECTED = 0x04,
    DS3USB_STATE_RUNNING = 0x08,
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

void ds3usb_init(uint8_t pads);
int ds3usb_get_status(int port);
void ds3usb_reset();
void ds3usb_get_data(char *dst, int size, int port);
void ds3usb_set_rumble(uint8_t lrum, uint8_t rrum, int port);
void ds3usb_set_led(uint8_t led, int port);
void ds3usb_get_bdaddr(uint8_t *data, int port);
void ds3usb_set_bdaddr(uint8_t *data, int port);

#endif
