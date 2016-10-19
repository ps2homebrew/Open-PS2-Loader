/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#include <tamtypes.h>
#include <stdio.h>
#include <intrman.h>
#include <thbase.h>
#include <thsemap.h>

#include "tcp.h"
#include "lanman.h"
#include "smsutils.h"
#include "smap.h"
#include "inet.h"

static g_param_t *g_param;

static u8 tcp_sndbuf[1514] __attribute__((aligned(64))); // MTU
static u8 tcp_rcvbuf[4096] __attribute__((aligned(64))); // MTU
static void *p_rcptbuf;
static u16 rcptbuf_size;
static int tcp_mutex = -1;
static int tcp_listen = 0;

#define FIN 0x00
#define SYN 0x02
#define RST 0x04
#define PSH 0x08
#define ACK 0x10

static u8 TCP_FLAG;
static u32 seq_num;
static u32 next_seqnum;
static u32 ack_num;

#define TIMEOUT (3000 * 1000)
iop_sys_clock_t timeout_sysclock;
static int timeout_flag = 0;
static int tcp_reply_flag = 0;

static u16 tcp_port_src = 8209;

//-------------------------------------------------------------------------
static unsigned int timer_intr_handler(void *args)
{
    if (timeout_flag)
        iSignalSema(tcp_mutex);

    iSetAlarm(&timeout_sysclock, timer_intr_handler, NULL);

    return (unsigned int)args;
}

//-------------------------------------------------------------------------
void tcp_init(g_param_t *gparam)
{
    g_param = gparam;

    tcpip_pkt_t *tcp_pkt = (tcpip_pkt_t *)tcp_sndbuf;

    // Initialize the static elements of our TCP packet
    mips_memcpy(tcp_pkt->eth.addr_dst, g_param->eth_addr_dst, 12); // hardware MAC addresses
    tcp_pkt->eth.type = 0x0008;                                    // Network byte order: 0x800
    tcp_pkt->ip.hlen = 0x45;                                       // IPv4
    tcp_pkt->ip.tos = 0;
    tcp_pkt->ip.id = 0;
    tcp_pkt->ip.flags = 0x40; // Don't fragment
    tcp_pkt->ip.frag_offset = 0;
    tcp_pkt->ip.ttl = TTL;
    tcp_pkt->ip.proto = IP_PROTO_TCP;
    mips_memcpy(&tcp_pkt->ip.addr_src.addr, &g_param->ip_addr_src, 4);
    mips_memcpy(&tcp_pkt->ip.addr_dst.addr, &g_param->ip_addr_dst, 4);
    tcp_pkt->tcp_port_src = IP_PORT(tcp_port_src);
    tcp_pkt->tcp_port_dst = g_param->ip_port_dst;
    tcp_pkt->tcp_wndsize = htons(TCPWND_SIZE);

    // create a mutex
    tcp_mutex = CreateMutex(IOP_MUTEX_LOCKED);

    USec2SysClock(TIMEOUT, &timeout_sysclock);
    SetAlarm(&timeout_sysclock, timer_intr_handler, NULL);
}

//-------------------------------------------------------------------------
int tcp_output(void *buf, int size, int hdata_flag)
{
    tcpip_pkt_t *tcp_pkt;
    int pktsize;
    int oldstate;

    tcp_pkt = (tcpip_pkt_t *)tcp_sndbuf;
    pktsize = size + sizeof(tcpip_pkt_t);

    tcp_pkt->ip.len = htons(pktsize - 14); // Subtract the ethernet header

    tcp_pkt->ip.csum = 0;
    tcp_pkt->ip.csum = inet_chksum(&tcp_pkt->ip, 20); // Checksum the IP header (20 bytes)

    CpuSuspendIntr(&oldstate);
    u32 seqno = htona((u8 *)&seq_num);
    mips_memcpy(tcp_pkt->tcp_seq_num, &seqno, 4);
    u32 ackno = htona((u8 *)&ack_num);
    mips_memcpy(tcp_pkt->tcp_ack_num, &ackno, 4);
    CpuResumeIntr(oldstate);

    tcp_pkt->tcp_hdrlen = (hdata_flag ? 20 + size : 20) << 2;
    if (buf)
        mips_memcpy(&tcp_sndbuf[54], buf, size);

    tcp_pkt->tcp_flags = TCP_FLAG;
    tcp_pkt->tcp_csum = 0;
    tcp_pkt->tcp_csum = inet_chksum_pseudo(&tcp_sndbuf[34], &g_param->ip_addr_src, &g_param->ip_addr_dst, 20 + size);

    CpuSuspendIntr(&oldstate);
    next_seqnum = seq_num + (TCP_FLAG == SYN ? 1 : size);
    while (smap_xmit(tcp_pkt, pktsize) != 0)
        ;
    CpuResumeIntr(oldstate);

    return 1;
}

//-------------------------------------------------------------------------
int tcp_input(void *buf, int size) /// !!! Intr Context !!!
{
    register u32 ackno;

    if (tcp_listen) {
        tcpip_pkt_t *tcp_pkt = (tcpip_pkt_t *)buf;

        if (inet_chksum_pseudo(&tcp_pkt->tcp_port_src, &g_param->ip_addr_dst, &g_param->ip_addr_src, (ntohs(tcp_pkt->ip.len) - 20)) == 0) { // check TCP checksum
            if (tcp_pkt->tcp_flags & ACK) {

                ackno = ntoha(tcp_pkt->tcp_ack_num);
                if (ackno == next_seqnum) {
                    // packet is aknowledged
                    if (ntoha(tcp_pkt->tcp_seq_num) == ack_num - 1)
                        tcp_send_ack(); // keep alive ack
                    else {
                        int len = ntohs(tcp_pkt->ip.len) - 20 - (tcp_pkt->tcp_hdrlen >> 2);
                        seq_num = ackno;
                        ack_num = ntoha(tcp_pkt->tcp_seq_num) + len + (tcp_pkt->tcp_flags & SYN ? 1 : 0);
                        if ((len > 0) || (tcp_pkt->tcp_flags & SYN)) {
                            // do input buffering
                            if (tcp_pkt->tcp_flags == (ACK | PSH))
                                tcp_send_ack();
                            tcp_reply_flag = 1;
                            p_rcptbuf = buf;
                            rcptbuf_size = size;
                            iSignalSema(tcp_mutex);
                        }
                    }
                }
            }
        }
    }

    return 1;
}

//-------------------------------------------------------------------------
void tcp_send(void *buf, int size, int hdata_flag)
{
    int oldstate;
    tcpip_pkt_t *tcp_sndpkt = (tcpip_pkt_t *)tcp_sndbuf;

send:
    if (TCP_FLAG == SYN) {
        // select a port and increment it for retries
        tcp_sndpkt->tcp_port_src = IP_PORT(tcp_port_src);
        tcp_port_src++;
    }

    CpuSuspendIntr(&oldstate);
    timeout_flag = 1;
    tcp_listen = 1;
    CpuResumeIntr(oldstate);

    tcp_output(buf, size, hdata_flag);

    WaitSema(tcp_mutex);

    //while (QueryIntrContext())
    //	DelayThread(10000);

    CpuSuspendIntr(&oldstate);
    timeout_flag = 0;
    tcp_listen = 0;
    if (tcp_reply_flag == 0) { // It was a timeout ?
        CpuResumeIntr(oldstate);
        goto send; // yes, retry...
    }
    tcp_reply_flag = 0;
    mips_memcpy(tcp_rcvbuf, p_rcptbuf, rcptbuf_size);
    CpuResumeIntr(oldstate);
}

//-------------------------------------------------------------------------
void tcp_connect(void)
{
    iop_sys_clock_t sysclock;
    u8 options[4];

    TCP_FLAG = SYN;

    // generate ISN
    GetSystemTime(&sysclock);
    seq_num = sysclock.lo & 0xffff;
    ack_num = 0;

    // Options: MSS=1460
    *((u16 *)&options[0]) = 0x0402;
    *((u16 *)&options[2]) = htons(TCP_MSS);

    // send the SYN
    tcp_send(options, sizeof(options), 1);

    TCP_FLAG = ACK | PSH;

    tcp_send_ack();
}

//-------------------------------------------------------------------------
void tcp_send_ack(void)
{
    TCP_FLAG = ACK;
    tcp_output("\0", 0, 0);
    TCP_FLAG = ACK | PSH;
}
