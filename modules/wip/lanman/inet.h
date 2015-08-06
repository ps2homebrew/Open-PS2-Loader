/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#ifndef INET_H
#define INET_H

#include <tamtypes.h>

#include "ip.h"

u16 htons(u16);
u16 ntohs(u16);
u32 htonl(u32);
u32 ntohl(u32);
u32 htona(u8 *);
u32 ntoha(u8 *);
u16 inet_chksum(ip_hdr_t *, u16);
u16 inet_chksum_pseudo(void *, u32 *, u32 *, u16);

#endif /* INET_H */
