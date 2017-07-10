#include <kernel.h>
#include <sifrpc.h>
#include <string.h>

#define DS3BT_INIT 1
#define DS3BT_INIT_CHARGING 2
#define DS3BT_GET_STATUS 3
#define DS3BT_GET_BDADDR 4
#define DS3BT_SET_RUMBLE 5
#define DS3BT_SET_LED 6
#define DS3BT_GET_DATA 7
#define DS3BT_RESET 8

#define DS3BT_BIND_RPC_ID 0x18E3878F

static SifRpcClientData_t ds3bt;

static u8 rpcbuf[64] __attribute__((aligned(64)));

static u8 ds3bt_inited = 0;

int ds3bt_init()
{
    ds3bt.server = NULL;

    do {
        if (SifBindRpc(&ds3bt, DS3BT_BIND_RPC_ID, 0) < 0)
            return 0;

        nopdelay();
    } while (!ds3bt.server);

    ds3bt_inited = 1;

    return 1;
}

int ds3bt_deinit()
{
    ds3bt_inited = 0;
    return 1;
}

int ds3bt_reinit_ports(u8 ports)
{
    if (!ds3bt_inited)
        return 0;

    rpcbuf[0] = ports;
    return (SifCallRpc(&ds3bt, DS3BT_INIT, 0, rpcbuf, 1, NULL, 0, NULL, NULL) == 0);
}

int ds3bt_init_charging()
{
    if (!ds3bt_inited)
        return 0;

    return (SifCallRpc(&ds3bt, DS3BT_INIT_CHARGING, 0, NULL, 0, NULL, 0, NULL, NULL) == 0);
}

int ds3bt_get_status(int port)
{
    if (!ds3bt_inited)
        return 0;

    rpcbuf[0] = port;

	if (SifCallRpc(&ds3bt, DS3BT_GET_STATUS, 0, rpcbuf, 1, rpcbuf, 1, NULL, NULL) == 0)
    	return rpcbuf[0];

    return 0;
}

int ds3bt_get_bdaddr(u8 *bdaddr)
{
    int i, ret = 0;

    if (!ds3bt_inited)
        return 0;

    if (SifCallRpc(&ds3bt, DS3BT_GET_BDADDR, 0, NULL, 0, rpcbuf, 7, NULL, NULL) == 0) {
        for (i = 0; i < 6; i++)
            bdaddr[i] = rpcbuf[i];

        ret = rpcbuf[6];
    }

    return ret;
}

int ds3bt_set_rumble(int port, u8 lrum, u8 rrum)
{
    if (!ds3bt_inited)
        return 0;

    rpcbuf[0] = port;
    rpcbuf[1] = lrum;
    rpcbuf[2] = rrum;

    return (SifCallRpc(&ds3bt, DS3BT_SET_RUMBLE, 0, rpcbuf, 3, NULL, 0, NULL, NULL) == 0);
}

int ds3bt_set_led(int port, u8 led)
{
    if (!ds3bt_inited)
        return 0;

    rpcbuf[0] = port;
    rpcbuf[1] = led;

    return (SifCallRpc(&ds3bt, DS3BT_SET_LED, 0, rpcbuf, 2, NULL, 0, NULL, NULL) == 0);
}

int ds3bt_get_data(int port, u8 *data)
{
    int ret;

    if (!ds3bt_inited)
        return 0;

    rpcbuf[0] = port;

    ret = (SifCallRpc(&ds3bt, DS3BT_GET_DATA, 0, rpcbuf, 1, rpcbuf, 18, NULL, NULL) == 0);

    memcpy(data, rpcbuf, 18);

    return ret;
}

int ds3bt_reset()
{
    if (!ds3bt_inited)
        return 0;

    return (SifCallRpc(&ds3bt, DS3BT_RESET, 0, NULL, 0, NULL, 0, NULL, NULL) == 0);
}
