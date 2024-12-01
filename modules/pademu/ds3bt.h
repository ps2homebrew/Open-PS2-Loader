#ifndef _DS3BT_H_
#define _DS3BT_H_

#include "irx.h"

#define PENDING    1
#define SUCCESSFUL 0

typedef struct
{
    u16 hci_handle;     // hci connection handle
    u16 control_scid;   // Channel endpoint on command destination
    u16 interrupt_scid; // Channel endpoint on interrupt destination
    u8 bdaddr[6];
    u8 enabled;
    u8 status;
    u8 isfake;
    u8 type;      // 0 - ds3, 1 - ds4
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
} ds3bt_pad_t;

enum eDS34BTStatus {
    DS3BT_STATE_USB_DISCONNECTED = 0x00,
    DS3BT_STATE_USB_AUTHORIZED = 0x01,
    DS3BT_STATE_USB_CONFIGURED = 0x02,
    DS3BT_STATE_CONNECTED = 0x04,
    DS3BT_STATE_RUNNING = 0x08,
    DS3BT_STATE_DISCONNECTING = 0x10,
    DS3BT_STATE_DISCONNECT_REQUEST = 0x20,
};

#define pad_status_clear(flag, pad) ds3pad[pad].status &= ~flag
#define pad_status_set(flag, pad)   ds3pad[pad].status |= flag
#define pad_status_check(flag, pad) (ds3pad[pad].status & flag)

#define hci_event_flag_clear(flag) hci_event_flag &= ~flag
#define hci_event_flag_set(flag)   hci_event_flag |= flag
#define hci_event_flag_check(flag) (hci_event_flag & flag)

#define l2cap_handle_ok(handle) (((u16)(l2cap_buf[0] | (l2cap_buf[1] << 8)) & 0x0FFF) == (u16)(handle & 0x0FFF))
#define l2cap_control_channel   ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == 0x0001) // Channel ID for ACL-U
#define l2cap_interrupt_channel ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == interrupt_dcid)
#define l2cap_command_channel   ((l2cap_buf[6] | (l2cap_buf[7] << 8)) == control_dcid)

int ds3bt_init(u8 pads, u8 options);
void ds3bt_reset();

#endif
