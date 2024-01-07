#ifndef _HIDUSB_H_
#define _HIDUSB_H_

#include "irx.h"
#include "../pademu.h"

#define MAX_BUFFER_SIZE 64 // Size of general purpose data buffer

#define USB_DT_HID 0x21

/* HID Descriptor (Class Specific Descriptor) */
typedef struct
{
    u8 bDescriptorType;
    u8 wDescriptorLengthL;
    u8 wDescriptorLengthH;
} UsbHidSubDescriptorInfo;

typedef struct
{
    u8 bLength;
    u8 bDescriptorType;
    u16 bcdHID;
    u8 bCountryCode;
    u8 bNumDescriptors; /* Number of SubDescriptor */
    UsbHidSubDescriptorInfo Sub[0];
} UsbHidDescriptor;

typedef struct
{
    u16 count;
    u16 size;      //in bits
    int start_pos; //position in report
} hiddata_t;

typedef struct
{
    hiddata_t axes;
    hiddata_t hats;
    hiddata_t buttons;
} hidreport_t;

typedef struct
{
    pad_device_t dev;
    hidreport_t rep;
    int usb_id;
    int sema;
    int cmd_sema;
    int controlEndp;
    int interruptEndp;
    int usb_resultcode;
    u8 lrum;
    u8 rrum;
    u8 update_rum;
    u8 data[18];
    u8 analog_btn;
    u8 btn_delay;
} hidusb_device;

#endif
