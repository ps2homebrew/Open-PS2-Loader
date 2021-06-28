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

/* This is the part of the API that is linked with
   the application */

#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/api_msg.h"
#include "lwip/tcpip.h"
#include "lwip/memp.h"

#include <intrman.h>
#include <thsemap.h>
#include <sysclib.h>

#include "smsutils.h"

struct netbuf *netbuf_new(void)
{
    struct netbuf *buf = memp_malloc(MEMP_NETBUF);

    if (buf) {
        buf->p = NULL;
        buf->ptr = NULL;
    } /* end if */

    return buf;

} /* end netbuf_new */

void netbuf_delete(struct netbuf *buf)
{

    if (buf) {
        if (buf->p != NULL) {
            pbuf_free(buf->p);
            buf->p = buf->ptr = NULL;
        } /* end if */
        memp_free(MEMP_NETBUF, buf);
    } /* end if */

} /* end netbuf_delete */

err_t netbuf_ref(struct netbuf *buf, void *dataptr, u16_t size)
{

    if (buf->p)
        pbuf_free(buf->p);

    buf->p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF);
    if (buf->p == NULL) {
        buf->ptr = NULL;
        return ERR_MEM;
    }
    buf->p->payload = dataptr;
    buf->p->len = buf->p->tot_len = size;
    buf->ptr = buf->p;

    return ERR_OK;
} /* end netbuf_ref */

void netbuf_copy_partial(
    struct netbuf *buf, void *dataptr, u16_t len, u16_t offset)
{

    struct pbuf *p;
    u16_t left = 0;

    if (!buf || !dataptr)
        return;

    for (p = buf->p; left < len && p; p = p->next) {

        if (offset && offset >= p->len)

            offset -= p->len;

        else {

            int buf_copy_len = p->len - offset;
            int lDiff = len - left;

            lDiff = buf_copy_len > lDiff ? lDiff : buf_copy_len;

            mips_memcpy(
                &((char *)dataptr)[left],
                &((char *)p->payload)[offset],
                lDiff);
            left += lDiff;

            if (left >= len)
                return;

            offset = 0;

        } /* end else */

    } /* end for */

} /* end netbuf_copy_partial */

struct
    netconn *
    netconn_new_with_proto_and_callback(enum netconn_type t, u16_t proto,
                                        void (*callback)(struct netconn *, enum netconn_evt, u16_t len))
{
    struct netconn *conn;
    struct api_msg *msg;

    conn = memp_malloc(MEMP_NETCONN);
    if (conn == NULL) {
        return NULL;
    }

    conn->err = ERR_OK;
    conn->type = t;
    conn->pcb.tcp = NULL;

#if !LWIP_TCPIP_CORE_LOCKING
    if ((conn->mbox = sys_mbox_new()) == SYS_MBOX_NULL) {
        memp_free(MEMP_NETCONN, conn);
        return NULL;
    }
#endif
    conn->recvmbox = SYS_MBOX_NULL;
    conn->acceptmbox = SYS_MBOX_NULL;
    conn->sem = SYS_SEM_NULL;
    conn->state = NETCONN_NONE;
    conn->socket = 0;
    conn->callback = callback;
    conn->recv_avail = 0;

    if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
        memp_free(MEMP_NETCONN, conn);
        return NULL;
    }

    msg->type = API_MSG_NEWCONN;
    msg->msg.msg.bc.port = proto; /* misusing the port field */
    msg->msg.conn = conn;
    api_msg_post(msg);
    memp_free(MEMP_API_MSG, msg);

    if (conn->err != ERR_OK) {
        memp_free(MEMP_NETCONN, conn);
        return NULL;
    }

    return conn;
}

err_t netconn_delete(struct netconn *conn)
{
    struct api_msg *msg;
    void *mem;

    if (conn == NULL) {
        return ERR_OK;
    }

    if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
        return ERR_MEM;
    }

    msg->type = API_MSG_DELCONN;
    msg->msg.conn = conn;
    api_msg_post(msg);
    memp_free(MEMP_API_MSG, msg);

    /* Drain the recvmbox. */
    if (conn->recvmbox != SYS_MBOX_NULL) {
        while (sys_arch_mbox_fetch(conn->recvmbox, &mem, 1) != SYS_ARCH_TIMEOUT) {
            if (conn->type == NETCONN_TCP) {
                pbuf_free((struct pbuf *)mem);
            } else {
                netbuf_delete((struct netbuf *)mem);
            }
        }
        sys_mbox_free(conn->recvmbox);
        conn->recvmbox = SYS_MBOX_NULL;
    }

    /* Drain the acceptmbox. */
    if (conn->acceptmbox != SYS_MBOX_NULL) {
        while (sys_arch_mbox_fetch(conn->acceptmbox, &mem, 1) != SYS_ARCH_TIMEOUT) {
            netconn_delete((struct netconn *)mem);
        }

        sys_mbox_free(conn->acceptmbox);
        conn->acceptmbox = SYS_MBOX_NULL;
    }

#if !LWIP_TCPIP_CORE_LOCKING
    sys_mbox_free(conn->mbox);
    conn->mbox = SYS_MBOX_NULL;
#endif
    if (conn->sem != SYS_SEM_NULL) {
        sys_sem_free(conn->sem);
    }
    /*  conn->sem = SYS_SEM_NULL;*/
    memp_free(MEMP_NETCONN, conn);
    return ERR_OK;
}

err_t netconn_peer(struct netconn *conn, struct ip_addr *addr,
                   u16_t *port)
{
    switch (conn->type) {
        case NETCONN_RAW:
            /* return an error as connecting is only a helper for upper layers */
            return ERR_CONN;
        case NETCONN_UDPLITE:
        case NETCONN_UDPNOCHKSUM:
        case NETCONN_UDP:
            if (conn->pcb.udp == NULL ||
                ((conn->pcb.udp->flags & UDP_FLAGS_CONNECTED) == 0))
                return ERR_CONN;
            *addr = (conn->pcb.udp->remote_ip);
            *port = conn->pcb.udp->remote_port;
            break;
        case NETCONN_TCP:
            if (conn->pcb.tcp == NULL)
                return ERR_CONN;
            *addr = (conn->pcb.tcp->remote_ip);
            *port = conn->pcb.tcp->remote_port;
            break;
    }
    return (conn->err = ERR_OK);
}

err_t netconn_addr(struct netconn *conn, struct ip_addr **addr,
                   u16_t *port)
{
    switch (conn->type) {
        case NETCONN_RAW:
            *addr = &(conn->pcb.raw->local_ip);
            *port = conn->pcb.raw->protocol;
            break;
        case NETCONN_UDPLITE:
        case NETCONN_UDPNOCHKSUM:
        case NETCONN_UDP:
            *addr = &(conn->pcb.udp->local_ip);
            *port = conn->pcb.udp->local_port;
            break;
        case NETCONN_TCP:
            *addr = &(conn->pcb.tcp->local_ip);
            *port = conn->pcb.tcp->local_port;
            break;
    }
    return (conn->err = ERR_OK);
}

err_t netconn_bind(struct netconn *conn, struct ip_addr *addr,
                   u16_t port)
{
    struct api_msg *msg;

    if (conn == NULL) {
        return ERR_VAL;
    }

    if (conn->type != NETCONN_TCP &&
        conn->recvmbox == SYS_MBOX_NULL) {
        if ((conn->recvmbox = sys_mbox_new()) == SYS_MBOX_NULL) {
            return ERR_MEM;
        }
    }

    if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
        return (conn->err = ERR_MEM);
    }
    msg->type = API_MSG_BIND;
    msg->msg.conn = conn;
    msg->msg.msg.bc.ipaddr = addr;
    msg->msg.msg.bc.port = port;
    api_msg_post(msg);
    memp_free(MEMP_API_MSG, msg);
    return conn->err;
}


err_t netconn_connect(struct netconn *conn, struct ip_addr *addr,
                      u16_t port)
{
    struct api_msg *msg;

    if (conn == NULL) {
        return ERR_VAL;
    }


    if (conn->recvmbox == SYS_MBOX_NULL) {
        if ((conn->recvmbox = sys_mbox_new()) == SYS_MBOX_NULL) {
            return ERR_MEM;
        }
    }

    if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
        return ERR_MEM;
    }
    msg->type = API_MSG_CONNECT;
    msg->msg.conn = conn;
    msg->msg.msg.bc.ipaddr = addr;
    msg->msg.msg.bc.port = port;
    api_msg_post(msg);
    memp_free(MEMP_API_MSG, msg);
    return conn->err;
}

err_t netconn_disconnect(struct netconn *conn)
{
    struct api_msg *msg;

    if (conn == NULL) {
        return ERR_VAL;
    }

    if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
        return ERR_MEM;
    }
    msg->type = API_MSG_DISCONNECT;
    msg->msg.conn = conn;
    api_msg_post(msg);
    memp_free(MEMP_API_MSG, msg);
    return conn->err;
}

err_t netconn_listen(struct netconn *conn)
{
    struct api_msg *msg;

    if (conn == NULL) {
        return ERR_VAL;
    }

    if (conn->acceptmbox == SYS_MBOX_NULL) {
        conn->acceptmbox = sys_mbox_new();
        if (conn->acceptmbox == SYS_MBOX_NULL) {
            return ERR_MEM;
        }
    }

    if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
        return (conn->err = ERR_MEM);
    }
    msg->type = API_MSG_LISTEN;
    msg->msg.conn = conn;
    api_msg_post(msg);
    memp_free(MEMP_API_MSG, msg);
    return conn->err;
}

struct netconn *
netconn_accept(struct netconn *conn)
{
    struct netconn *newconn;

    if (conn == NULL) {
        return NULL;
    }

    sys_mbox_fetch(conn->acceptmbox, (void **)&newconn);
    /* Register event with callback */
    if (conn->callback)
        (*conn->callback)(conn, NETCONN_EVT_RCVMINUS, 0);

    return newconn;
}

struct netbuf *
netconn_recv(struct netconn *conn)
{
    struct api_msg *msg;
    struct netbuf *buf;
    struct pbuf *p;
    u16_t len;

    if (conn == NULL) {
        return NULL;
    }

    if (conn->recvmbox == SYS_MBOX_NULL) {
        conn->err = ERR_CONN;
        return NULL;
    }

    if (conn->err != ERR_OK) {
        return NULL;
    }

    if (conn->type == NETCONN_TCP) {
        if (conn->pcb.tcp->state == LISTEN) {
            conn->err = ERR_CONN;
            return NULL;
        }


        buf = memp_malloc(MEMP_NETBUF);

        if (buf == NULL) {
            conn->err = ERR_MEM;
            return NULL;
        }

        sys_mbox_fetch(conn->recvmbox, (void **)&p);

        if (p != NULL) {
            len = p->tot_len;
            SYS_ARCH_DEC(conn->recv_avail, len);
        } else
            len = 0;

        /* Register event with callback */
        if (conn->callback)
            (*conn->callback)(conn, NETCONN_EVT_RCVMINUS, len);

        /* If we are closed, we indicate that we no longer wish to receive
       data by setting conn->recvmbox to SYS_MBOX_NULL. */
        if (p == NULL) {
            memp_free(MEMP_NETBUF, buf);
            sys_mbox_free(conn->recvmbox);
            conn->recvmbox = SYS_MBOX_NULL;
            return NULL;
        }

        buf->p = p;
        buf->ptr = p;
        buf->fromport = 0;
        buf->fromaddr = NULL;

        /* Let the stack know that we have taken the data. */
        if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
            conn->err = ERR_MEM;
            return buf;
        }
        msg->type = API_MSG_RECV;
        msg->msg.conn = conn;
        if (buf != NULL) {
            msg->msg.msg.len = buf->p->tot_len;
        } else {
            msg->msg.msg.len = 1;
        }
        api_msg_post(msg);

        memp_free(MEMP_API_MSG, msg);
    } else {
        sys_mbox_fetch(conn->recvmbox, (void **)&buf);
        SYS_ARCH_DEC(conn->recv_avail, buf->p->tot_len);
        /* Register event with callback */
        if (conn->callback)
            (*conn->callback)(conn, NETCONN_EVT_RCVMINUS, buf->p->tot_len);
    }

    return buf;
}

err_t netconn_send(struct netconn *conn, struct netbuf *buf)
{
    struct api_msg *msg;

    if (conn == NULL) {
        return ERR_VAL;
    }

    if (conn->err != ERR_OK) {
        return conn->err;
    }

    if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
        return (conn->err = ERR_MEM);
    }
    msg->type = API_MSG_SEND;
    msg->msg.conn = conn;
    msg->msg.msg.p = buf->p;
    api_msg_post(msg);

    memp_free(MEMP_API_MSG, msg);
    return conn->err;
}


err_t netconn_write(struct netconn *conn, void *dataptr, u16_t size, u8_t copy)
{
    struct api_msg *msg;
    u16_t len, sndbuf;

    if (conn == NULL) {
        return ERR_VAL;
    }

    if (conn->err != ERR_OK) {
        return conn->err;
    }

    if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
        return (conn->err = ERR_MEM);
    }
    msg->type = API_MSG_WRITE;
    msg->msg.conn = conn;


    conn->state = NETCONN_WRITE;
    while (conn->err == ERR_OK && size > 0) {
        msg->msg.msg.w.dataptr = dataptr;
        msg->msg.msg.w.copy = copy;

        if (conn->type == NETCONN_TCP) {
            while ((sndbuf = tcp_sndbuf(conn->pcb.tcp)) == 0) {
                sys_sem_wait(conn->sem);
                if (conn->err != ERR_OK) {
                    goto ret;
                }
            }
            if (size > sndbuf) {
                /* We cannot send more than one send buffer's worth of data at a
     time. */
                len = sndbuf;
            } else {
                len = size;
            }
        } else {
            len = size;
        }

        LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_write: writing %d bytes (%d)\n", len, copy));
        msg->msg.msg.w.len = len;
        api_msg_post(msg);
        if (conn->err == ERR_OK) {
            dataptr = (void *)((u8_t *)dataptr + len);
            size -= len;
        } else if (conn->err == ERR_MEM) {
            conn->err = ERR_OK;
            sys_sem_wait(conn->sem);
        } else {
            goto ret;
        }
    }
ret:
    memp_free(MEMP_API_MSG, msg);
    conn->state = NETCONN_NONE;

    return conn->err;
}

err_t netconn_close(struct netconn *conn)
{
    struct api_msg *msg;

    if (conn == NULL) {
        return ERR_VAL;
    }
    if ((msg = memp_malloc(MEMP_API_MSG)) == NULL) {
        return (conn->err = ERR_MEM);
    }

    conn->state = NETCONN_CLOSE;
again:
    msg->type = API_MSG_CLOSE;
    msg->msg.conn = conn;
    api_msg_post(msg);
    if (conn->err == ERR_MEM &&
        conn->sem != SYS_SEM_NULL) {
        sys_sem_wait(conn->sem);
        goto again;
    }
    conn->state = NETCONN_NONE;
    memp_free(MEMP_API_MSG, msg);
    return conn->err;
}
