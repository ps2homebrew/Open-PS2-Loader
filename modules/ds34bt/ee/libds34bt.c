#include <kernel.h>
#include <sifrpc.h>
#include <string.h>
#include "libds34bt.h"

#define DS34BT_INIT          1
#define DS34BT_INIT_CHARGING 2
#define DS34BT_GET_STATUS    3
#define DS34BT_GET_BDADDR    4
#define DS34BT_SET_RUMBLE    5
#define DS34BT_SET_LED       6
#define DS34BT_GET_DATA      7
#define DS34BT_RESET         8
#define DS34BT_GET_VERSION   9
#define DS34BT_GET_FEATURES  10

#define DS34BT_BIND_RPC_ID 0x18E3878F

static SifRpcClientData_t ds34bt;

static u8 rpcbuf[64] __attribute__((aligned(64)));

static u8 ds34bt_inited = 0;

int ds34bt_init()
{
    ds34bt.server = NULL;

    do {
        if (SifBindRpc(&ds34bt, DS34BT_BIND_RPC_ID, 0) < 0)
            return 0;

        nopdelay();
    } while (!ds34bt.server);

    ds34bt_inited = 1;

    return 1;
}

int ds34bt_deinit()
{
    ds34bt_inited = 0;
    return 1;
}

int ds34bt_reinit_ports(u8 ports)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = ports;
    return (SifCallRpc(&ds34bt, DS34BT_INIT, 0, rpcbuf, 1, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_init_charging()
{
    if (!ds34bt_inited)
        return 0;

    return (SifCallRpc(&ds34bt, DS34BT_INIT_CHARGING, 0, NULL, 0, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_get_status(int port)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = port;

    if (SifCallRpc(&ds34bt, DS34BT_GET_STATUS, 0, rpcbuf, 1, rpcbuf, 1, NULL, NULL) == 0)
        return rpcbuf[0];

    return 0;
}

int ds34bt_get_bdaddr(u8 *bdaddr)
{
    int ret = 0;

    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_GET_BDADDR, 0, NULL, 0, rpcbuf, 7, NULL, NULL) == 0) {
        memcpy(bdaddr, rpcbuf, 6);

        ret = rpcbuf[6];
    }

    return ret;
}

int ds34bt_set_rumble(int port, u8 lrum, u8 rrum)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = port;
    rpcbuf[1] = lrum;
    rpcbuf[2] = rrum;

    return (SifCallRpc(&ds34bt, DS34BT_SET_RUMBLE, 0, rpcbuf, 3, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_set_led(int port, u8 *led)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = port;
    rpcbuf[1] = led[0];
    rpcbuf[2] = led[1];
    rpcbuf[3] = led[2];
    rpcbuf[4] = 0;

    return (SifCallRpc(&ds34bt, DS34BT_SET_LED, 0, rpcbuf, 5, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_get_data(int port, u8 *data)
{
    int ret;

    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = port;

    ret = (SifCallRpc(&ds34bt, DS34BT_GET_DATA, 0, rpcbuf, 1, rpcbuf, 18, NULL, NULL) == 0);

    memcpy(data, rpcbuf, 18);

    return ret;
}

int ds34bt_reset()
{
    if (!ds34bt_inited)
        return 0;

    return (SifCallRpc(&ds34bt, DS34BT_RESET, 0, NULL, 0, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_get_version(hci_information_t *info)
{
    int ret = 0;

    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_GET_VERSION, 0, NULL, 0, rpcbuf, 1 + sizeof(hci_information_t), NULL, NULL) == 0) {
        memcpy(info, rpcbuf, sizeof(hci_information_t));

        ret = rpcbuf[sizeof(hci_information_t)];
    }

    return ret;
}

int ds34bt_get_features(u8 *info)
{
    int ret = 0;

    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_GET_FEATURES, 0, NULL, 0, rpcbuf, 9, NULL, NULL) == 0) {
        memcpy(info, rpcbuf, 8);

        ret = rpcbuf[8];
    }

    return ret;
}
