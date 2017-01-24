#include <kernel.h>
#include <sifrpc.h>
#include <string.h>

#define DS3USB_INIT 1
#define DS3USB_GET_STATUS 2
#define DS3USB_GET_BDADDR 3
#define DS3USB_SET_BDADDR 4
#define DS3USB_SET_RUMBLE 5
#define DS3USB_SET_LED 6
#define DS3USB_GET_DATA 7
#define DS3USB_RESET 8

#define DS3USB_BIND_RPC_ID 0x18E3878E

static SifRpcClientData_t ds3usb;

static u8 rpcbuf[64] __attribute__((aligned(64)));

static u8 ds3usb_inited = 0;

int ds3usb_init()
{
    ds3usb.server = NULL;

    do {
        if (SifBindRpc(&ds3usb, DS3USB_BIND_RPC_ID, 0) < 0)
            return -1;

        nopdelay();
    } while (!ds3usb.server);

    ds3usb_inited = 1;

    return 1;
}

int ds3usb_reinit_ports(u8 ports)
{
    if (!ds3usb_inited)
        return -1;

    rpcbuf[0] = ports;
    return SifCallRpc(&ds3usb, DS3USB_INIT, 0, rpcbuf, 1, NULL, 0, NULL, NULL);
}

int ds3usb_get_status(int port)
{
    if (!ds3usb_inited)
        return -1;

    rpcbuf[0] = port;

    SifCallRpc(&ds3usb, DS3USB_GET_STATUS, 0, rpcbuf, 1, rpcbuf, 1, NULL, NULL);

    return rpcbuf[0];
}

int ds3usb_get_bdaddr(int port, u8 *bdaddr)
{
    int i, ret;

    if (!ds3usb_inited)
        return -1;

    rpcbuf[0] = port;

    ret = SifCallRpc(&ds3usb, DS3USB_GET_BDADDR, 0, rpcbuf, 1, rpcbuf, 6, NULL, NULL);

    for (i = 0; i < 6; i++)
        bdaddr[i] = rpcbuf[i];

    return ret;
}

int ds3usb_set_bdaddr(int port, u8 *bdaddr)
{
    int i;

    if (!ds3usb_inited)
        return -1;

    rpcbuf[0] = port;

    for (i = 0; i < 6; i++)
        rpcbuf[i + 1] = bdaddr[i];

    return SifCallRpc(&ds3usb, DS3USB_SET_BDADDR, 0, rpcbuf, 7, NULL, 0, NULL, NULL);
}

int ds3usb_set_rumble(int port, u8 lrum, u8 rrum)
{
    if (!ds3usb_inited)
        return -1;

    rpcbuf[0] = port;
    rpcbuf[1] = lrum;
    rpcbuf[2] = rrum;

    return SifCallRpc(&ds3usb, DS3USB_SET_RUMBLE, 0, rpcbuf, 3, NULL, 0, NULL, NULL);
}

int ds3usb_set_led(int port, u8 led)
{
    if (!ds3usb_inited)
        return -1;

    rpcbuf[0] = port;
    rpcbuf[1] = led;

    return SifCallRpc(&ds3usb, DS3USB_SET_LED, 0, rpcbuf, 2, NULL, 0, NULL, NULL);
}

int ds3usb_get_data(int port, u8 *data)
{
    int ret;

    if (!ds3usb_inited)
        return -1;

    rpcbuf[0] = port;

    ret = SifCallRpc(&ds3usb, DS3USB_GET_DATA, 0, rpcbuf, 1, rpcbuf, 18, NULL, NULL);

    memcpy(data, rpcbuf, 18);

    return ret;
}

int ds3usb_reset()
{
    if (!ds3usb_inited)
        return -1;

    return SifCallRpc(&ds3usb, DS3USB_RESET, 0, NULL, 0, NULL, 0, NULL, NULL);
}
