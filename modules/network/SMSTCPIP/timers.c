/**
 * @file
 * Stack-internal timers implementation.
 * This file includes timer callbacks for stack-internal timers as well as
 * functions to set up or stop timers and check for expired timers.
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
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
 *         Simon Goldschmidt
 *
 */

#include <thsemap.h>

#include "lwip/opt.h"

#include "lwip/timers.h"
#include "lwip/tcp.h"

#if LWIP_TIMERS

#include "lwip/def.h"
#include "lwip/memp.h"
#include "lwip/tcpip.h"

#include "lwip/ip_frag.h"
#include "netif/etharp.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"

/** The one and only timeout list */
static struct sys_timeo *next_timeout;

#if LWIP_TCP
/** global variable that shows if the tcp timer is currently scheduled or not */
static int tcpip_tcp_timer_active;

/**
 * Timer callback function that calls tcp_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
tcpip_tcp_timer(void *arg)
{
    /* call TCP timer handler */
    tcp_tmr();
    /* timer still needed? */
    if (tcp_active_pcbs || tcp_tw_pcbs) {
        /* restart timer */
        sys_timeout(TCP_TMR_INTERVAL, tcpip_tcp_timer, NULL);
    } else {
        /* disable timer */
        tcpip_tcp_timer_active = 0;
    }
}

/**
 * Called from TCP_REG when registering a new PCB:
 * the reason is to have the TCP timer only running when
 * there are active (or time-wait) PCBs.
 */
void tcp_timer_needed(void)
{
    /* timer is off but needed again? */
    if (!tcpip_tcp_timer_active && (tcp_active_pcbs || tcp_tw_pcbs)) {
        /* enable and start timer */
        tcpip_tcp_timer_active = 1;
        sys_timeout(TCP_TMR_INTERVAL, tcpip_tcp_timer, NULL);
    }
}
#endif /* LWIP_TCP */

#if LWIP_ARP
/**
 * Timer callback function that calls etharp_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
arp_timer(void *arg)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_DEBUGF(TIMERS_DEBUG, ("tcpip: etharp_tmr()\n"));
    etharp_tmr();
    sys_timeout(ARP_TMR_INTERVAL, arp_timer, NULL);
}
#endif /* LWIP_ARP */

/** Initialize this module */
void sys_timeouts_init(void)
{
#if LWIP_ARP
    sys_timeout(ARP_TMR_INTERVAL, arp_timer, NULL);
#endif /* LWIP_ARP */
}

/**
 * Create a one-shot timer (aka timeout). Timeouts are processed in the
 * following cases:
 * - while waiting for a message using sys_timeouts_mbox_fetch()
 * - by calling sys_check_timeouts() (NO_SYS==1 only)
 *
 * @param msecs time in milliseconds after that the timer should expire
 * @param handler callback function to call when msecs have elapsed
 * @param arg argument to pass to the callback function
 */
#if LWIP_DEBUG_TIMERNAMES
void sys_timeout_debug(u32_t msecs, sys_timeout_handler handler, void *arg, const char *handler_name)
#else  /* LWIP_DEBUG_TIMERNAMES */
void sys_timeout(u32_t msecs, sys_timeout_handler handler, void *arg)
#endif /* LWIP_DEBUG_TIMERNAMES */
{
    struct sys_timeo *timeout, *t;

    timeout = (struct sys_timeo *)memp_malloc(MEMP_SYS_TIMEOUT);
    if (timeout == NULL) {
        LWIP_ASSERT("sys_timeout: timeout != NULL, pool MEMP_SYS_TIMEOUT is empty", timeout != NULL);
        return;
    }
    timeout->next = NULL;
    timeout->h = handler;
    timeout->arg = arg;
    timeout->time = msecs;
#if LWIP_DEBUG_TIMERNAMES
    timeout->handler_name = handler_name;
    LWIP_DEBUGF(TIMERS_DEBUG, ("sys_timeout: %p msecs=%" U32_F " handler=%s arg=%p\n",
                               (void *)timeout, msecs, handler_name, (void *)arg));
#endif /* LWIP_DEBUG_TIMERNAMES */

    if (next_timeout == NULL) {
        next_timeout = timeout;
        return;
    }

    if (next_timeout->time > msecs) {
        next_timeout->time -= msecs;
        timeout->next = next_timeout;
        next_timeout = timeout;
    } else {
        for (t = next_timeout; t != NULL; t = t->next) {
            timeout->time -= t->time;
            if (t->next == NULL || t->next->time > timeout->time) {
                if (t->next != NULL) {
                    t->next->time -= timeout->time;
                }
                timeout->next = t->next;
                t->next = timeout;
                break;
            }
        }
    }
}

/**
 * Go through timeout list (for this task only) and remove the first matching
 * entry, even though the timeout has not triggered yet.
 *
 * @note This function only works as expected if there is only one timeout
 * calling 'handler' in the list of timeouts.
 *
 * @param handler callback function that would be called by the timeout
 * @param arg callback argument that would be passed to handler
 */
void sys_untimeout(sys_timeout_handler handler, void *arg)
{
    struct sys_timeo *prev_t, *t;

    if (next_timeout == NULL) {
        return;
    }

    for (t = next_timeout, prev_t = NULL; t != NULL; prev_t = t, t = t->next) {
        if ((t->h == handler) && (t->arg == arg)) {
            /* We have a match */
            /* Unlink from previous in list */
            if (prev_t == NULL) {
                next_timeout = t->next;
            } else {
                prev_t->next = t->next;
            }
            /* If not the last one, add time of this one back to next */
            if (t->next != NULL) {
                t->next->time += t->time;
            }
            memp_free(MEMP_SYS_TIMEOUT, t);
            return;
        }
    }
    return;
}


/**
 * Wait (forever) for a message to arrive in an mbox.
 * While waiting, timeouts are processed.
 *
 * @param mbox the mbox to fetch the message from
 * @param msg the place to store the message
 */
void sys_timeouts_mbox_fetch(sys_mbox_t mbox, void **msg)
{
    u32_t time_needed;
    struct sys_timeo *tmptimeout;
    sys_timeout_handler handler;
    void *arg;

again:
    if (!next_timeout) {
        time_needed = sys_arch_mbox_fetch(mbox, msg, 0);
    } else {
        if (next_timeout->time > 0) {
            time_needed = sys_arch_mbox_fetch(mbox, msg, next_timeout->time);
        } else {
            time_needed = SYS_ARCH_TIMEOUT;
        }

        if (time_needed == SYS_ARCH_TIMEOUT) {
            /* If time == SYS_ARCH_TIMEOUT, a timeout occured before a message
         could be fetched. We should now call the timeout handler and
         deallocate the memory allocated for the timeout. */
            tmptimeout = next_timeout;
            next_timeout = tmptimeout->next;
            handler = tmptimeout->h;
            arg = tmptimeout->arg;
#if LWIP_DEBUG_TIMERNAMES
            if (handler != NULL) {
                LWIP_DEBUGF(TIMERS_DEBUG, ("stmf calling h=%s arg=%p\n",
                                           tmptimeout->handler_name, arg));
            }
#endif /* LWIP_DEBUG_TIMERNAMES */
            memp_free(MEMP_SYS_TIMEOUT, tmptimeout);
            if (handler != NULL) {
                /* For LWIP_TCPIP_CORE_LOCKING, lock the core before calling the
           timeout handler function. */
                LOCK_TCPIP_CORE();
                handler(arg);
                UNLOCK_TCPIP_CORE();
            }

            /* We try again to fetch a message from the mbox. */
            goto again;
        } else {
            /* If time != SYS_ARCH_TIMEOUT, a message was received before the timeout
         occured. The time variable is set to the number of
         milliseconds we waited for the message. */
            if (time_needed < next_timeout->time) {
                next_timeout->time -= time_needed;
            } else {
                next_timeout->time = 0;
            }
        }
    }
}

#else  /* LWIP_TIMERS */
/* Satisfy the TCP code which calls this function */
void tcp_timer_needed(void)
{
}
#endif /* LWIP_TIMERS */
