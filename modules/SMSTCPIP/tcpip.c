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

#include "lwip/opt.h"

#include "lwip/sys.h"

#include "lwip/memp.h"
#include "lwip/pbuf.h"

#include "lwip/ip.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"

#include "lwip/tcpip.h"

static void (* tcpip_init_done)(void *arg) = NULL;
static void *tcpip_init_done_arg;
sys_mbox_t g_TCPIPMBox;

static void tcpip_thread ( void* arg ) {

 struct tcpip_msg* msg;

 ip_init  ();
#if LWIP_UDP
 udp_init ();
#endif  /* LWIP_UDP */
#if LWIP_TCP
 tcp_init ();
#endif  /* LWIP_TCP */
 tcpip_init_done ( tcpip_init_done_arg );

 while ( 1 ) {

  sys_mbox_fetch (  g_TCPIPMBox, ( void* )&msg  );

  switch ( msg -> type ) {

   case TCPIP_MSG_API:
    api_msg_input ( msg -> msg.apimsg );
   break;

   case TCPIP_MSG_INPUT:
    ip_input ( msg -> msg.inp.p, msg -> msg.inp.netif );
   break;

   case TCPIP_MSG_CALLBACK:
    msg -> msg.cb.f ( msg -> msg.cb.ctx );
   break;

  }  /* end switch */

  memp_free ( MEMP_TCPIP_MSG, msg );

 }  /* end while */

}  /* end tcpip_thread */

err_t tcpip_input ( struct pbuf* p, struct netif* inp ) {

 struct tcpip_msg *msg = memp_malloc ( MEMP_TCPIP_MSG );
  
 if ( msg ) {

  msg -> type          = TCPIP_MSG_INPUT;
  msg -> msg.inp.p     = p;
  msg -> msg.inp.netif = inp;
  sys_mbox_post ( g_TCPIPMBox, msg );

  return ERR_OK;

 } else {

  pbuf_free ( p );
  return ERR_MEM;  

 }  /* end else */

}  /* end tcpip_input */

void tcpip_apimsg ( struct api_msg* apimsg ) {

 struct tcpip_msg* msg = memp_malloc ( MEMP_TCPIP_MSG );

 if ( msg ) {

  msg -> type       = TCPIP_MSG_API;
  msg -> msg.apimsg = apimsg;
  sys_mbox_post ( g_TCPIPMBox, msg );

 } else memp_free ( MEMP_API_MSG, apimsg );

}  /* end tcpip_apimsg */

void tcpip_init (  void ( *initfunc ) ( void* ), void* arg  ) {

 tcpip_init_done     = initfunc;
 tcpip_init_done_arg = arg;

 g_TCPIPMBox = sys_mbox_new ();

 sys_thread_new ( tcpip_thread, NULL, TCPIP_THREAD_PRIO );

}  /* end tcpip_init */
