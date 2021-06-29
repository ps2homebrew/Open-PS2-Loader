/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
*/

#ifndef _TCPIP_H
#define _TCPIP_H

/* Some portions of this header fall under the following copyright.  The license
   is compatible with that of ps2sdk.  */

/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include <sys/time.h>

typedef signed char err_t; /* lwIP error type.  */

/* From src/include/lwip/pbuf.h:  */

#define PBUF_TRANSPORT_HLEN 20
#define PBUF_IP_HLEN        20

typedef enum {
    PBUF_TRANSPORT,
    PBUF_IP,
    PBUF_LINK,
    PBUF_RAW
} pbuf_layer;

typedef enum {
    PBUF_RAM,
    PBUF_ROM,
    PBUF_REF,
    PBUF_POOL
} pbuf_flag;

/* Definitions for the pbuf flag field (these are not the flags that
   are passed to pbuf_alloc()). */
#define PBUF_FLAG_RAM  0x00 /* Flags that pbuf data is stored in RAM */
#define PBUF_FLAG_ROM  0x01 /* Flags that pbuf data is stored in ROM */
#define PBUF_FLAG_POOL 0x02 /* Flags that the pbuf comes from the pbuf pool */
#define PBUF_FLAG_REF  0x04 /* Flags thet the pbuf payload refers to RAM */

struct pbuf
{
    /** next pbuf in singly linked pbuf chain */
    struct pbuf *next;

    /** pointer to the actual data in the buffer */
    void *payload;

    /**
     * total length of this buffer and all next buffers in chain
     * invariant: p->tot_len == p->len + (p->next? p->next->tot_len: 0)
     */
    u16 tot_len;

    /* length of this buffer */
    u16 len;

    /* flags telling the type of pbuf */
    u16 flags;

    /**
     * the reference count always equals the number of pointers
     * that refer to this pbuf. This can be pointers from an application,
     * the stack itself, or pbuf->next pointers from a chain.
     */
    u16 ref;
};

/* From include/ipv4/lwip/ip_addr.h:  */

struct ip_addr
{
    u32 addr;
};

#define IP4_ADDR(ipaddr, a, b, c, d) (ipaddr)->addr = htonl(((u32)(a & 0xff) << 24) | ((u32)(b & 0xff) << 16) | \
                                                            ((u32)(c & 0xff) << 8) | (u32)(d & 0xff))

#define ip_addr_set(dest, src)              (dest)->addr = ((struct ip_addr *)src)->addr
#define ip_addr_maskcmp(addr1, addr2, mask) (((addr1)->addr &  \
                                              (mask)->addr) == \
                                             ((addr2)->addr &  \
                                              (mask)->addr))
#define ip_addr_cmp(addr1, addr2) ((addr1)->addr == (addr2)->addr)

#define ip_addr_isany(addr1) ((addr1) == NULL || (addr1)->addr == 0)

#define ip_addr_isbroadcast(addr1, mask) (((((addr1)->addr) & ~((mask)->addr)) == \
                                           (0xffffffff & ~((mask)->addr))) ||     \
                                          ((addr1)->addr == 0xffffffff) ||        \
                                          ((addr1)->addr == 0x00000000))

#define ip_addr_ismulticast(addr1) (((addr1)->addr & ntohl(0xf0000000)) == ntohl(0xe0000000))

#define ip4_addr1(ipaddr) ((u8)(ntohl((ipaddr)->addr) >> 24) & 0xff)
#define ip4_addr2(ipaddr) ((u8)(ntohl((ipaddr)->addr) >> 16) & 0xff)
#define ip4_addr3(ipaddr) ((u8)(ntohl((ipaddr)->addr) >> 8) & 0xff)
#define ip4_addr4(ipaddr) ((u8)(ntohl((ipaddr)->addr)) & 0xff)


/* From include/lwip/netif.h:  */

/** must be the maximum of all used hardware address lengths
    across all types of interfaces in use */
#define NETIF_MAX_HWADDR_LEN 6U

/** TODO: define the use (where, when, whom) of netif flags */

/** whether the network interface is 'up'. this is
 * a software flag used to control whether this network
 * interface is enabled and processes traffic */
#define NETIF_FLAG_UP           0x1U
/** if set, the netif has broadcast capability */
#define NETIF_FLAG_BROADCAST    0x2U
/** if set, the netif is one end of a point-to-point connection */
#define NETIF_FLAG_POINTTOPOINT 0x4U
/** if set, the interface is configured using DHCP */
#define NETIF_FLAG_DHCP         0x08U
/** if set, the interface has an active link
 *  (set by the interface) */
#define NETIF_FLAG_LINK_UP      0x10U

/** generic data structure used for all lwIP network interfaces */
struct netif
{
    /** pointer to next in linked list */
    struct netif *next;
    /** The following fields should be filled in by the
      initialization function for the device driver. */

    /** IP address configuration in network byte order */
    struct ip_addr ip_addr;
    struct ip_addr netmask;
    struct ip_addr gw;

    /** This function is called by the network device driver
      to pass a packet up the TCP/IP stack. */
    err_t (*input)(struct pbuf *p, struct netif *inp);
    /** This function is called by the IP module when it wants
      to send a packet on the interface. This function typically
      first resolves the hardware address, then sends the packet. */
    err_t (*output)(struct netif *netif, struct pbuf *p,
                    struct ip_addr *ipaddr);
    /** This function is called by the ARP module when it wants
      to send a packet on the interface. This function outputs
      the pbuf as-is on the link medium. */
    err_t (*linkoutput)(struct netif *netif, struct pbuf *p);
    /** This field can be set by the device driver and could point
      to state information for the device. */
    void *state;
#if LWIP_DHCP
    /** the DHCP client state information for this netif */
    struct dhcp *dhcp;
#endif
    /** number of bytes used in hwaddr */
    unsigned char hwaddr_len;
    /** link level hardware address of this interface */
    unsigned char hwaddr[NETIF_MAX_HWADDR_LEN];
    /** maximum transfer unit (in bytes) */
    u16 mtu;
    /** descriptive abbreviation */
    char name[2];
    /** number of this interface */
    u8 num;
    /** NETIF_FLAG_* */
    u8 flags;
};

/* From include/lwip/sockets.h:  */

struct in_addr
{
    u32 s_addr;
};

struct sockaddr_in
{
    u8 sin_len;
    u8 sin_family;
    u16 sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct sockaddr
{
    u8 sa_len;
    u8 sa_family;
    char sa_data[14];
};

#ifndef socklen_t
#define socklen_t int
#endif

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

/*
 * Option flags per-socket.
 */
#define SO_DEBUG       0x0001 /* turn on debugging info recording */
#define SO_ACCEPTCONN  0x0002 /* socket has had listen() */
#define SO_REUSEADDR   0x0004 /* allow local address reuse */
#define SO_KEEPALIVE   0x0008 /* keep connections alive */
#define SO_DONTROUTE   0x0010 /* just use interface addresses */
#define SO_BROADCAST   0x0020 /* permit sending of broadcast msgs */
#define SO_USELOOPBACK 0x0040 /* bypass hardware when possible */
#define SO_LINGER      0x0080 /* linger on close if data present */
#define SO_OOBINLINE   0x0100 /* leave received OOB data in line */

#define SO_DONTLINGER (int)(~SO_LINGER)

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF   0x1001 /* send buffer size */
#define SO_RCVBUF   0x1002 /* receive buffer size */
#define SO_SNDLOWAT 0x1003 /* send low-water mark */
#define SO_RCVLOWAT 0x1004 /* receive low-water mark */
#define SO_SNDTIMEO 0x1005 /* send timeout */
#define SO_RCVTIMEO 0x1006 /* receive timeout */
#define SO_ERROR    0x1007 /* get error status and clear */
#define SO_TYPE     0x1008 /* get socket type */

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define SOL_SOCKET 0xfff /* options for socket level */

#define AF_UNSPEC 0
#define AF_INET   2
#define PF_INET   AF_INET

#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

#define INADDR_ANY       0
#define INADDR_BROADCAST 0xffffffff

/* Flags we can use with send and recv. */
#define MSG_DONTWAIT 0x40 /* Nonblocking i/o for this operation only */

#ifndef FD_SET
#undef FD_SETSIZE
#define FD_SETSIZE     16
#define FD_SET(n, p)   ((p)->fd_bits[(n) / 8] |= (1 << ((n)&7)))
#define FD_CLR(n, p)   ((p)->fd_bits[(n) / 8] &= ~(1 << ((n)&7)))
#define FD_ISSET(n, p) ((p)->fd_bits[(n) / 8] & (1 << ((n)&7)))
#define FD_ZERO(p)     memset((void *)(p), 0, sizeof(*(p)))

typedef struct fd_set
{
    unsigned char fd_bits[(FD_SETSIZE + 7) / 8];
} fd_set;
#endif

#if !defined(INVALID_SOCKET)
#define INVALID_SOCKET -1
#endif

#if !defined(htonl)
#define htonl(x) ((((x)&0xff) << 24) | (((x)&0xff00) << 8) | (((x)&0xff0000) >> 8) | (((x)&0xff000000) >> 24))
#endif

#if !defined(htons)
#define htons(x) ((((x)&0xff) << 8) | (((x)&0xff00) >> 8))
#endif

#if !defined(ntohl)
#define ntohl(x) htonl(x)
#endif

#if !defined(ntohs)
#define ntohs(x) htons(x)
#endif

/*
 * Commands for ioctlsocket(),  taken from the BSD file fcntl.h.
 *
 *
 * Ioctl's have the command encoded in the lower word,
 * and the size of any in or out parameters in the upper
 * word.  The high 2 bits of the upper word are used
 * to encode the in/out status of the parameter; for now
 * we restrict parameters to at most 128 bytes.
 */

#if !defined(FIONREAD) || !defined(FIONBIO)
#define IOCPARM_MASK 0x7f       /* parameters must be < 128 bytes */
#define IOC_VOID     0x20000000 /* no parameters */
#define IOC_OUT      0x40000000 /* copy out parameters */
#define IOC_IN       0x80000000 /* copy in parameters */
#define IOC_INOUT    (IOC_IN | IOC_OUT)
/* 0x20000000 distinguishes new &
                                           old ioctl's */
#define _IO(x, y)    (IOC_VOID | ((x) << 8) | (y))

#define _IOR(x, y, t) (IOC_OUT | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))

#define _IOW(x, y, t) (IOC_IN | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#endif

#ifndef FIONREAD
#define FIONREAD _IOR('f', 127, unsigned int) /* get # bytes to read */
#endif

#ifndef FIONBIO
#define FIONBIO _IOW('f', 126, unsigned int) /* set/clear non-blocking i/o */
#endif

typedef struct
{
    char netif_name[4];
    struct in_addr ipaddr;
    struct in_addr netmask;
    struct in_addr gw;
    struct in_addr dns_server;
    u32 dhcp_enabled;
    u32 dhcp_status;
    u8 hw_addr[8];
} t_ip_info;

#ifndef FAR
#define FAR
#endif
struct hostent
{
    char FAR *h_name;           /* official name of host */
    char FAR *FAR *h_aliases;   /* alias list */
    short h_addrtype;           /* host address type */
    short h_length;             /* length of address */
    char FAR *FAR *h_addr_list; /* list of addresses */
#define h_addr h_addr_list[0]   /* address, for backward compat */
};

#if !defined(INADDR_NONE)
#define INADDR_NONE ((u32)0xffffffff) /* 255.255.255.255 */
#endif

/* From include/lwip/sockets.h:  */
/*
 * User-settable options (used with setsockopt, IPPROTO_TCP level).
 */
#define TCP_NODELAY   0x01 /* don't delay send to coalesce packets */
#define TCP_KEEPALIVE 0x02 /* send KEEPALIVE probes when idle for pcb->keepalive miliseconds */

#endif
