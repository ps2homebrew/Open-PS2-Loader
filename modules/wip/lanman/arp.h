/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#ifndef ARP_H
#define ARP_H

#include <tamtypes.h>

#include "lanman.h"
#include "eth.h"
#include "ip.h"

#define ARP_REQUEST 0x01
#define ARP_REPLY 0x02

typedef struct
{
    // Ethernet (14)
    eth_hdr_t eth;

    // ARP (28)
    u16 arp_hwtype;
    u16 arp_protocoltype;
    u8 arp_hwsize;
    u8 arp_protocolsize;
    u16 arp_opcode;
    u8 arp_sender_eth_addr[6];
    ip_addr_t arp_sender_ip_addr;
    u8 arp_target_eth_addr[6];
    ip_addr_t arp_target_ip_addr;
} arp_pkt_t __attribute__((packed));

void arp_init(g_param_t *gparam);
void arp_input(void *buf, int size);
void arp_request(void);

#endif /* ARP_H */
