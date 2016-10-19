/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#ifndef LANMAN_H
#define LANMAN_H

#include <tamtypes.h>

/* These automatically convert the address and port to network order.  */
#define IP_ADDR(a, b, c, d) (((d & 0xff) << 24) | ((c & 0xff) << 16) | \
                             ((b & 0xff) << 8) | ((a & 0xff)))
#define IP_PORT(port) (((port & 0xff00) >> 8) | ((port & 0xff) << 8))

typedef struct
{
    u8 eth_addr_dst[6];
    u8 eth_addr_src[6];
    u32 ip_addr_dst;
    u32 ip_addr_src;
    u16 ip_port_dst;
    u16 ip_port_src;
} g_param_t;

#endif /* LANMAN_H */
