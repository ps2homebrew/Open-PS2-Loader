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

#include <stdio.h>
#include "nbd-protocol.h"
#include "nbd_opts.h"

#define LOG(format, args...) \
    printf(APP_NAME ": " format, ##args)

#ifdef DEBUG
#define DEBUGLOG LOG
#else
#define DEBUGLOG(args...) \
    do {                  \
    } while (0)
#endif

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
    uint16_t blocksize;
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

//********************* nbd_protocol.c *********************
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
