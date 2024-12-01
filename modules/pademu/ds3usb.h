#ifndef _DS3USB_H_
#define _DS3USB_H_

#include "irx.h"

#define GUITAR_GH 2
#define GUITAR_RB 3

#define MODEL_GUITAR 1
#define MODEL_PS2    3

#define MAX_BUFFER_SIZE 64 // Size of general purpose data buffer

typedef struct _usb_ds3
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
} ds3usb_device;

enum eDS3USBStatus {
    DS3USB_STATE_DISCONNECTED = 0x00,
    DS3USB_STATE_AUTHORIZED = 0x01,
    DS3USB_STATE_CONFIGURED = 0x02,
    DS3USB_STATE_CONNECTED = 0x04,
    DS3USB_STATE_RUNNING = 0x08,
};

int ds3usb_init(u8 pads, u8 options);
void ds3usb_reset();

#endif
