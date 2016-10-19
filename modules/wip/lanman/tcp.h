/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#ifndef TCP_H
#define TCP_H

#include <tamtypes.h>

#include "lanman.h"
#include "eth.h"
#include "ip.h"

#define TTL 255
#define TCPWND_SIZE 8192
#define IP_PROTO_TCP 0x06
#define TCP_MSS 1460

typedef struct
{
    // Ethernet (14)
    eth_hdr_t eth;

    // IP header (20)
    ip_hdr_t ip;

    // TCP header (20)
    u16 tcp_port_src;
    u16 tcp_port_dst;
    u8 tcp_seq_num[4];
    u8 tcp_ack_num[4];
    u8 tcp_hdrlen;
    u8 tcp_flags;
    u16 tcp_wndsize;
    u16 tcp_csum;
    u16 tcp_urgp;

    // Data goes here
} tcpip_pkt_t __attribute__((packed));

void tcp_init(g_param_t *gparam);
int tcp_input(void *buf, int size);
void tcp_connect(void);
void tcp_send_ack(void);

#endif /* TCP_H */
