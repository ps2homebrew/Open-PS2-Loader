/**
 * @file
 * Address Resolution Protocol module for IP over Ethernet
 *
 * Functionally, ARP is divided into two parts. The first maps an IP address
 * to a physical address when sending a packet, and the second part answers
 * requests from other machines for our physical address.
 *
 * This implementation complies with RFC 826 (Ethernet ARP) and supports
 * Gratuitious ARP from RFC3220 (IP Mobility Support for IPv4) section 4.6.
 */

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

/**
 * TODO:
 * - pbufs should be sent from the queue once an ARP entry state
 *   goes from PENDING to STABLE.
 * - Non-PENDING entries MUST NOT have queued packets.
 */

/*
 * TODO:
 *
RFC 3220 4.6          IP Mobility Support for IPv4          January 2002

      -  A Gratuitous ARP [45] is an ARP packet sent by a node in order
         to spontaneously cause other nodes to update an entry in their
         ARP cache.  A gratuitous ARP MAY use either an ARP Request or
         an ARP Reply packet.  In either case, the ARP Sender Protocol
         Address and ARP Target Protocol Address are both set to the IP
         address of the cache entry to be updated, and the ARP Sender
         Hardware Address is set to the link-layer address to which this
         cache entry should be updated.  When using an ARP Reply packet,
         the Target Hardware Address is also set to the link-layer
         address to which this cache entry should be updated (this field
         is not used in an ARP Request packet).

         In either case, for a gratuitous ARP, the ARP packet MUST be
         transmitted as a local broadcast packet on the local link.  As
         specified in [36], any node receiving any ARP packet (Request
         or Reply) MUST update its local ARP cache with the Sender
         Protocol and Hardware Addresses in the ARP packet, if the
         receiving node has an entry for that IP address already in its
         ARP cache.  This requirement in the ARP protocol applies even
         for ARP Request packets, and for ARP Reply packets that do not
         match any ARP Request transmitted by the receiving node [36].
*
  My suggestion would be to send a ARP request for our newly obtained
  address upon configuration of an Ethernet interface.

*/

#include "lwip/opt.h"
#include "lwip/inet.h"
#include "netif/etharp.h"
#include "lwip/ip.h"
#include "lwip/stats.h"

/* ARP needs to inform DHCP of any ARP replies? */
#if (LWIP_DHCP && DHCP_DOES_ARP_CHECK)
#  include "lwip/dhcp.h"
#endif

/** the time an ARP entry stays valid after its last update, (120 * 10) seconds = 20 minutes. */
#define ARP_MAXAGE 120
/** the time an ARP entry stays pending after first request, (2 * 10) seconds = 20 seconds. */
#define ARP_MAXPENDING 2

#define HWTYPE_ETHERNET 1

/** ARP message types */
#define ARP_REQUEST 0x0100
#define ARP_REPLY   0x0200

#define ARPH_HWLEN(hdr) (ntohs((hdr)->_hwlen_protolen) >> 8)
#define ARPH_PROTOLEN(hdr) (ntohs((hdr)->_hwlen_protolen) & 0xff)

#define ARPH_HWLEN_SET(hdr, len) (hdr)->_hwlen_protolen = htons(ARPH_PROTOLEN(hdr) | ((len) << 8))
#define ARPH_PROTOLEN_SET(hdr, len) (hdr)->_hwlen_protolen = htons((len) | (ARPH_HWLEN(hdr) << 8))

enum etharp_state {
  ETHARP_STATE_EMPTY,
  ETHARP_STATE_PENDING,
  ETHARP_STATE_STABLE
};

struct etharp_entry {
  struct ip_addr ipaddr;
  struct eth_addr ethaddr;
  enum etharp_state state;
#if ARP_QUEUEING
  /** 
   * Pointer to queue of pending outgoing packets on this ARP entry.
   * Must be at most a single packet for now. */
  struct pbuf *p;
#endif
  u8_t ctime;
};

static const struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};
static struct etharp_entry arp_table[ARP_TABLE_SIZE];

static s8_t find_arp_entry(void);
#define ARP_INSERT_FLAG 1
static struct pbuf *update_arp_entry(struct netif *netif, struct ip_addr *ipaddr, struct eth_addr *ethaddr, u8_t flags);
#if ARP_QUEUEING
static struct pbuf *etharp_enqueue(s8_t i, struct pbuf *q);
static u8_t etharp_dequeue(s8_t i);
#endif
/**
 * Initializes ARP module.
 */
void
etharp_init(void)
{
  s8_t i;
  /* clear ARP entries */
  for(i = 0; i < ARP_TABLE_SIZE; ++i) {
    arp_table[i].state = ETHARP_STATE_EMPTY;
#if ARP_QUEUEING
    arp_table[i].p = NULL;
#endif
    arp_table[i].ctime = 0;
  }
}
/**
 * Clears expired entries in the ARP table.
 *
 * This function should be called every ETHARP_TMR_INTERVAL microseconds (10 seconds),
 * in order to expire entries in the ARP table.
 */
void etharp_tmr ( void ) {

 s8_t i;

 for ( i = 0; i < ARP_TABLE_SIZE; ++i ) {
  ++arp_table[ i ].ctime;
  if (  arp_table[ i ].state == ETHARP_STATE_STABLE && arp_table[ i ].ctime >= ARP_MAXAGE ) {
   goto empty;
  } else if ( arp_table[ i ].state == ETHARP_STATE_PENDING && arp_table[ i ].ctime >= ARP_MAXPENDING ) {
empty:
   arp_table[ i ].state = ETHARP_STATE_EMPTY;
#if ARP_QUEUEING
   if ( arp_table[ i ].p ) {
    pbuf_free ( arp_table[ i ].p );
    arp_table[ i ].p = NULL;
   }  /* end if */
#endif
  }  /* end if */
 }  /* end for */
}  /* end etharp_tmr */
/**
 * Return an empty ARP entry (possibly recycling the oldest stable entry).
 *
 * @return The ARP entry index that is available, ERR_MEM if no usable
 * entry is found.
 */
static s8_t
find_arp_entry(void)
{
  s8_t i, j;
  u8_t maxtime = 0;

  j = ARP_TABLE_SIZE;
  /* search ARP table for an unused or old entry */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
  	/* empty entry? */
    if (arp_table[i].state == ETHARP_STATE_EMPTY) {
      LWIP_DEBUGF(ETHARP_DEBUG, ("find_arp_entry: returning empty entry %u\n", i));
      return i;
  	/* stable entry? */
    } else if (arp_table[i].state == ETHARP_STATE_STABLE) {
      /* remember entry with oldest stable entry in j */
      if (arp_table[i].ctime >= maxtime) maxtime = arp_table[j = i].ctime;
    }
  }
  /* no empty entry found? */
  if (i == ARP_TABLE_SIZE) {
  	LWIP_DEBUGF(ETHARP_DEBUG, ("find_arp_entry: found oldest stable entry %u\n", j));
    /* fall-back to oldest stable */
  	i = j;
  }
  /* no available entry found? */
  if (i == ARP_TABLE_SIZE) {
    LWIP_DEBUGF(ETHARP_DEBUG, ("find_arp_entry: no replacable entry could be found\n"));
    /* return failure */
    return ERR_MEM;
  }

  /* clean up the recycled stable entry */
  if (arp_table[i].state == ETHARP_STATE_STABLE) {
#if ARP_QUEUEING
    /* free packets on queue */
    etharp_dequeue(i);
#endif
    LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("find_arp_entry: recycling oldest stable entry %u\n", i));
    arp_table[i].state = ETHARP_STATE_EMPTY;
    arp_table[i].ctime = 0;
  }
  LWIP_DEBUGF(ETHARP_DEBUG, ("find_arp_entry: returning %u\n", i));
  return i;
}

#if ARP_QUEUEING
/*
 * Enqueues a pbuf (chain) on an ARP entry.
 * 
 * Places the pbuf (chain) on the queue (if space allows). The
 * caller may safely free the pbuf (chain) afterwards, as the
 * pbufs will be referenced by the queue and copies are made of
 * pbufs referencing external payloads.
 * 
 * @ i the ARP entry index
 * @arg q the pbuf (chain) to be queued on the ARP entry
 * 
 * @return Returns the new head of queue of the ARP entry.
 * 
 */
static struct pbuf *
etharp_enqueue(s8_t i, struct pbuf *q)
{
  /* any pbuf to queue? */
  if (q != NULL) {
/* queue later packet over earliers? TODO: Implement multiple pbuf queue */
#if ARP_QUEUE_FIRST == 0
    /* remove any pbufs on queue */
    u8_t deq = etharp_dequeue(i);
    if (deq > 0) LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE | 3, ("etharp_query: dequeued %u pbufs from ARP entry %u. Should not occur.\n", deq, i));
#endif
    /* packet can be queued? TODO: Implement multiple pbuf queue */
    if (arp_table[i].p == NULL) {
      /* copy any PBUF_REF referenced payloads into PBUF_RAM */
      q = pbuf_take(q);
      /* add pbuf to queue */
      arp_table[i].p = q;
      /* pbuf (chain) now queued, increase the reference count */
      pbuf_ref(q);
      LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE | DBG_STATE, ("etharp_query: queued packet %p on ARP entry %u.\n", (void *)q, i));
    }
  }
  return arp_table[i].p;
}

/**
 * Dequeues any pbufs queued on an ARP entry
 * 
 * @return number of pbufs removed from the queue
 * 
 * TODO: decide what is a sensible return value?
 */
static u8_t
etharp_dequeue(s8_t i)
{
  /* queued packets on a stable entry (work in progress) */
  if (arp_table[i].p != NULL) {
  	/* queue no longer references pbuf */
  	pbuf_free(arp_table[i].p);
    arp_table[i].p = NULL;
    return 1;
  } else {
    return 0;
  }
}
#endif

/**
 * Update (or insert) a IP/MAC address pair in the ARP cache.
 *
 * If a pending entry is resolved, any queued packets will be sent
 * at this point.
 * 
 * @param ipaddr IP address of the inserted ARP entry.
 * @param ethaddr Ethernet address of the inserted ARP entry.
 * @param flags Defines behaviour:
 * - ARP_INSERT_FLAG Allows ARP to insert this as a new item. If not specified,
 * only existing ARP entries will be updated.
 *
 * @return pbuf If non-NULL, a packet that was queued on a pending entry.
 * You should sent it and must call pbuf_free() afterwards.
 *
 * @see pbuf_free()
 */
static struct pbuf *
update_arp_entry(struct netif *netif, struct ip_addr *ipaddr, struct eth_addr *ethaddr, u8_t flags)
{
  s8_t i, k;
  LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE | 3, ("update_arp_entry()\n"));
  LWIP_ASSERT("netif->hwaddr_len != 0", netif->hwaddr_len != 0);
  LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("update_arp_entry: %u.%u.%u.%u - %02x:%02x:%02x:%02x:%02x:%02x\n", ip4_addr1(ipaddr), ip4_addr2(ipaddr), ip4_addr3(ipaddr), ip4_addr4(ipaddr),
  ethaddr->addr[0], ethaddr->addr[1], ethaddr->addr[2], ethaddr->addr[3], ethaddr->addr[4], ethaddr->addr[5]));
  /* do not update for 0.0.0.0 addresses */
  if (ipaddr->addr == 0) {
    LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("update_arp_entry: will not add 0.0.0.0 to ARP cache\n"));
    return NULL;
  }
  /* Walk through the ARP mapping table and try to find an entry to
  update. If none is found, the IP -> MAC address mapping is
  inserted in the ARP table. */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    /* Check if the source IP address of the incoming packet matches
    the IP address in this ARP table entry. */
    if (ip_addr_cmp(ipaddr, &arp_table[i].ipaddr)) {
      /* pending entry? */
      if (arp_table[i].state == ETHARP_STATE_PENDING) {
        LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("update_arp_entry: pending entry %u goes stable\n", i));
        /* A pending entry was found, mark it stable */
        arp_table[i].state = ETHARP_STATE_STABLE;
        /* fall-through to next if */
      }
      /* stable entry? (possibly just marked to become stable) */
      if (arp_table[i].state == ETHARP_STATE_STABLE) {
#if ARP_QUEUEING
        struct pbuf *p;
        struct eth_hdr *ethhdr;
#endif
        LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("update_arp_entry: updating stable entry %u\n", i));
        /* An old entry found, update this and return. */
        for (k = 0; k < netif->hwaddr_len; ++k) {
          arp_table[i].ethaddr.addr[k] = ethaddr->addr[k];
        }
        /* reset time stamp */
        arp_table[i].ctime = 0;
/* this is where we will send out queued packets! */
#if ARP_QUEUEING
        /* get the first packet on the queue (if any) */
        p = arp_table[i].p;
        /* queued packet present? */
        while (p != NULL) {
          struct pbuf *q, *n;
          /* search for second packet on queue (n) */
          q = p;
          while (q->tot_len > q->len) {
          	/* proceed to next pbuf of this packet */
          	LWIP_ASSERT("q->next ! NULL", q->next != NULL);
          	q = q->next;
          }
          /* { q = last pbuf of first packet, q->tot_len = q->len } */
          n = q->next;
          /* { n = first pbuf of 2nd packet, or NULL if no 2nd packet } */
          /* terminate the first packet pbuf chain */
          q->next = NULL;
          /* fill-in Ethernet header */
          ethhdr = p->payload;
          for (k = 0; k < netif->hwaddr_len; ++k) {
            ethhdr->dest.addr[k] = ethaddr->addr[k];
            ethhdr->src.addr[k] = netif->hwaddr[k];
          }
          ethhdr->type = ETHTYPE_IP;
          LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("update_arp_entry: sending queued IP packet %p.\n",(void *)p));
          /* send the queued IP packet */
          netif->linkoutput(netif, p);
          /* free the queued IP packet */
          pbuf_free(p);
          /* proceed to next packet on queue */
          p = n;
        }
        /* NULL attached buffer*/
        arp_table[i].p = NULL;
#endif
        return NULL;
      }
    } /* if */
  } /* for */

  /* no matching ARP entry was found */
  LWIP_ASSERT("update_arp_entry: i == ARP_TABLE_SIZE", i == ARP_TABLE_SIZE);

  LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("update_arp_entry: IP address not yet in table\n"));
  /* allowed to insert an entry? */
  if ((ETHARP_ALWAYS_INSERT) || (flags & ARP_INSERT_FLAG))
  {
    LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("update_arp_entry: adding entry to table\n"));
    /* find an empty or old entry. */
    i = find_arp_entry();
    if (i == ERR_MEM) {
      LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("update_arp_entry: no available entry found\n"));
      return NULL;
    }
    /* set IP address */
    ip_addr_set(&arp_table[i].ipaddr, ipaddr);
    /* set Ethernet hardware address */
    for (k = 0; k < netif->hwaddr_len; ++k) {
      arp_table[i].ethaddr.addr[k] = ethaddr->addr[k];
    }
    /* reset time-stamp */
    arp_table[i].ctime = 0;
    /* mark as stable */
    arp_table[i].state = ETHARP_STATE_STABLE;
    /* no queued packet */
#if ARP_QUEUEING
    arp_table[i].p = NULL;
#endif
  }
  else
  {
    LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("update_arp_entry: no matching stable entry to update\n"));
  }
  return NULL;
}

/**
 * Updates the ARP table using the given IP packet.
 *
 * Uses the incoming IP packet's source address to update the
 * ARP cache for the local network. The function does not alter
 * or free the packet. This function must be called before the
 * packet p is passed to the IP layer.
 *
 * @param netif The lwIP network interface on which the IP packet pbuf arrived.
 * @param pbuf The IP packet that arrived on netif.
 *
 * @return NULL
 *
 * @see pbuf_free()
 */
void
etharp_ip_input(struct netif *netif, struct pbuf *p) {
  struct ethip_hdr *hdr;

  /* Only insert an entry if the source IP address of the
     incoming IP packet comes from a host on the local network. */
  hdr = p->payload;
  /* source is on local network? */
  if (ip_addr_maskcmp(&(hdr->ip.src), &(netif->ip_addr), &(netif->netmask)))
   update_arp_entry(netif, &(hdr->ip.src), &(hdr->eth.src), ARP_INSERT_FLAG);
}

/**
 * Responds to ARP requests to us. Upon ARP replies to us, add entry to cache  
 * send out queued IP packets. Updates cache with snooped address pairs.
 *
 * Should be called for incoming ARP packets. The pbuf in the argument
 * is freed by this function.
 *
 * @param netif The lwIP network interface on which the ARP packet pbuf arrived.
 * @param pbuf The ARP packet that arrived on netif. Is freed by this function.
 * @param ethaddr Ethernet address of netif.
 *
 * @return NULL
 *
 * @see pbuf_free()
 */
void etharp_arp_input(struct netif *netif, struct eth_addr *ethaddr, struct pbuf *p)
{
  struct etharp_hdr *hdr;
  u8_t i;
  u8_t for_us;

  /* drop short ARP packets */
  if (p->tot_len < sizeof(struct etharp_hdr)) return;

  hdr = p->payload;
 
  /* this interface is not configured? */
  if (netif->ip_addr.addr == 0) {
    for_us = 0;
  } else {
    /* ARP packet directed to us? */
    for_us = ip_addr_cmp(&(hdr->dipaddr), &(netif->ip_addr));
  }

  /* add or update entries in the ARP cache */
  if (for_us) {
    /* insert IP address in ARP cache (assume requester wants to talk to us)
     * we might even send out a queued packet to this host */
    update_arp_entry(netif, &(hdr->sipaddr), &(hdr->shwaddr), ARP_INSERT_FLAG);
  /* request was not directed to us, but snoop anyway */
  } else {
    /* update the source IP address in the cache */
    update_arp_entry(netif, &(hdr->sipaddr), &(hdr->shwaddr), 0);
  }

  switch (hdr->opcode) {
  /* ARP request? */
  case ARP_REQUEST:
    /* ARP request. If it asked for our address, we send out a
    reply. In any case, we time-stamp any existing ARP entry,
    and possiby send out an IP packet that was queued on it. */

    LWIP_DEBUGF (ETHARP_DEBUG | DBG_TRACE, ("etharp_arp_input: incoming ARP request\n"));
    /* we are not configured? */
    if (netif->ip_addr.addr == 0) return;
    /* ARP request for our address? */
    if (for_us) {

      LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("etharp_arp_input: replying to ARP request for our IP address\n"));
      /* re-use pbuf to send ARP reply */
      hdr->opcode = ARP_REPLY;

      ip_addr_set(&(hdr->dipaddr), &(hdr->sipaddr));
      ip_addr_set(&(hdr->sipaddr), &(netif->ip_addr));

      for(i = 0; i < netif->hwaddr_len; ++i) {
        hdr->dhwaddr.addr[i] = hdr->shwaddr.addr[i];
        hdr->shwaddr.addr[i] = ethaddr->addr[i];
        hdr->ethhdr.dest.addr[i] = hdr->dhwaddr.addr[i];
        hdr->ethhdr.src.addr[i] = ethaddr->addr[i];
      }

      hdr->hwtype = htons(HWTYPE_ETHERNET);
      ARPH_HWLEN_SET(hdr, netif->hwaddr_len);

      hdr->proto = ETHTYPE_IP;
      ARPH_PROTOLEN_SET(hdr, sizeof(struct ip_addr));

      hdr->ethhdr.type = ETHTYPE_ARP;
      /* return ARP reply */
      netif->linkoutput(netif, p);

    /* request was not directed to us */
    } else {
      LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("etharp_arp_input: incoming ARP request was not for us.\n"));
    }
    break;
  case ARP_REPLY:
    /* ARP reply. We insert or update the ARP table later. */
    LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("etharp_arp_input: incoming ARP reply\n"));
#if (LWIP_DHCP && DHCP_DOES_ARP_CHECK)
    /* DHCP needs to know about ARP replies to our address */
    if (for_us) dhcp_arp_reply(netif, &hdr->sipaddr);
#endif
    break;
  default:
    LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("etharp_arp_input: ARP unknown opcode type %d\n", htons(hdr->opcode)));
    break;
  }
}

/**
 * Resolve and fill-in Ethernet address header for outgoing packet.
 *
 * If ARP has the Ethernet address in cache, the given packet is
 * returned, ready to be sent.
 *
 * If ARP does not have the Ethernet address in cache the packet is
 * queued (if enabled and space available) and a ARP request is sent.
 * This ARP request is returned as a pbuf, which should be sent by
 * the caller.
 *
 * A returned non-NULL packet should be sent by the caller.
 *
 * If ARP failed to allocate resources, NULL is returned.
 *
 * @param netif The lwIP network interface which the IP packet will be sent on.
 * @param ipaddr The IP address of the packet destination.
 * @param pbuf The pbuf(s) containing the IP packet to be sent.
 *
 * @return If non-NULL, a packet ready to be sent by caller.
 *
 */
struct pbuf *
etharp_output(struct netif *netif, struct ip_addr *ipaddr, struct pbuf *q)
{
  struct eth_addr *dest, *srcaddr, mcastaddr;
  struct eth_hdr *ethhdr;
  s8_t i;

  /* make room for Ethernet header */
  if (pbuf_header(q, sizeof(struct eth_hdr)) != 0) {
    /* The pbuf_header() call shouldn't fail, and we'll just bail
    out if it does.. */
    LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE | 2, ("etharp_output: could not allocate room for header.\n"));
    LINK_STATS_INC(link.lenerr);
    return NULL;
  }

  /* assume unresolved Ethernet address */
  dest = NULL;
  /* Construct Ethernet header. Start with looking up deciding which
     MAC address to use as a destination address. Broadcasts and
     multicasts are special, all other addresses are looked up in the
     ARP table. */

  /* destination IP address is an IP broadcast address? */
  if (ip_addr_isany(ipaddr) ||
    ip_addr_isbroadcast(ipaddr, &(netif->netmask))) {
    /* broadcast on Ethernet also */
    dest = (struct eth_addr *)&ethbroadcast;
  }
  /* destination IP address is an IP multicast address? */
  else if (ip_addr_ismulticast(ipaddr)) {
    /* Hash IP multicast address to MAC address. */
    mcastaddr.addr[0] = 0x01;
    mcastaddr.addr[1] = 0x00;
    mcastaddr.addr[2] = 0x5e;
    mcastaddr.addr[3] = ip4_addr2(ipaddr) & 0x7f;
    mcastaddr.addr[4] = ip4_addr3(ipaddr);
    mcastaddr.addr[5] = ip4_addr4(ipaddr);
    /* destination Ethernet address is multicast */
    dest = &mcastaddr;
  }
  /* destination IP address is an IP unicast address */
  else {
    /* destination IP network address not on local network?
     * IP layer wants us to forward to the default gateway */
    if (!ip_addr_maskcmp(ipaddr, &(netif->ip_addr), &(netif->netmask))) {
      /* interface has default gateway? */
      if (netif->gw.addr != 0)
      {
        /* route to default gateway IP address */
        ipaddr = &(netif->gw);
      }
      /* no gateway available? */
      else
      {
        /* IP destination address outside local network, but no gateway available */
        /* { packet is discarded } */
        return NULL;
      }
    }

    /* Ethernet address for IP destination address is in ARP cache? */
    for (i = 0; i < ARP_TABLE_SIZE; ++i) {
      /* match found? */
      if (arp_table[i].state == ETHARP_STATE_STABLE &&
        ip_addr_cmp(ipaddr, &arp_table[i].ipaddr)) {
        dest = &arp_table[i].ethaddr;
        break;
      }
    }
    /* could not find the destination Ethernet address in ARP cache? */
    if (dest == NULL) {
      /* ARP query for the IP address, submit this IP packet for queueing */
      /* TODO: How do we handle netif->ipaddr == ipaddr? */
      etharp_query(netif, ipaddr, q);
      /* { packet was queued (ERR_OK), or discarded } */
      /* return nothing */
      return NULL;
    }
    /* destination Ethernet address resolved from ARP cache */
    else
    {
      /* fallthrough */
    }
  }

  /* destination Ethernet address known */
  if (dest != NULL) {
    /* obtain source Ethernet address of the given interface */
    srcaddr = (struct eth_addr *)netif->hwaddr;

    /* A valid IP->MAC address mapping was found, fill in the
     * Ethernet header for the outgoing packet */
    ethhdr = q->payload;

    for(i = 0; i < netif->hwaddr_len; i++) {
      ethhdr->dest.addr[i] = dest->addr[i];
      ethhdr->src.addr[i] = srcaddr->addr[i];
    }

    ethhdr->type = ETHTYPE_IP;
    /* return the outgoing packet */
    return q;
  }
  /* never reached; here for safety */
  return NULL;
}

/**
 * Send an ARP request for the given IP address.
 *
 * Sends an ARP request for the given IP address, unless
 * a request for this address is already pending. Optionally
 * queues an outgoing packet on the resulting ARP entry.
 *
 * @param netif The lwIP network interface where ipaddr
 * must be queried for.
 * @param ipaddr The IP address to be resolved.
 * @param q If non-NULL, a pbuf that must be queued on the
 * ARP entry for the ipaddr IP address.
 *
 * @return NULL.
 *
 * @note Might be used in the future by manual IP configuration
 * as well.
 *
 * TODO: use the ctime field to see how long ago an ARP request was sent,
 * possibly retry.
 */
err_t etharp_query(struct netif *netif, struct ip_addr *ipaddr, struct pbuf *q)
{
  struct eth_addr *srcaddr;
  struct etharp_hdr *hdr;
  err_t result = ERR_OK;
  s8_t i;
  u8_t perform_arp_request = 1;
  /* prevent 'unused argument' warning if ARP_QUEUEING == 0 */
  (void)q;
  srcaddr = (struct eth_addr *)netif->hwaddr;
  /* bail out if this IP address is pending */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    if (ip_addr_cmp(ipaddr, &arp_table[i].ipaddr)) {
      if (arp_table[i].state == ETHARP_STATE_PENDING) {
        LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE | DBG_STATE, ("etharp_query: requested IP already pending as entry %u\n", i));
        /* break out of for-loop, user may wish to queue a packet on a pending entry */
        /* TODO: we will issue a new ARP request, which should not occur too often */
        /* we might want to run a faster timer on ARP to limit this */
        break;
      }
      else if (arp_table[i].state == ETHARP_STATE_STABLE) {
        LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE | DBG_STATE, ("etharp_query: requested IP already stable as entry %u\n", i));
        /* user may wish to queue a packet on a stable entry, so we proceed without ARP requesting */
        /* TODO: even if the ARP entry is stable, we might do an ARP request anyway */
        perform_arp_request = 0;
        break;
      }
    }
  }
  /* queried address not yet in ARP table? */
  if (i == ARP_TABLE_SIZE) {
    LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("etharp_query: IP address not found in ARP table\n"));
    /* find an available (unused or old) entry */
    i = find_arp_entry();
    /* bail out if no ARP entries are available */
    if (i == ERR_MEM) {
      LWIP_DEBUGF(ETHARP_DEBUG | 2, ("etharp_query: no more ARP entries available. Should seldom occur.\n"));
      return ERR_MEM;
    }
    /* i is available, create ARP entry */
    arp_table[i].state = ETHARP_STATE_PENDING;
    ip_addr_set(&arp_table[i].ipaddr, ipaddr);
  /* queried address was already in ARP table */
  } else {
#if ARP_QUEUEING
    etharp_enqueue(i, q);
#endif
  }
  /* ARP request? */
  if (perform_arp_request)
  {
    struct pbuf *p;
    /* allocate a pbuf for the outgoing ARP request packet */
    p = pbuf_alloc(PBUF_LINK, sizeof(struct etharp_hdr), PBUF_RAM);
    /* could allocate pbuf? */
    if (p != NULL) {
      u8_t j;
      LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE, ("etharp_query: sending ARP request.\n"));
      hdr = p->payload;
      hdr->opcode = ARP_REQUEST;
      for (j = 0; j < netif->hwaddr_len; ++j)
      {
        hdr->shwaddr.addr[j] = srcaddr->addr[j];
        /* the hardware address is what we ask for, in
         * a request it is a don't-care, we use 0's */
        hdr->dhwaddr.addr[j] = 0x00;
      }
      ip_addr_set(&(hdr->dipaddr), ipaddr);
      ip_addr_set(&(hdr->sipaddr), &(netif->ip_addr));

      hdr->hwtype = htons(HWTYPE_ETHERNET);
      ARPH_HWLEN_SET(hdr, netif->hwaddr_len);

      hdr->proto = ETHTYPE_IP;
      ARPH_PROTOLEN_SET(hdr, sizeof(struct ip_addr));
      for (j = 0; j < netif->hwaddr_len; ++j)
      {
        hdr->ethhdr.dest.addr[j] = 0xff;
        hdr->ethhdr.src.addr[j] = srcaddr->addr[j];
      }
      hdr->ethhdr.type = ETHTYPE_ARP;
      /* send ARP query */
      result = netif->linkoutput(netif, p);
      /* free ARP query packet */
      pbuf_free(p);
      p = NULL;
    } else {
      result = ERR_MEM;
      LWIP_DEBUGF(ETHARP_DEBUG | DBG_TRACE | 2, ("etharp_query: could not allocate pbuf for ARP request.\n"));
    }
  }
  return result;
}
