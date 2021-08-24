/****************************************************************/ /**
 *
 * @file nbd_server.c
 *
 * @author   Ronan Bignaux <ronan@aimao.org>
 *
 * @brief    Network Block Device Protocol server
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

/**
 * @defgroup nbd NBD server
 * @ingroup apps
 *
 * This is simple NBD server for the lwIP Socket API.
 */

#include "nbd_server.h"

/*
 * https://lwip.fandom.com/wiki/Receiving_data_with_LWIP
 */
int nbd_recv(int s, void *mem, size_t len, int flags)
{
    uint32_t bytesRead = 0;
    uint32_t left = len;
    uint32_t totalRead = 0;

    //        LWIP_DEBUGF(NBD_DEBUG | LWIP_DBG_STATE("nbd_recv(-, 0x%X, %d)\n", (int)mem, size);
    // dbgprintf("left = %u\n", left);
    do {
        bytesRead = recv(s, mem + totalRead, left, flags);
        // dbgprintf("bytesRead = %u\n", bytesRead);
        if (bytesRead <= 0)
            break;

        left -= bytesRead;
        totalRead += bytesRead;

    } while (left);
    return totalRead;
}

/** @ingroup nbd
 * Initialize NBD server.
 * @param ctx NBD callback struct
 */
int nbd_init(struct nbd_context **ctx)
{
    int tcp_socket, client_socket = -1;
    struct sockaddr_in peer;
    socklen_t addrlen;
    register int r;
    struct nbd_context *ctxt;

    peer.sin_family = AF_INET;
    peer.sin_port = htons(NBD_SERVER_PORT);
    peer.sin_addr.s_addr = htonl(INADDR_ANY);

    while (1) {

        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0)
            goto error;

        r = bind(tcp_socket, (struct sockaddr *)&peer, sizeof(peer));
        if (r < 0)
            goto error;

        r = listen(tcp_socket, 3);
        if (r < 0)
            goto error;

        while (1) {

            printf("lwNBD: a new client connected.\n");

            addrlen = sizeof(peer);
            client_socket = accept(tcp_socket, (struct sockaddr *)&peer,
                                   &addrlen);
            if (client_socket < 0)
                goto error;

            ctxt = negotiation_phase(client_socket, ctx);
            if (ctxt != NULL)
                r = transmission_phase(client_socket, ctxt);
            else
                printf("lwNBD: handshake failed.\n");

            if (r != NBD_SUCCESS)
                printf("lwNBD: an error occured during transmission phase.\n");

            closesocket(client_socket);
        }

    error:
        printf("lwNBD: failed to init server.");
        closesocket(tcp_socket);
    }
}
