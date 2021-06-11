/****************************************************************/ /**
 *
 * @file nbd_server.h
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

//#include "lwip/apps/nbd_opts.h"
//#include "lwip/err.h"
//#include "lwip/pbuf.h"
//#include "lwip/mem.h"

#ifdef PS2SDK
#include <ps2ip.h>
#include <stdio.h>
#include <sysclib.h>
#include <stdint.h>
//#include <errno.h>
//#include <malloc.h>

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

//TODO: Missing in PS2SK's <stdint.h> , needed for "nbd-protocol.h"
// https://en.cppreference.com/w/c/types/integer
#define UINT64_MAX 0xffffffffffffffff
#define UINT64_C(x) ((x) + (UINT64_MAX - UINT64_MAX))
#endif

#ifdef __cplusplus
extern "C" {
#endif

//extern uint8_t buffer[];
extern uint8_t nbd_buffer[NBD_BUFFER_LEN] __attribute__((aligned(64)));

/** @ingroup nbd
 * NBD context containing callback functions for NBD transfers
 * https://github.com/QuantumLeaps/OOP-in-C/blob/master/AN_OOP_in_C.pdf
 */

struct nbd_context
{

    char export_name[32];
    char export_desc[64];
    uint64_t export_size; /* size of export in byte */
    uint16_t eflags;      /* per-export flags */
    uint8_t blockshift;   /* in power of 2 for bit shifting - log2(blocksize) */
    uint8_t *buffer;
    /**
   *  block device
   * @param
   * @returns
   */
    int (*export_init)(struct nbd_context *me);
    /**
   * Close block device handle
   * @param handle File handle returned by open()
   */
    //  void (*close)(struct nbd_context *me);
    /**
   * Read from block device
   * @param
   * @param buffer Target buffer to copy read data to
   * @param offset Offset in block to copy read data to
   * @param length Number of blocks to copy to buffer
   * @returns &gt;= 0: Success; &lt; 0: Error
   */
    int (*read)(struct nbd_context *me, void *buffer, uint64_t offset, uint32_t length);
    /**
   * Write to block device
   * @param me ()
   * @param buffer Target buffer to copy write data to
   * @param offset Offset in block to copy write data to
   * @param length Number of blocks to copy to buffer
   * @returns &gt;= 0: Success; &lt; 0: Error
   */
    int (*write)(struct nbd_context *me, void *buffer, uint64_t offset, uint32_t length);
    /**
   * Flush to block device
   * @param me ()
   * @returns &gt;= 0: Success; &lt; 0: Error
   */
    int (*flush)(struct nbd_context *me);
};

int nbd_init(struct nbd_context **ctx);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_HDR_APPS_NBD_SERVER_H */
