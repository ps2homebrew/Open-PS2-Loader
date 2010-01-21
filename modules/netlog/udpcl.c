/*
 * udpcl.c - UDP client library
 *
 * Copyright (C) 2009 misfire <misfire@xploderfreax.de>
 *
 * Licensed under the Academic Free License version 2.0.  See file LICENSE.
 */

#include <ps2ip.h>
#include <sysclib.h>
#include "udpcl.h"

/* Create a new UDP client object. */
int udpcl_create(udpcl_t *cl, const char *ipaddr, int ipport, int flags)
{
	if (cl == NULL || ipaddr == NULL || ipport == 0)
		return -1;

	/* Populate the sockaddr structure */
	cl->sockaddr.sin_family = AF_INET;
	cl->sockaddr.sin_port = htons(ipport);
	cl->sockaddr.sin_addr.s_addr = inet_addr(ipaddr);

	/* Create a socket */
	cl->socket = socket(AF_INET, SOCK_DGRAM, 0);

	/* Bind the socket if we want to listen */
	if (flags & UDPCL_F_LISTEN && cl->socket > 0) {
		if (bind(cl->socket, (struct sockaddr*)&cl->sockaddr, sizeof(struct sockaddr)) < 0)
			return -1;
	}

	return cl->socket;
}

/* Destroy an UDP client object. */
void udpcl_destroy(const udpcl_t *cl)
{
	if (cl != NULL) {
		if (cl->socket > 0)
			disconnect(cl->socket);
		memset((void*)cl, 0, sizeof(udpcl_t));
	}
}

/* Send an UDP datagram to a remote host. */
int udpcl_send(const udpcl_t *cl, const void *buf, int size)
{
	int total = 0, ret = 1;

	/* Keep sending data until it has all been sent */
	while ((size > 0) && (ret > 0)) {
		ret = sendto(cl->socket, (void*)(buf + total), size, 0,
			(struct sockaddr*)&cl->sockaddr, sizeof(struct sockaddr));
		if (ret > 0) {
			total += ret;
			size -= ret;
		}
	}

	/* Return the total bytes sent */
	return total;
}

#if 0
/* Return an UDP datagram that was sent by a remote host. */
int udpcl_receive(const udpcl_t *cl, void *buf, int size)
{
	return recvfrom(cl->socket, buf, size, 0, NULL, NULL);
}

int udpcl_receive_all(const udpcl_t *cl, void *buf, int size)
{
	int total = 0, ret = 1;

	/* Receive the data from the socket */
	while ((size > 0) && (ret > 0)) {
		/* We don't care about the source address */
		ret = recvfrom(cl->socket, (void*)(buf + total), size, 0, NULL, NULL);
		if (ret > 0) {
			total += ret;
			size -= ret;
		}
	}

	/* Return the total bytes received */
	return total;
}

int udpcl_wait(const udpcl_t *cl, int timeout)
{
	fd_set nfds;
	struct timeval tv;

	/* Initialize the rdfs structure */
	FD_ZERO(&nfds);
	FD_SET(cl->socket, &nfds);

	/* Populate the tv structure */
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	/*
	 * If no timeout was specified, wait forever;
	 * otherwise, wait until the time has elapsed.
	 */
	return select(FD_SETSIZE, &nfds, NULL, NULL, (timeout < 0) ? NULL : &tv);
}
#endif
