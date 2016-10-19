/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#include <tamtypes.h>
#include <stdio.h>
#include <loadcore.h>

#include "lanman.h"
#include "arp.h"
#include "tcp.h"
#include "smap.h"
#include "udptty.h"

#define MODNAME "lanman"
IRX_ID(MODNAME, 1, 1);

static g_param_t g_param = {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // needed for broadcast
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    IP_ADDR(192, 168, 0, 2),
    IP_ADDR(192, 168, 0, 10),
    IP_PORT(445),
    IP_PORT(0x4712)};

//-------------------------------------------------------------------------
int _start(int argc, char *argv[])
{
    // Init SMAP
    if (smap_init(&g_param.eth_addr_src[0]) != 0)
        return MODULE_NO_RESIDENT_END;

    // Does ARP request and wait reply to get server MAC address
    arp_init(&g_param);

#ifdef UDPTTY
    // Init UDP tty
    ttyInit(&g_param);
#endif

    // Init TCPIP layer
    tcp_init(&g_param);

    tcp_connect();

    return MODULE_RESIDENT_END;
}
