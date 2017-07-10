
enum eDS3BTStatus {
    DS3BT_STATE_USB_DISCONNECTED = 0x00,
    DS3BT_STATE_USB_AUTHORIZED = 0x01,
    DS3BT_STATE_USB_CONFIGURED = 0x02,
    DS3BT_STATE_CONNECTED = 0x04,
    DS3BT_STATE_RUNNING = 0x08,
};

int ds3bt_init();
int ds3bt_deinit();
int ds3bt_reinit_ports(u8 ports);
int ds3bt_init_charging();
int ds3bt_get_status(int port);
int ds3bt_get_bdaddr(u8 *bdaddr);
int ds3bt_set_rumble(int port, u8 lrum, u8 rrum);
int ds3bt_set_led(int port, u8 led);
int ds3bt_get_data(int port, u8 *data);
int ds3bt_reset();
