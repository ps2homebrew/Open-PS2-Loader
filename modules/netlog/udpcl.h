/*
 * udpcl.h - UDP client library
 *
 * Copyright (C) 2009 misfire <misfire@xploderfreax.de>
 *
 * Licensed under the Academic Free License version 2.0.  See file LICENSE.
 */

#ifndef _UDPCL_H_
#define _UDPCL_H_

#include <ps2ip.h>

#define UDPCL_F_NONE	0
#define UDPCL_F_LISTEN	1

/* UDP client context */
typedef struct {
	int socket;
	struct sockaddr_in sockaddr;
} udpcl_t;

int udpcl_create(udpcl_t *cl, const char *ipaddr, int ipport, int flags);
void udpcl_destroy(const udpcl_t *cl);
int udpcl_send(const udpcl_t *cl, const void *buf, int size);
#if 0
int udpcl_receive(const udpcl_t *cl, void *buf, int size);
int udpcl_receive_all(const udpcl_t *cl, void *buf, int size);
int udpcl_wait(const udpcl_t *cl, int timeout);
#endif

#endif /* _UDPCL_H_ */
