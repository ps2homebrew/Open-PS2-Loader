
enum eDS34USBStatus {
    DS34USB_STATE_DISCONNECTED = 0x00,
    DS34USB_STATE_AUTHORIZED = 0x01,
    DS34USB_STATE_CONFIGURED = 0x02,
    DS34USB_STATE_CONNECTED = 0x04,
    DS34USB_STATE_RUNNING = 0x08,
};

int ds34usb_init();
int ds34usb_deinit();
int ds34usb_reinit_ports(u8 ports);
int ds34usb_get_status(int port);
int ds34usb_get_bdaddr(int port, u8 *bdaddr);
int ds34usb_set_bdaddr(int port, u8 *bdaddr);
int ds34usb_set_rumble(int port, u8 lrum, u8 rrum);
int ds34usb_set_led(int port, u8 led);
int ds34usb_get_data(int port, u8 *data);
int ds34usb_reset();
