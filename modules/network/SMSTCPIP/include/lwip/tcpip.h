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
#ifndef __LWIP_TCPIP_H__
#define __LWIP_TCPIP_H__

#include "lwip/api_msg.h"
#include "lwip/pbuf.h"
#include "lwip/timers.h"

#if LWIP_TCPIP_CORE_LOCKING
/** The global semaphore to lock the stack. */
extern sys_sem_t lock_tcpip_core;
/** Lock lwIP core mutex (needs @ref LWIP_TCPIP_CORE_LOCKING 1) */
#define LOCK_TCPIP_CORE()   sys_sem_wait(lock_tcpip_core)
/** Unlock lwIP core mutex (needs @ref LWIP_TCPIP_CORE_LOCKING 1) */
#define UNLOCK_TCPIP_CORE() sys_sem_signal(lock_tcpip_core)
#define TCPIP_APIMSG_ACK(m)
#else /* LWIP_TCPIP_CORE_LOCKING */
#define LOCK_TCPIP_CORE()
#define UNLOCK_TCPIP_CORE()
#define TCPIP_APIMSG_ACK(m) sys_mbox_post(m, NULL)
#endif /* LWIP_TCPIP_CORE_LOCKING */

void tcpip_init(void (*tcpip_init_done)(void *), void *arg);
void tcpip_apimsg(struct api_msg *apimsg);
err_t tcpip_input(struct pbuf *p, struct netif *inp);
err_t tcpip_callback(void (*f)(void));

void tcpip_tcp_timer_needed(void);

enum tcpip_msg_type {
#if !LWIP_TCPIP_CORE_LOCKING
    TCPIP_MSG_API,
#endif
#if !LWIP_TCPIP_CORE_LOCKING_INPUT
    TCPIP_MSG_INPUT,
#endif
    TCPIP_MSG_CALLBACK
};

struct tcpip_msg
{
    enum tcpip_msg_type type;
    sys_sem_t *sem;
    union
    {
        struct api_msg *apimsg;
        struct
        {
            struct pbuf *p;
            struct netif *netif;
        } inp;
        struct
        {
            void (*f)(void *ctx);
            void *ctx;
        } cb;
    } msg;
};


#endif /* __LWIP_TCPIP_H__ */
