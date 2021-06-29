/**
 * @file
 *
 * Transmission Control Protocol, outgoing traffic
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


/* tcp_output.c
 *
 * The output functions of TCP.
 *
 */



#include "lwip/def.h"
#include "lwip/opt.h"

#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/sys.h"

#include "lwip/netif.h"

#include "lwip/inet.h"
#include "lwip/tcp.h"

#include "lwip/stats.h"

#include "sysclib.h"

#include "smsutils.h"

#if LWIP_TCP

/* Forward declarations.*/
static void tcp_output_segment(struct tcp_seg *seg, struct tcp_pcb *pcb);



err_t tcp_send_ctrl(struct tcp_pcb *pcb, u8_t flags)
{
    return tcp_enqueue(pcb, NULL, 0, flags, 1, NULL, 0);
}

err_t tcp_write(struct tcp_pcb *pcb, const void *arg, u16_t len, u8_t copy)
{
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG, ("tcp_write(pcb=%p, arg=%p, len=%u, copy=%d)\n", (void *)pcb,
                                   arg, len, (unsigned int)copy));
    if (pcb->state == SYN_SENT ||
        pcb->state == SYN_RCVD ||
        pcb->state == ESTABLISHED ||
        pcb->state == CLOSE_WAIT) {
        if (len > 0) {
            return tcp_enqueue(pcb, (void *)arg, len, 0, copy, NULL, 0);
        }
        return ERR_OK;
    } else {
        LWIP_DEBUGF(TCP_OUTPUT_DEBUG | DBG_STATE | 3, ("tcp_write() called in invalid state\n"));
        return ERR_CONN;
    }
}

err_t tcp_enqueue(
    struct tcp_pcb *pcb, void *arg, u16_t len,
    u8_t flags, u8_t copy,
    u8_t *optdata, u8_t optlen)
{

    struct pbuf *p;
    struct tcp_seg *seg, *useg, *queue;
    u32_t left, seqno;
    u16_t seglen;
    void *ptr;
    u8_t queuelen;
    flags_t lPCBFlags = pcb->flags;
    int iSegCNT = 0;

    left = len;
    ptr = arg;

    if (len > pcb->snd_buf)
        return ERR_MEM;

    seqno = pcb->snd_lbb;
    queue = NULL;
    queuelen = pcb->snd_queuelen;

    if (queuelen >= TCP_SND_QUEUELEN)
        goto memerr;

    seg = useg = NULL;
    seglen = 0;

    while (!queue || left > 0) {

        if (lPCBFlags & TF_EVENSEG) {

            ++iSegCNT;

            seglen = left > pcb->mss ? pcb->mss : (((iSegCNT % 2) == 1) ? ((left + 1) / 2) : left);
        } else
            seglen = left > pcb->mss ? pcb->mss : left;

        seg = memp_malloc(MEMP_TCP_SEG);

        if (!seg)
            goto memerr;

        seg->next = NULL;
        seg->p = NULL;

        if (!queue)
            useg = queue = seg;
        else {
            useg->next = seg;
            useg = seg;
        } /* end else */

        if (optdata != NULL) {
            if ((seg->p = pbuf_alloc(PBUF_TRANSPORT, optlen, PBUF_RAM)) == NULL) {
                goto memerr;
            }
            ++queuelen;
            seg->dataptr = seg->p->payload;
        } else if (copy) {
            if ((seg->p = pbuf_alloc(PBUF_TRANSPORT, seglen, PBUF_RAM)) == NULL) {
                LWIP_DEBUGF(TCP_OUTPUT_DEBUG | 2, ("tcp_enqueue : could not allocate memory for pbuf copy size %u\n", seglen));
                goto memerr;
            }
            ++queuelen;
            if (arg != NULL) {
                mips_memcpy(seg->p->payload, ptr, seglen);
            }
            seg->dataptr = seg->p->payload;
        }
        /* do not copy data */
        else {

            /* first, allocate a pbuf for holding the data.
             * since the referenced data is available at least until it is sent out on the
             * link (as it has to be ACKed by the remote party) we can safely use PBUF_ROM
             * instead of PBUF_REF here.
             */
            if ((p = pbuf_alloc(PBUF_TRANSPORT, seglen, PBUF_ROM)) == NULL) {
                LWIP_DEBUGF(TCP_OUTPUT_DEBUG | 2, ("tcp_enqueue: could not allocate memory for zero-copy pbuf\n"));
                goto memerr;
            }
            ++queuelen;
            p->payload = ptr;
            seg->dataptr = ptr;

            /* Second, allocate a pbuf for the headers. */
            if ((seg->p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_RAM)) == NULL) {
                /* If allocation fails, we have to deallocate the data pbuf as
                 * well. */
                pbuf_free(p);
                LWIP_DEBUGF(TCP_OUTPUT_DEBUG | 2, ("tcp_enqueue: could not allocate memory for header pbuf\n"));
                goto memerr;
            }
            ++queuelen;

            /* Concatenate the headers and data pbufs together. */
            pbuf_cat(seg->p, p);
            p = NULL;
        }

        /* Now that there are more segments queued, we check again if the
    length of the queue exceeds the configured maximum. */
        if (queuelen > TCP_SND_QUEUELEN) {
            LWIP_DEBUGF(TCP_OUTPUT_DEBUG | 2, ("tcp_enqueue: queue too long %u (%u)\n", queuelen, TCP_SND_QUEUELEN));
            goto memerr;
        }

        seg->len = seglen;

        if (pbuf_header(seg->p, TCP_HLEN)) {

            LWIP_DEBUGF(TCP_OUTPUT_DEBUG | 2, ("tcp_enqueue: no room for TCP header in pbuf.\n"));

            TCP_STATS_INC(tcp.err);
            goto memerr;
        }
        seg->tcphdr = seg->p->payload;
        seg->tcphdr->src = htons(pcb->local_port);
        seg->tcphdr->dest = htons(pcb->remote_port);
        seg->tcphdr->seqno = htonl(seqno);
        seg->tcphdr->urgp = 0;
        TCPH_FLAGS_SET(seg->tcphdr, flags);
        /* don't fill in tcphdr->ackno and tcphdr->wnd until later */

        /* Copy the options into the header, if they are present. */
        if (optdata == NULL) {
            TCPH_HDRLEN_SET(seg->tcphdr, 5);
        } else {
            TCPH_HDRLEN_SET(seg->tcphdr, (5 + optlen / 4));
            /* Copy options into data portion of segment.
       Options can thus only be sent in non data carrying
       segments such as SYN|ACK. */
            mips_memcpy(seg->dataptr, optdata, optlen);
        }

        left -= seglen;
        seqno += seglen;
        ptr = (void *)((char *)ptr + seglen);
    }


    /* Now that the data to be enqueued has been broken up into TCP
  segments in the queue variable, we add them to the end of the
  pcb->unsent queue. */
    if (pcb->unsent == NULL) {
        useg = NULL;
    } else {
        for (useg = pcb->unsent; useg->next != NULL; useg = useg->next)
            ;
    }

    /* If there is room in the last pbuf on the unsent queue,
  chain the first pbuf on the queue together with that. */
    if (useg != NULL &&
        TCP_TCPLEN(useg) != 0 &&
        !(TCPH_FLAGS(useg->tcphdr) & (TCP_SYN | TCP_FIN)) &&
        !(flags & (TCP_SYN | TCP_FIN)) &&
        useg->len + queue->len <= pcb->mss) {
        /* Remove TCP header from first segment. */
        pbuf_header(queue->p, -TCP_HLEN);
        pbuf_cat(useg->p, queue->p);
        useg->len += queue->len;
        useg->next = queue->next;

        if (seg == queue)
            seg = NULL;

        memp_free(MEMP_TCP_SEG, queue);
    } else {
        if (useg == NULL) {
            pcb->unsent = queue;

        } else {
            useg->next = queue;
        }
    }
    if ((flags & TCP_SYN) || (flags & TCP_FIN)) {
        ++len;
    }
    pcb->snd_lbb += len;
    pcb->snd_buf -= len;
    pcb->snd_queuelen = queuelen;
    LWIP_DEBUGF(TCP_QLEN_DEBUG, ("tcp_enqueue: %d (after enqueued)\n", pcb->snd_queuelen));
    if (pcb->snd_queuelen != 0) {
        LWIP_ASSERT("tcp_enqueue: valid queue length", pcb->unacked != NULL || pcb->unsent != NULL);
    }

    /* Set the PSH flag in the last segment that we enqueued, but only
  if the segment has data (indicated by seglen > 0). */
    if (seg != NULL && seglen > 0 && seg->tcphdr != NULL) {
        TCPH_SET_FLAG(seg->tcphdr, TCP_PSH);
    }

    return ERR_OK;
memerr:
    TCP_STATS_INC(tcp.memerr);

    if (queue != NULL) {
        tcp_segs_free(queue);
    }
    if (pcb->snd_queuelen != 0) {
        LWIP_ASSERT("tcp_enqueue: valid queue length", pcb->unacked != NULL || pcb->unsent != NULL);
    }
    LWIP_DEBUGF(TCP_QLEN_DEBUG | DBG_STATE, ("tcp_enqueue: %d (with mem err)\n", pcb->snd_queuelen));
    return ERR_MEM;
}

/* find out what we can send and send it */
err_t tcp_output(struct tcp_pcb *pcb)
{
    struct pbuf *p;
    struct tcp_hdr *tcphdr;
    struct tcp_seg *seg, *useg;
    u32_t wnd;
#if TCP_CWND_DEBUG
    int i = 0;
#endif /* TCP_CWND_DEBUG */

    /* First, check if we are invoked by the TCP input processing
     code. If so, we do not output anything. Instead, we rely on the
     input processing code to call us when input processing is done
     with. */
    if (tcp_input_pcb == pcb) {
        return ERR_OK;
    }

    wnd = LWIP_MIN(pcb->snd_wnd, pcb->cwnd);


    seg = pcb->unsent;

    /* useg should point to last segment on unacked queue */
    useg = pcb->unacked;
    if (useg != NULL) {
        for (; useg->next != NULL; useg = useg->next)
            ;
    }


    /* If the TF_ACK_NOW flag is set, we check if there is data that is
     to be sent. If data is to be sent out, we'll just piggyback our
     acknowledgement with the outgoing segment. If no data will be
     sent (either because the ->unsent queue is empty or because the
     window doesn't allow it) we'll have to construct an empty ACK
     segment and send it. */
    if (pcb->flags & TF_ACK_NOW &&
        (seg == NULL ||
         ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len > wnd)) {
        pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
        p = pbuf_alloc(PBUF_IP, TCP_HLEN, PBUF_RAM);
        if (p == NULL) {
            LWIP_DEBUGF(TCP_OUTPUT_DEBUG, ("tcp_output: (ACK) could not allocate pbuf\n"));
            return ERR_BUF;
        }
        LWIP_DEBUGF(TCP_OUTPUT_DEBUG, ("tcp_output: sending ACK for %lu\n", pcb->rcv_nxt));

        tcphdr = p->payload;
        tcphdr->src = htons(pcb->local_port);
        tcphdr->dest = htons(pcb->remote_port);
        tcphdr->seqno = htonl(pcb->snd_nxt);
        tcphdr->ackno = htonl(pcb->rcv_nxt);
        TCPH_FLAGS_SET(tcphdr, TCP_ACK);
        tcphdr->wnd = htons(pcb->rcv_wnd);
        tcphdr->urgp = 0;
        TCPH_HDRLEN_SET(tcphdr, 5);

        tcphdr->chksum = 0;
        tcphdr->chksum = inet_chksum_pseudo(p, &(pcb->local_ip), &(pcb->remote_ip),
                                            IP_PROTO_TCP, p->tot_len);


        ip_output(p, &(pcb->local_ip), &(pcb->remote_ip), pcb->ttl, pcb->tos,
                  IP_PROTO_TCP);
        pbuf_free(p);

        return ERR_OK;
    }

    while (seg != NULL &&
           ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len <= wnd) {

        pcb->unsent = seg->next;

        if (pcb->state != SYN_SENT) {
            TCPH_SET_FLAG(seg->tcphdr, TCP_ACK);
            pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
        }

        tcp_output_segment(seg, pcb);
        pcb->snd_nxt = ntohl(seg->tcphdr->seqno) + TCP_TCPLEN(seg);
        if (TCP_SEQ_LT(pcb->snd_max, pcb->snd_nxt)) {
            pcb->snd_max = pcb->snd_nxt;
        }
        /* put segment on unacknowledged list if length > 0 */
        if (TCP_TCPLEN(seg) > 0) {
            seg->next = NULL;
            if (pcb->unacked == NULL) {
                pcb->unacked = seg;
                useg = seg;
            } else {
                useg->next = seg;
                useg = useg->next;
            }
        } else {
            tcp_seg_free(seg);
        }
        seg = pcb->unsent;
    }
    return ERR_OK;
}


static void
tcp_output_segment(struct tcp_seg *seg, struct tcp_pcb *pcb)
{
    u16_t len;
    struct netif *netif;

    /* The TCP header has already been constructed, but the ackno and
   wnd fields remain. */
    seg->tcphdr->ackno = htonl(pcb->rcv_nxt);

    /* silly window avoidance */
    if (pcb->rcv_wnd < pcb->mss) {
        seg->tcphdr->wnd = 0;
    } else {
        /* advertise our receive window size in this TCP segment */
        seg->tcphdr->wnd = htons(pcb->rcv_wnd);
    }

    /* If we don't have a local IP address, we get one by
     calling ip_route(). */
    if (ip_addr_isany(&(pcb->local_ip))) {
        netif = ip_route(&(pcb->remote_ip));
        if (netif == NULL) {
            return;
        }
        ip_addr_set(&(pcb->local_ip), &(netif->ip_addr));
    }

    pcb->rtime = 0;

    if (pcb->rttest == 0) {
        pcb->rttest = tcp_ticks;
        pcb->rtseq = ntohl(seg->tcphdr->seqno);

        LWIP_DEBUGF(TCP_RTO_DEBUG, ("tcp_output_segment: rtseq %lu\n", pcb->rtseq));
    }
    LWIP_DEBUGF(TCP_OUTPUT_DEBUG, ("tcp_output_segment: %lu:%lu\n",
                                   htonl(seg->tcphdr->seqno), htonl(seg->tcphdr->seqno) + seg->len));

    len = (u16_t)((u8_t *)seg->tcphdr - (u8_t *)seg->p->payload);

    seg->p->len -= len;
    seg->p->tot_len -= len;

    seg->p->payload = seg->tcphdr;

    seg->tcphdr->chksum = 0;
    seg->tcphdr->chksum = inet_chksum_pseudo(seg->p,
                                             &(pcb->local_ip),
                                             &(pcb->remote_ip),
                                             IP_PROTO_TCP, seg->p->tot_len);
    TCP_STATS_INC(tcp.xmit);

    ip_output(seg->p, &(pcb->local_ip), &(pcb->remote_ip), pcb->ttl, pcb->tos,
              IP_PROTO_TCP);
}

void tcp_rst(u32_t seqno, u32_t ackno,
             struct ip_addr *local_ip, struct ip_addr *remote_ip,
             u16_t local_port, u16_t remote_port)
{
    struct pbuf *p;
    struct tcp_hdr *tcphdr;
    p = pbuf_alloc(PBUF_IP, TCP_HLEN, PBUF_RAM);
    if (p == NULL) {
        LWIP_DEBUGF(TCP_DEBUG, ("tcp_rst: could not allocate memory for pbuf\n"));
        return;
    }

    tcphdr = p->payload;
    tcphdr->src = htons(local_port);
    tcphdr->dest = htons(remote_port);
    tcphdr->seqno = htonl(seqno);
    tcphdr->ackno = htonl(ackno);
    TCPH_FLAGS_SET(tcphdr, TCP_RST | TCP_ACK);
    tcphdr->wnd = htons(TCP_WND);
    tcphdr->urgp = 0;
    TCPH_HDRLEN_SET(tcphdr, 5);

    tcphdr->chksum = 0;
    tcphdr->chksum = inet_chksum_pseudo(p, local_ip, remote_ip,
                                        IP_PROTO_TCP, p->tot_len);

    TCP_STATS_INC(tcp.xmit);
    /* Send output with hardcoded TTL since we have no access to the pcb */
    ip_output(p, local_ip, remote_ip, TCP_TTL, 0, IP_PROTO_TCP);
    pbuf_free(p);
    LWIP_DEBUGF(TCP_RST_DEBUG, ("tcp_rst: seqno %lu ackno %lu.\n", seqno, ackno));
}

void tcp_rexmit(struct tcp_pcb *pcb)
{
    struct tcp_seg *seg;

    if (pcb->unacked == NULL) {
        return;
    }

    /* Move all unacked segments to the unsent queue. */
    for (seg = pcb->unacked; seg->next != NULL; seg = seg->next)
        ;

    seg->next = pcb->unsent;
    pcb->unsent = pcb->unacked;

    pcb->unacked = NULL;


    pcb->snd_nxt = ntohl(pcb->unsent->tcphdr->seqno);

    ++pcb->nrtx;

    /* Don't take any rtt measurements after retransmitting. */
    pcb->rttest = 0;

    /* Do the actual retransmission. */
    tcp_output(pcb);
}

void tcp_keepalive(struct tcp_pcb *pcb)
{
    struct pbuf *p;
    struct tcp_hdr *tcphdr;

    LWIP_DEBUGF(TCP_DEBUG, ("tcp_keepalive: sending KEEPALIVE probe to %u.%u.%u.%u\n",
                            ip4_addr1(&pcb->remote_ip), ip4_addr2(&pcb->remote_ip),
                            ip4_addr3(&pcb->remote_ip), ip4_addr4(&pcb->remote_ip)));

    LWIP_DEBUGF(TCP_DEBUG, ("tcp_keepalive: tcp_ticks %ld   pcb->tmr %ld  pcb->keep_cnt %ld\n", tcp_ticks, pcb->tmr, pcb->keep_cnt));

    p = pbuf_alloc(PBUF_IP, TCP_HLEN, PBUF_RAM);

    if (p == NULL) {
        LWIP_DEBUGF(TCP_DEBUG, ("tcp_keepalive: could not allocate memory for pbuf\n"));
        return;
    }

    tcphdr = p->payload;
    tcphdr->src = htons(pcb->local_port);
    tcphdr->dest = htons(pcb->remote_port);
    tcphdr->seqno = htonl(pcb->snd_nxt - 1);
    tcphdr->ackno = htonl(pcb->rcv_nxt);
    tcphdr->wnd = htons(pcb->rcv_wnd);
    tcphdr->urgp = 0;
    TCPH_HDRLEN_SET(tcphdr, 5);

    tcphdr->chksum = 0;
    tcphdr->chksum = inet_chksum_pseudo(p, &pcb->local_ip, &pcb->remote_ip, IP_PROTO_TCP, p->tot_len);

    TCP_STATS_INC(tcp.xmit);

    /* Send output to IP */
    ip_output(p, &pcb->local_ip, &pcb->remote_ip, pcb->ttl, 0, IP_PROTO_TCP);

    pbuf_free(p);

    LWIP_DEBUGF(TCP_RST_DEBUG, ("tcp_keepalive: seqno %lu ackno %lu.\n", pcb->snd_nxt - 1, pcb->rcv_nxt));
}

#endif /* LWIP_TCP */
