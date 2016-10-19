/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#ifndef ETH_H
#define ETH_H

#include <tamtypes.h>

typedef struct
{
    // Ethernet header (14)
    u8 addr_dst[6];
    u8 addr_src[6];
    u16 type;
} eth_hdr_t;

#endif /* ETH_H */
