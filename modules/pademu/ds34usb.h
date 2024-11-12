#ifndef _DS34USB_H_
#define _DS34USB_H_

#include "irx.h"

#include <ds34common.h>

#define DS3       0
#define DS4       1
#define GUITAR_GH 2
#define GUITAR_RB 3

#define MODEL_GUITAR 1
#define MODEL_PS2    3

#define MAX_BUFFER_SIZE 64 // Size of general purpose data buffer

enum eDS34USBStatus {
    DS34USB_STATE_DISCONNECTED = 0x00,
    DS34USB_STATE_AUTHORIZED = 0x01,
    DS34USB_STATE_CONFIGURED = 0x02,
    DS34USB_STATE_CONNECTED = 0x04,
    DS34USB_STATE_RUNNING = 0x08,
};

int ds34usb_init(u8 pads, u8 options);
void ds34usb_reset();

#endif
