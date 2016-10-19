/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#ifndef IP_H
#define IP_H

#include <tamtypes.h>

typedef struct
{
    u8 addr[4];
} ip_addr_t __attribute__((packed));

typedef struct
{
    // IP header (20)
    u8 hlen;
    u8 tos;
    u16 len;
    u16 id;
    u8 flags;
    u8 frag_offset;
    u8 ttl;
    u8 proto;
    u16 csum;
    ip_addr_t addr_src;
    ip_addr_t addr_dst;
} ip_hdr_t;

#endif /* IP_H */
