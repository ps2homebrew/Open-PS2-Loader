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
 * Author: Jani Monoses <jani@iv.ro>
 * original reassembly code by Adam Dunkels <adam@sics.se>
 *
 */


/* ip_frag.c
 *
 * This is the code for IP segmentation and reassembly
 *
 */


#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/ip.h"
#include "lwip/ip_frag.h"
#include "lwip/netif.h"

#include "lwip/stats.h"

#include <sysclib.h>
#include <thbase.h>

#include "smsutils.h"

/*
 * Copy len bytes from offset in pbuf to buffer
 *
 * helper used by both ip_reass and ip_frag
 */
static struct pbuf *copy_from_pbuf(
    struct pbuf *p, u16_t *offset, u8_t *buffer, u16_t len)
{

    u16_t l = 0;

    p->payload = (u8_t *)p->payload + *offset;
    p->len -= *offset;

    for (; len; p = p->next) {
        l = len < p->len ? len : p->len;
        mips_memcpy(buffer, p->payload, l);
        buffer += l;
        len -= l;
    } /* end while */

    *offset = l;

    return p;

} /* end copy_from_pbuf */

#define IP_REASS_BUFSIZE 5760
#define IP_REASS_MAXAGE  30
#define IP_REASS_TMO     250

static iop_sys_clock_t s_Clock = {
    0x01194000, 0x00000000};

static u8_t ip_reassbuf[IP_HLEN + IP_REASS_BUFSIZE];
static u8_t ip_reassbitmap[IP_REASS_BUFSIZE / (8 * 8)];

static const u8_t bitmap_bits[8] = {
    0xFF, 0x7F, 0x3F, 0x1F,
    0x0F, 0x07, 0x03, 0x01};

static u16_t ip_reasslen;
static u8_t ip_reassflags;

#define IP_REASS_FLAG_LASTFRAG 0x01

static u8_t ip_reasstmr;

static unsigned ip_reass_timer(void *apArg)
{

    iop_sys_clock_t *lpClock = (iop_sys_clock_t *)apArg;

    if (ip_reasstmr) {
        --ip_reasstmr;
        return lpClock->lo;
    } else
        return 0;

} /* end ip_reass_timer */

struct pbuf *
ip_reass(struct pbuf *p)
{
    struct pbuf *q;
    struct ip_hdr *fraghdr, *iphdr;
    u16_t offset, len;
    u16_t i;

    iphdr = (struct ip_hdr *)ip_reassbuf;
    fraghdr = (struct ip_hdr *)p->payload;
    /* If ip_reasstmr is zero, no packet is present in the buffer, so we
     write the IP header of the fragment into the reassembly
     buffer. The timer is updated with the maximum age. */
    if (ip_reasstmr == 0) {
        mips_memcpy(iphdr, fraghdr, IP_HLEN);
        ip_reasstmr = IP_REASS_MAXAGE;
        SetAlarm(&s_Clock, ip_reass_timer, &s_Clock);
        ip_reassflags = 0;
        /* Clear the bitmap. */
        mips_memset(ip_reassbitmap, 0, sizeof(ip_reassbitmap));
    }

    /* Check if the incoming fragment matches the one currently present
     in the reasembly buffer. If so, we proceed with copying the
     fragment into the buffer. */
    if (ip_addr_cmp(&iphdr->src, &fraghdr->src) &&
        ip_addr_cmp(&iphdr->dest, &fraghdr->dest) &&
        IPH_ID(iphdr) == IPH_ID(fraghdr)) {
        /* Find out the offset in the reassembly buffer where we should
       copy the fragment. */
        len = ntohs(IPH_LEN(fraghdr)) - IPH_HL(fraghdr) * 4;
        offset = (ntohs(IPH_OFFSET(fraghdr)) & IP_OFFMASK) * 8;

        /* If the offset or the offset + fragment length overflows the
       reassembly buffer, we discard the entire packet. */
        if (offset > IP_REASS_BUFSIZE || offset + len > IP_REASS_BUFSIZE) {
            CancelAlarm(ip_reass_timer, &s_Clock);
            ip_reasstmr = 0;
            goto nullreturn;
        }

        /* Copy the fragment into the reassembly buffer, at the right
       offset. */
        LWIP_DEBUGF(IP_REASS_DEBUG,
                    ("ip_reass: copying with offset %d into %d:%d\n", offset,
                     IP_HLEN + offset, IP_HLEN + offset + len));
        i = IPH_HL(fraghdr) * 4;
        copy_from_pbuf(p, &i, &ip_reassbuf[IP_HLEN + offset], len);

        /* Update the bitmap. */
        if (offset / (8 * 8) == (offset + len) / (8 * 8)) {
            /* If the two endpoints are in the same byte, we only update
         that byte. */
            ip_reassbitmap[offset / (8 * 8)] |=
                bitmap_bits[(offset / 8) & 7] &
                ~bitmap_bits[((offset + len) / 8) & 7];
        } else {
            /* If the two endpoints are in different bytes, we update the
         bytes in the endpoints and fill the stuff inbetween with
         0xff. */
            ip_reassbitmap[offset / (8 * 8)] |= bitmap_bits[(offset / 8) & 7];
            for (i = 1 + offset / (8 * 8); i < (offset + len) / (8 * 8); ++i) {
                ip_reassbitmap[i] = 0xff;
            }
            ip_reassbitmap[(offset + len) / (8 * 8)] |=
                ~bitmap_bits[((offset + len) / 8) & 7];
        }

        /* If this fragment has the More Fragments flag set to zero, we
       know that this is the last fragment, so we can calculate the
       size of the entire packet. We also set the
       IP_REASS_FLAG_LASTFRAG flag to indicate that we have received
       the final fragment. */

        if ((ntohs(IPH_OFFSET(fraghdr)) & IP_MF) == 0) {
            ip_reassflags |= IP_REASS_FLAG_LASTFRAG;
            ip_reasslen = offset + len;
        }

        /* Finally, we check if we have a full packet in the buffer. We do
       this by checking if we have the last fragment and if all bits
       in the bitmap are set. */
        if (ip_reassflags & IP_REASS_FLAG_LASTFRAG) {
            /* Check all bytes up to and including all but the last byte in
         the bitmap. */
            for (i = 0; i < ip_reasslen / (8 * 8) - 1; ++i) {
                if (ip_reassbitmap[i] != 0xff) {
                    goto nullreturn;
                }
            }
            /* Check the last byte in the bitmap. It should contain just the
         right amount of bits. */
            if (ip_reassbitmap[ip_reasslen / (8 * 8)] !=
                (u8_t)~bitmap_bits[ip_reasslen / 8 & 7]) {
                goto nullreturn;
            }

            /* Pretend to be a "normal" (i.e., not fragmented) IP packet
         from now on. */
            ip_reasslen += IP_HLEN;

            IPH_LEN_SET(iphdr, htons(ip_reasslen));
            IPH_OFFSET_SET(iphdr, 0);
            IPH_CHKSUM_SET(iphdr, 0);
            IPH_CHKSUM_SET(iphdr, inet_chksum(iphdr, IP_HLEN));

            /* If we have come this far, we have a full packet in the
         buffer, so we allocate a pbuf and copy the packet into it. We
         also reset the timer. */
            CancelAlarm(ip_reass_timer, &s_Clock);

            ip_reasstmr = 0;
            pbuf_free(p);
            p = pbuf_alloc(PBUF_LINK, ip_reasslen, PBUF_POOL);
            if (p != NULL) {
                i = 0;
                for (q = p; q != NULL; q = q->next) {
                    /* Copy enough bytes to fill this pbuf in the chain. The
       available data in the pbuf is given by the q->len
       variable. */
                    mips_memcpy(q->payload, &ip_reassbuf[i],
                                q->len > ip_reasslen - i ? ip_reasslen - i : q->len);
                    i += q->len;
                }
            }
            return p;
        }
    }
nullreturn:
    pbuf_free(p);
    return NULL;
}

#define MAX_MTU 1500
static u8_t buf[MEM_ALIGN_SIZE(MAX_MTU)];

/**
 * Fragment an IP packet if too large
 *
 * Chop the packet in mtu sized chunks and send them in order
 * by using a fixed size static memory buffer (PBUF_ROM)
 */
err_t ip_frag(struct pbuf *p, struct netif *netif, struct ip_addr *dest)
{
    struct pbuf *rambuf;
    struct pbuf *header;
    struct ip_hdr *iphdr;
    u16_t nfb = 0;
    u16_t left, cop;
    u16_t mtu = netif->mtu;
    u16_t ofo, omf;
    u16_t last;
    u16_t poff = IP_HLEN;
    u16_t tmp;

    /* Get a RAM based MTU sized pbuf */
    rambuf = pbuf_alloc(PBUF_LINK, 0, PBUF_REF);
    rambuf->tot_len = rambuf->len = mtu;
    rambuf->payload = MEM_ALIGN((void *)buf);


    /* Copy the IP header in it */
    iphdr = rambuf->payload;
    mips_memcpy(iphdr, p->payload, IP_HLEN);

    /* Save original offset */
    tmp = ntohs(IPH_OFFSET(iphdr));
    ofo = tmp & IP_OFFMASK;
    omf = tmp & IP_MF;

    left = p->tot_len - IP_HLEN;

    while (left) {
        last = (left <= mtu - IP_HLEN);

        /* Set new offset and MF flag */
        ofo += nfb;
        tmp = omf | (IP_OFFMASK & (ofo));
        if (!last)
            tmp = tmp | IP_MF;
        IPH_OFFSET_SET(iphdr, htons(tmp));

        /* Fill this fragment */
        nfb = (mtu - IP_HLEN) / 8;
        cop = last ? left : nfb * 8;

        p = copy_from_pbuf(p, &poff, (u8_t *)iphdr + IP_HLEN, cop);

        /* Correct header */
        IPH_LEN_SET(iphdr, htons(cop + IP_HLEN));
        IPH_CHKSUM_SET(iphdr, 0);
        IPH_CHKSUM_SET(iphdr, inet_chksum(iphdr, IP_HLEN));

        if (last)
            pbuf_realloc(rambuf, left + IP_HLEN);
        /* This part is ugly: we alloc a RAM based pbuf for
         * the link level header for each chunk and then
         * free it.A PBUF_ROM style pbuf for which pbuf_header
         * worked would make things simpler.
         */
        header = pbuf_alloc(PBUF_LINK, 0, PBUF_RAM);
        pbuf_chain(header, rambuf);
        netif->output(netif, header, dest);
        IPFRAG_STATS_INC(ip_frag.xmit);
        pbuf_free(header);

        left -= cop;
    }
    pbuf_free(rambuf);
    return ERR_OK;
}
