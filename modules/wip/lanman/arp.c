/*
  Copyright (c) 2010  jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Licenced under Academic Free License version 3.0
*/

#include <tamtypes.h>
#include <stdio.h>
#include <intrman.h>
#include <thbase.h>
#include <thsemap.h>
#include <errno.h>

#include "arp.h"
#include "smsutils.h"
#include "smap.h"
#include "inet.h"

static g_param_t *g_param;

// ARP

static int arp_mutex = -1;
static int arp_reply_flag;
static int wait_arp_reply = 0;

//-------------------------------------------------------------------------
void arp_init(g_param_t *gparam)
{
    g_param = gparam;

    arp_request();
}

//-------------------------------------------------------------------------
static void arp_output(u16 opcode, u8 *target_eth_addr)
{
    arp_pkt_t arp_pkt;

    mips_memset(&arp_pkt, 0, sizeof(arp_pkt_t));
    mips_memcpy(arp_pkt.eth.addr_dst, g_param->eth_addr_dst, 12);
    arp_pkt.eth.type = 0x0608; // Network byte order: 0x806

    arp_pkt.arp_hwtype = 0x0100;       // Network byte order: 0x01
    arp_pkt.arp_protocoltype = 0x0008; // Network byte order: 0x800
    arp_pkt.arp_hwsize = 6;
    arp_pkt.arp_protocolsize = 4;
    arp_pkt.arp_opcode = htons(opcode);
    mips_memcpy(arp_pkt.arp_sender_eth_addr, g_param->eth_addr_src, 6);
    mips_memcpy(&arp_pkt.arp_sender_ip_addr, &g_param->ip_addr_src, 4);
    mips_memcpy(arp_pkt.arp_target_eth_addr, target_eth_addr, 6);
    mips_memcpy(&arp_pkt.arp_target_ip_addr, &g_param->ip_addr_dst, 4);

    while (smap_xmit(&arp_pkt, sizeof(arp_pkt_t)) != 0)
        ;
}

//-------------------------------------------------------------------------
void arp_input(void *buf, int size) // !!! Intr Context !!!
{
    arp_pkt_t *arp_pkt = (arp_pkt_t *)buf;

    if (arp_pkt->arp_opcode == 0x0200) { // checking for ARP reply - network byte order

        if (wait_arp_reply) {
            mips_memcpy(g_param->eth_addr_dst, &arp_pkt->arp_sender_eth_addr[0], 6);

            arp_reply_flag = 1;
            iSignalSema(arp_mutex);
        }
    } else if (arp_pkt->arp_opcode == 0x0100) { // checking for ARP request - network byte order
        int i;
        for (i = 0; i < 4; i++) {
            u8 *p = (u8 *)&g_param->ip_addr_src;
            if (arp_pkt->arp_target_ip_addr.addr[i] != p[i])
                break;
        }
        if (i == 4)
            arp_output(ARP_REPLY, g_param->eth_addr_dst);
    }
}

//-------------------------------------------------------------------------
static unsigned int timer_intr_handler(void *args)
{
    iSignalSema(arp_mutex);

    return (unsigned int)args;
}

//-------------------------------------------------------------------------
void arp_request(void)
{
    iop_sys_clock_t sysclock;
    int oldstate;

    arp_mutex = CreateMutex(IOP_MUTEX_LOCKED);

    CpuSuspendIntr(&oldstate);
    wait_arp_reply = 1;
    CpuResumeIntr(oldstate);

send_arp_request:

    // Send an ARP resquest
    arp_output(ARP_REQUEST, "\x00\x00\x00\x00\x00\x00");

    USec2SysClock(3000 * 1000, &sysclock);
    SetAlarm(&sysclock, timer_intr_handler, NULL);

    // Wait for ARP reply or timeout
    WaitSema(arp_mutex);

    //while (QueryIntrContext())
    //	DelayThread(10000);

    CpuSuspendIntr(&oldstate);
    CancelAlarm(timer_intr_handler, NULL);
    if (arp_reply_flag == 0) { // It was a timeout ?
        CpuResumeIntr(oldstate);
        goto send_arp_request; // yes, retry...
    }
    wait_arp_reply = 0;
    arp_reply_flag = 0;
    CpuResumeIntr(oldstate);

    DeleteSema(arp_mutex);
    arp_mutex = -1;
}
