/****************************************************************/ /**
 *
 * @file lwnbd.h
 *
 * @author   Ronan Bignaux <ronan@aimao.org>
 *
 * @brief    Network Block Device Protocol implementation options
 *
 * Copyright (c) Ronan Bignaux. 2021
 * All rights reserved.
 *
 ********************************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification,are permitted provided that the following conditions are met:
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
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Ronan Bignaux <ronan@aimao.org>
 *
 */

#ifndef LWIP_HDR_APPS_NBD_SERVER_H
#define LWIP_HDR_APPS_NBD_SERVER_H

#include "nbd-protocol.h"
#include "nbd_opts.h"

#include <stdint.h>
#include <stdio.h>

#define LOG(format, args...) \
    printf(APP_NAME ": " format, ##args)

#ifdef DEBUG
#define DEBUGLOG LOG
#else
#define DEBUGLOG(args...) \
    do {                  \
    } while (0)
#endif

//#include "lwip/apps/nbd_opts.h"
//#include "lwip/err.h"
//#include "lwip/pbuf.h"
//#include "lwip/mem.h"

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <endian.h>
#include <unistd.h>

#include <assert.h> //todo: move in .c

typedef signed char err_t;
//TODO : manage endianess
#define htonll(x) htobe64(x)
#define ntohll(x) be64toh(x)
#endif

#ifdef _IOP
#include <ps2ip.h>
#include <sysclib.h>

#define assert(expr) \
    ((expr) ||       \
     dbgprintf(F_NUM, __LINE__))

//#include <errno.h>
//#include <malloc.h>

//why not provide lwip/sockets.h ?
// #define send(a, b, c, d) lwip_send(a, b, c, d)
#define close(x) lwip_close(x)

//TODO: Missing <byteswap.h> in PS2SDK
// pickup from https://gist.github.com/jtbr/7a43e6281e6cca353b33ee501421860c
static inline uint64_t bswap64(uint64_t x)
{
    return (((x & 0xff00000000000000ull) >> 56) | ((x & 0x00ff000000000000ull) >> 40) | ((x & 0x0000ff0000000000ull) >> 24) | ((x & 0x000000ff00000000ull) >> 8) | ((x & 0x00000000ff000000ull) << 8) | ((x & 0x0000000000ff0000ull) << 24) | ((x & 0x000000000000ff00ull) << 40) | ((x & 0x00000000000000ffull) << 56));
}

//TODO: Missing in PS2SK's "common/include/tcpip.h"
#if __BIG_ENDIAN__
#define htonll(x) (x)
#define ntohll(x) (x)
#else
#define htonll(x) bswap64(x)
#define ntohll(x) bswap64(x)
#endif

// Missing in stddef.h
#ifndef offsetof
#define offsetof(st, m) \
    ((size_t)((char *)&((st *)0)->m - (char *)0))
#endif

//TODO: Missing in PS2SK's <stdint.h> , needed for "nbd-protocol.h"
// https://en.cppreference.com/w/c/types/integer
#define UINT64_MAX  0xffffffffffffffff
#define UINT64_C(x) ((x) + (UINT64_MAX - UINT64_MAX))
#endif /* _IOP */

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup nbd
 * NBD context containing callback functions for NBD transfers
 * https://github.com/QuantumLeaps/OOP-in-C/blob/master/AN_OOP_in_C.pdf
 */

struct lwnbd_operations;

typedef struct nbd_context
{
    struct lwnbd_operations const *vptr;

    char export_desc[64];
    char export_name[32];
    uint64_t export_size; /* size of export in byte */
    uint16_t eflags;      /* per-export flags */
    uint16_t blocksize;   /* in power of 2 for bit shifting - log2(blocksize) */
    uint8_t *buffer;

} nbd_context;

struct lwnbd_operations
{

    /**
   * Close block device handle
   * @param handle File handle returned by open()
   */
    //  void (*close)(nbd_context *me);
    /**
   * Read from block device
   * @param
   * @param buffer Target buffer to copy read data to
   * @param offset Offset in block to copy read data to
   * @param length Number of blocks to copy to buffer
   * @returns &gt;= 0: Success; &lt; 0: Error
   */
    int (*read)(nbd_context const *const me, void *buffer, uint64_t offset, uint32_t length);
    /**
   * Write to block device
   * @param me ()
   * @param buffer Target buffer to copy write data to
   * @param offset Offset in block to copy write data to
   * @param length Number of blocks to copy to buffer
   * @returns &gt;= 0: Success; &lt; 0: Error
   */
    int (*write)(nbd_context const *const me, void *buffer, uint64_t offset, uint32_t length);
    /**
   * Flush to block device
   * @param me ()
   * @returns &gt;= 0: Success; &lt; 0: Error
   */
    int (*flush)(nbd_context const *const me);
};

uint32_t nbd_recv(int s, void *mem, size_t len, int flags);
int nbd_init(nbd_context **ctx);


// in nbd_protocol.c
//todo: const ctxs
err_t negotiation_phase(const int client_socket, nbd_context **ctxs, nbd_context **ctx);
err_t transmission_phase(const int client_socket, nbd_context *ctx);

void nbd_context_ctor(nbd_context *const me);
static inline int nbd_read(nbd_context const *const me, void *buffer, uint64_t offset, uint32_t length)
{
    return (*me->vptr->read)(me, buffer, offset, length);
}

static inline int nbd_write(nbd_context const *const me, void *buffer, uint64_t offset, uint32_t length)
{
    return (*me->vptr->write)(me, buffer, offset, length);
}

static inline int nbd_flush(nbd_context const *const me)
{
    return (*me->vptr->flush)(me);
}

//Todo: make a real server object
extern char gdefaultexport[];
extern uint8_t nbd_buffer[];
nbd_context *nbd_context_getDefaultExportByName(nbd_context **nbd_contexts, const char *exportname);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_HDR_APPS_NBD_SERVER_H */
