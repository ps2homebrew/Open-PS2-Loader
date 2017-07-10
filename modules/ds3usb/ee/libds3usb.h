
enum eDS3USBStatus {
    DS3USB_STATE_DISCONNECTED = 0x00,
    DS3USB_STATE_AUTHORIZED = 0x01,
    DS3USB_STATE_CONFIGURED = 0x02,
    DS3USB_STATE_CONNECTED = 0x04,
    DS3USB_STATE_RUNNING = 0x08,
};

int ds3usb_init();
int ds3usb_deinit();
int ds3usb_reinit_ports(u8 ports);
int ds3usb_get_status(int port);
int ds3usb_get_bdaddr(int port, u8 *bdaddr);
int ds3usb_set_bdaddr(int port, u8 *bdaddr);
int ds3usb_set_rumble(int port, u8 lrum, u8 rrum);
int ds3usb_set_led(int port, u8 led);
int ds3usb_get_data(int port, u8 *data);
int ds3usb_reset();
