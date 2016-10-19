/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#ifndef UDP_H
#define UDP_H

#include <tamtypes.h>

#include "eth.h"
#include "ip.h"

typedef struct
{
    /* Ethernet header (14).  */
    eth_hdr_t eth;

    /* IP header (20).  */
    ip_hdr_t ip;

    /* UDP header (8).  */
    u16 udp_port_src;
    u16 udp_port_dst;
    u16 udp_len;
    u16 udp_csum;

    /* Data goes here.  */
} udp_pkt_t __attribute__((packed));

#endif /* UDP_H */
