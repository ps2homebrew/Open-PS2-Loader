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



/* ip.c
 *
 * This is the code for the IP layer.
 *
 */


#include "lwip/opt.h"


#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/ip.h"
#include "lwip/ip_frag.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/icmp.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"

#include "lwip/stats.h"

#include "lwip/snmp.h"
#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif /* LWIP_DHCP */


/* ip_init:
 *
 * Initializes the IP layer.
 */

void ip_init(void)
{
}

/* ip_lookup:
 *
 * An experimental feature that will be changed in future versions. Do
 * not depend on it yet...
 */

#ifdef LWIP_DEBUG
u8_t ip_lookup(void *header, struct netif *inp)
{
    struct ip_hdr *iphdr;

    iphdr = header;

    /* not IP v4? */
    if (IPH_V(iphdr) != 4) {
        return 0;
    }

#if IP_OPTIONS == 0
    if (IPH_HL(iphdr) != 5) {
        return 0;
    }
#endif /* IP_OPTIONS == 0 */

    switch (IPH_PROTO(iphdr)) {
#if LWIP_UDP
        case IP_PROTO_UDP:
        case IP_PROTO_UDPLITE:
            return udp_lookup(iphdr, inp);
#endif /* LWIP_UDP */
#if LWIP_TCP
        case IP_PROTO_TCP:
            return 1;
#endif /* LWIP_TCP */
        case IP_PROTO_ICMP:
            return 1;
        default:
            return 0;
    }
}
#endif /* LWIP_DEBUG */

/* ip_route:
 *
 * Finds the appropriate network interface for a given IP address. It
 * searches the list of network interfaces linearly. A match is found
 * if the masked IP address of the network interface equals the masked
 * IP address given to the function.
 */

struct netif *
ip_route(struct ip_addr *dest)
{
    struct netif *netif;

    /* iterate through netifs */
    for (netif = netif_list; netif != NULL; netif = netif->next) {
        /* network mask matches? */
        if (ip_addr_maskcmp(dest, &(netif->ip_addr), &(netif->netmask))) {
            /* return netif on which to forward IP packet */
            return netif;
        }
    }
    /* no matching netif found, use default netif */
    return netif_default;
}
#if IP_FORWARD

/* ip_forward:
 *
 * Forwards an IP packet. It finds an appropriate route for the
 * packet, decrements the TTL value of the packet, adjusts the
 * checksum and outputs the packet on the appropriate interface.
 */

static void
ip_forward(struct pbuf *p, struct ip_hdr *iphdr, struct netif *inp)
{
    struct netif *netif;

    PERF_START;
    /* Find network interface where to forward this IP packet to. */
    netif = ip_route((struct ip_addr *)&(iphdr->dest));
    if (netif == NULL) {
        LWIP_DEBUGF(IP_DEBUG, ("ip_forward: no forwarding route for 0x%lx found\n",
                               iphdr->dest.addr));
        snmp_inc_ipnoroutes();
        return;
    }
    /* Do not forward packets onto the same network interface on which
     they arrived. */
    if (netif == inp) {
        LWIP_DEBUGF(IP_DEBUG, ("ip_forward: not bouncing packets back on incoming interface.\n"));
        snmp_inc_ipnoroutes();
        return;
    }

    /* decrement TTL */
    IPH_TTL_SET(iphdr, IPH_TTL(iphdr) - 1);
    /* send ICMP if TTL == 0 */
    if (IPH_TTL(iphdr) == 0) {
        /* Don't send ICMP messages in response to ICMP messages */
        if (IPH_PROTO(iphdr) != IP_PROTO_ICMP) {
            icmp_time_exceeded(p, ICMP_TE_TTL);
            snmp_inc_icmpouttimeexcds();
        }
        return;
    }
    /* Incrementally update the IP checksum. */
    if (IPH_CHKSUM(iphdr) >= htons(0xffff - 0x100)) {
        IPH_CHKSUM_SET(iphdr, IPH_CHKSUM(iphdr) + 2);
    } else {
        IPH_CHKSUM_SET(iphdr, IPH_CHKSUM(iphdr) + 1);
    }

    LWIP_DEBUGF(IP_DEBUG, ("ip_forward: forwarding packet to 0x%lx\n",
                           iphdr->dest.addr));

    IP_STATS_INC(ip.fw);
    IP_STATS_INC(ip.xmit);
    snmp_inc_ipforwdatagrams();

    PERF_STOP("ip_forward");
    /* transmit pbuf on chosen interface */
    netif->output(netif, p, (struct ip_addr *)&(iphdr->dest));
}
#endif /* IP_FORWARD */

/* ip_input:
 *
 * This function is called by the network interface device driver when
 * an IP packet is received. The function does the basic checks of the
 * IP header such as packet size being at least larger than the header
 * size etc. If the packet was not destined for us, the packet is
 * forwarded (using ip_forward). The IP checksum is always checked.
 *
 * Finally, the packet is sent to the upper layer protocol input function.
 */
err_t ip_input(struct pbuf *p, struct netif *inp)
{

    static struct ip_hdr *iphdr;
    static struct netif *netif;
    static u16_t iphdrlen;

    iphdr = p->payload;

    if (IPH_V(iphdr) != 4) {
        pbuf_free(p);
        return ERR_OK;
    } /* end if */

    iphdrlen = IPH_HL(iphdr);
    iphdrlen *= 4;

    if (iphdrlen > p->len) {
        pbuf_free(p);
        return ERR_OK;
    } /* end if */

    if (inet_chksum(iphdr, iphdrlen)) {
        pbuf_free(p);
        return ERR_OK;
    } /* end if */

    pbuf_realloc(p, ntohs(IPH_LEN(iphdr)));

    for (netif = netif_list; netif; netif = netif->next)

        if (!ip_addr_isany(&netif->ip_addr)) {

            if (ip_addr_cmp(&iphdr->dest, &netif->ip_addr) || (ip_addr_isbroadcast(&iphdr->dest, &netif->netmask) &&
                                                               ip_addr_maskcmp(&iphdr->dest, &netif->ip_addr, &netif->netmask)) ||
                ip_addr_cmp(&iphdr->dest, IP_ADDR_BROADCAST))
                break;

        } /* end if */
#if LWIP_DHCP
    if (!netif) {
        if (IPH_PROTO(iphdr) == IP_PROTO_UDP) {
            if (ntohs(
                    ((struct udp_hdr *)((u8_t *)iphdr + iphdrlen))->dest) == DHCP_CLIENT_PORT)
                netif = inp;
        } /* end if */
    }     /* end if */
#endif    /* LWIP_DHCP */
    if (!netif) {
#if IP_FORWARD
        if (!ip_addr_isbroadcast(&iphdr->dest, &inp->netmask))
            ip_forward(p, iphdr, inp);
#endif /* IP_FORWARD */
        pbuf_free(p);
        return ERR_OK;
    } /* end if */
#if IP_REASSEMBLY
    if ((IPH_OFFSET(iphdr) & 0xFF3F) != 0) {
        p = ip_reass(p);
        if (p == NULL)
            return ERR_OK;
        iphdr = p->payload;
    }  /* end if */
#else  /* IP_REASSEMBLY */
    if ((IPH_OFFSET(iphdr) & 0xFF3F) != 0) {
        pbuf_free(p);
        return ERR_OK;
    } /* end if */
#endif /* IP_REASSEMBLY */
#if IP_OPTIONS == 0
    if (iphdrlen > IP_HLEN) {
        LWIP_DEBUGF(IP_DEBUG | 2, ("IP packet dropped since there were IP options (while IP_OPTIONS == 0).\n"));
        pbuf_free(p);
        IP_STATS_INC(ip.opterr);
        IP_STATS_INC(ip.drop);
        snmp_inc_ipunknownprotos();
        return ERR_OK;
    }
#endif /* IP_OPTIONS == 0 */
#if LWIP_RAW
    if (!raw_input(p, inp)) {
#endif /* LWIP_RAW */

        switch (IPH_PROTO(iphdr)) {
#if LWIP_UDP
            case IP_PROTO_UDP:
            case IP_PROTO_UDPLITE:
                snmp_inc_ipindelivers();
                udp_input(p, inp);
                break;
#endif /* LWIP_UDP */
#if LWIP_TCP
            case IP_PROTO_TCP:
                snmp_inc_ipindelivers();
                tcp_input(p, inp);
                break;
#endif /* LWIP_TCP */
            case IP_PROTO_ICMP:
                snmp_inc_ipindelivers();
                icmp_input(p, inp);
                break;
            default:
                /* send ICMP destination protocol unreachable unless is was a broadcast */
                if (!ip_addr_isbroadcast(&(iphdr->dest), &(inp->netmask)) &&
                    !ip_addr_ismulticast(&(iphdr->dest))) {
                    p->payload = iphdr;
                    icmp_dest_unreach(p, ICMP_DUR_PROTO);
                }
                pbuf_free(p);
        }
#if LWIP_RAW
    } /* LWIP_RAW */
#endif
    return ERR_OK;
}


/* ip_output_if:
 *
 * Sends an IP packet on a network interface. This function constructs
 * the IP header and calculates the IP header checksum. If the source
 * IP address is NULL, the IP address of the outgoing network
 * interface is filled in as source address.
 */

err_t ip_output_if(struct pbuf *p, struct ip_addr *src, struct ip_addr *dest,
                   u8_t ttl, u8_t tos,
                   u8_t proto, struct netif *netif)
{
    static struct ip_hdr *iphdr;
    static u16_t ip_id = 0;

    snmp_inc_ipoutrequests();

    if (dest != IP_HDRINCL) {
        if (pbuf_header(p, IP_HLEN)) {
            LWIP_DEBUGF(IP_DEBUG | 2, ("ip_output: not enough room for IP header in pbuf\n"));

            IP_STATS_INC(ip.err);
            snmp_inc_ipoutdiscards();
            return ERR_BUF;
        }

        iphdr = p->payload;

        IPH_TTL_SET(iphdr, ttl);
        IPH_PROTO_SET(iphdr, proto);

        ip_addr_set(&(iphdr->dest), dest);

        IPH_VHLTOS_SET(iphdr, 4, IP_HLEN / 4, tos);
        IPH_LEN_SET(iphdr, htons(p->tot_len));
        IPH_OFFSET_SET(iphdr, 0x0040);
        IPH_ID_SET(iphdr, htons(ip_id));
        ++ip_id;

        if (ip_addr_isany(src)) {
            ip_addr_set(&(iphdr->src), &(netif->ip_addr));
        } else {
            ip_addr_set(&(iphdr->src), src);
        }

        IPH_CHKSUM_SET(iphdr, 0);
        IPH_CHKSUM_SET(iphdr, inet_chksum(iphdr, IP_HLEN));
    } else {
        iphdr = p->payload;
        dest = &(iphdr->dest);
    }

#if IP_FRAG
    /* don't fragment if interface has mtu set to 0 [loopif] */
    if (netif->mtu && (p->tot_len > netif->mtu))
        return ip_frag(p, netif, dest);
#endif
    return netif->output(netif, p, dest);
}
/* ip_output:
 *
 * Simple interface to ip_output_if. It finds the outgoing network
 * interface and calls upon ip_output_if to do the actual work.
 */
err_t ip_output(struct pbuf *p, struct ip_addr *src, struct ip_addr *dest, u8_t ttl, u8_t tos, u8_t proto)
{

    struct netif *netif;

    if (!(netif = ip_route(dest)))
        return ERR_RTE;

    return ip_output_if(p, src, dest, ttl, tos, proto, netif);

} /* end ip_output */
