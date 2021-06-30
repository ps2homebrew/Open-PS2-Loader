#include <kernel.h>
#include <sifrpc.h>
#include <string.h>

#define DS34USB_INIT       1
#define DS34USB_GET_STATUS 2
#define DS34USB_GET_BDADDR 3
#define DS34USB_SET_BDADDR 4
#define DS34USB_SET_RUMBLE 5
#define DS34USB_SET_LED    6
#define DS34USB_GET_DATA   7
#define DS34USB_RESET      8

#define DS34USB_BIND_RPC_ID 0x18E3878E

static SifRpcClientData_t ds34usb;

static u8 rpcbuf[64] __attribute__((aligned(64)));

static u8 ds34usb_inited = 0;

int ds34usb_init()
{
    ds34usb.server = NULL;

    do {
        if (SifBindRpc(&ds34usb, DS34USB_BIND_RPC_ID, 0) < 0)
            return 0;

        nopdelay();
    } while (!ds34usb.server);

    ds34usb_inited = 1;

    return 1;
}

int ds34usb_deinit()
{
    ds34usb_inited = 0;
    return 1;
}

int ds34usb_reinit_ports(u8 ports)
{
    if (!ds34usb_inited)
        return 0;

    rpcbuf[0] = ports;
    return (SifCallRpc(&ds34usb, DS34USB_INIT, 0, rpcbuf, 1, NULL, 0, NULL, NULL) == 0);
}

int ds34usb_get_status(int port)
{
    if (!ds34usb_inited)
        return 0;

    rpcbuf[0] = port;

    if (SifCallRpc(&ds34usb, DS34USB_GET_STATUS, 0, rpcbuf, 1, rpcbuf, 1, NULL, NULL) == 0)
        return rpcbuf[0];

    return 0;
}

int ds34usb_get_bdaddr(int port, u8 *bdaddr)
{
    int i, ret;

    if (!ds34usb_inited)
        return 0;

    rpcbuf[0] = port;

    ret = (SifCallRpc(&ds34usb, DS34USB_GET_BDADDR, 0, rpcbuf, 1, rpcbuf, 7, NULL, NULL) == 0);

    ret &= rpcbuf[0];

    for (i = 0; i < 6; i++)
        bdaddr[i] = rpcbuf[i + 1];

    return ret;
}

int ds34usb_set_bdaddr(int port, u8 *bdaddr)
{
    int i;

    if (!ds34usb_inited)
        return 0;

    rpcbuf[0] = port;

    for (i = 0; i < 6; i++)
        rpcbuf[i + 1] = bdaddr[i];

    return (SifCallRpc(&ds34usb, DS34USB_SET_BDADDR, 0, rpcbuf, 7, NULL, 0, NULL, NULL) == 0);
}

int ds34usb_set_rumble(int port, u8 lrum, u8 rrum)
{
    if (!ds34usb_inited)
        return 0;

    rpcbuf[0] = port;
    rpcbuf[1] = lrum;
    rpcbuf[2] = rrum;

    return (SifCallRpc(&ds34usb, DS34USB_SET_RUMBLE, 0, rpcbuf, 3, NULL, 0, NULL, NULL) == 0);
}

int ds34usb_set_led(int port, u8 led)
{
    if (!ds34usb_inited)
        return 0;

    rpcbuf[0] = port;
    rpcbuf[1] = led;

    return (SifCallRpc(&ds34usb, DS34USB_SET_LED, 0, rpcbuf, 2, NULL, 0, NULL, NULL) == 0);
}

int ds34usb_get_data(int port, u8 *data)
{
    int ret;

    if (!ds34usb_inited)
        return 0;

    rpcbuf[0] = port;

    ret = (SifCallRpc(&ds34usb, DS34USB_GET_DATA, 0, rpcbuf, 1, rpcbuf, 18, NULL, NULL) == 0);

    memcpy(data, rpcbuf, 18);

    return ret;
}

int ds34usb_reset()
{
    if (!ds34usb_inited)
        return 0;

    return (SifCallRpc(&ds34usb, DS34USB_RESET, 0, NULL, 0, NULL, 0, NULL, NULL) == 0);
}
