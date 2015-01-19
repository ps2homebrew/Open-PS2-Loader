/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#ifndef SMAP_H
#define SMAP_H

#include <tamtypes.h>

int smap_init(u8 *eth_addr_src);
int smap_xmit(void *buf, int size);

#endif /* SMAP_H */
