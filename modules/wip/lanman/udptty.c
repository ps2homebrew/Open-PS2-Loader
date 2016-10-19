/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003  Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: tty.c 629 2004-10-11 00:45:00Z mrbrown $
# TTY filesystem for UDPTTY.
*/

#include <tamtypes.h>
#include <stdio.h>
#include <thsemap.h>
#include <ioman.h>
#include <errno.h>

#include "lanman.h"
#include "smsutils.h"
#include "smap.h"
#include "udp.h"
#include "inet.h"

#define DEVNAME "tty"

#ifdef UDPTTY

static g_param_t *g_param;
static int tty_sema = -1;

/* The max we'll send is a MTU, 1514 bytes.  */
static u8 pktbuf[1514];

static int tty_init(iop_device_t *device);
static int tty_deinit(iop_device_t *device);
static int tty_stdout_fd(void);
static int tty_write(iop_file_t *file, void *buf, size_t size);
static int tty_error(void);

/* device ops */
static iop_device_ops_t tty_ops = {
    tty_init,
    tty_deinit,
    (void *)tty_error,
    (void *)tty_stdout_fd,
    (void *)tty_stdout_fd,
    (void *)tty_error,
    (void *)tty_write,
    (void *)tty_error,
    (void *)tty_error,
    (void *)tty_error,
    (void *)tty_error,
    (void *)tty_error,
    (void *)tty_error,
    (void *)tty_error,
    (void *)tty_error,
    (void *)tty_error,
    (void *)tty_error};

/* device descriptor */
static iop_device_t tty_device = {
    DEVNAME,
    IOP_DT_CHAR | IOP_DT_CONS,
    1,
    "TTY via SMAP UDP",
    &tty_ops};

/* Init TTY */
void ttyInit(g_param_t *gparam)
{
    g_param = gparam;

    close(0);
    close(1);
    DelDrv(DEVNAME);

    if (AddDrv(&tty_device) < 0)
        return;

    open(DEVNAME "00:", 0x1000 | O_RDWR);
    open(DEVNAME "00:", O_WRONLY);
}

/* UDP */

static int udp_init(void)
{
    udp_pkt_t *udp_pkt;

    /* Initialize the static elements of our UDP packet.  */
    udp_pkt = (udp_pkt_t *)pktbuf;

    mips_memcpy(udp_pkt->eth.addr_dst, g_param->eth_addr_dst, 12);
    udp_pkt->eth.type = 0x0008; /* Network byte order: 0x800 */

    udp_pkt->ip.hlen = 0x45;
    udp_pkt->ip.tos = 0;
    udp_pkt->ip.id = 0;
    udp_pkt->ip.flags = 0;
    udp_pkt->ip.frag_offset = 0;
    udp_pkt->ip.ttl = 64;
    udp_pkt->ip.proto = 0x11;
    mips_memcpy(&udp_pkt->ip.addr_src.addr, &g_param->ip_addr_src, 4);
    mips_memcpy(&udp_pkt->ip.addr_dst.addr, &g_param->ip_addr_dst, 4);

    udp_pkt->udp_port_src = g_param->ip_port_src;
    udp_pkt->udp_port_dst = IP_PORT(0x4712);

    return 0;
}

/* Copy the data into place, calculate the various checksums, and send the
   final packet.  */
static int udp_send(void *buf, size_t size)
{
    udp_pkt_t *udp_pkt;
    size_t pktsize, udpsize;

    if ((size + sizeof(udp_pkt_t)) > sizeof(pktbuf))
        size = sizeof(pktbuf) - sizeof(udp_pkt_t);

    udp_pkt = (udp_pkt_t *)pktbuf;
    pktsize = size + sizeof(udp_pkt_t);

    udp_pkt->ip.len = htons(pktsize - 14); /* Subtract the ethernet header.  */

    udp_pkt->ip.csum = 0;
    udp_pkt->ip.csum = inet_chksum(&udp_pkt->ip, 20); /* Checksum the IP header (20 bytes).  */

    udpsize = htons(size + 8); /* Size of the UDP header + data.  */
    udp_pkt->udp_len = udpsize;
    mips_memcpy(pktbuf + sizeof(udp_pkt_t), buf, size);

    udp_pkt->udp_csum = 0; /* Don't bother.  */

    while (smap_xmit(udp_pkt, pktsize) != 0)
        ;

    return 0;
}


/* TTY driver.  */

static int tty_init(iop_device_t *device)
{
    int res;

    if ((res = udp_init()) < 0)
        return res;

    if ((tty_sema = CreateMutex(IOP_MUTEX_UNLOCKED)) < 0)
        return -1;

    return 0;
}

static int tty_deinit(iop_device_t *device)
{
    DeleteSema(tty_sema);
    return 0;
}

static int tty_stdout_fd(void)
{
    return 1;
}

static int tty_write(iop_file_t *file, void *buf, size_t size)
{
    int res = 0;

    WaitSema(tty_sema);
    res = udp_send(buf, size);

    SignalSema(tty_sema);
    return res;
}

static int tty_error(void)
{
    return -EIO;
}

#endif
